#include "build_executor.hpp"
#include "container_monitor.hpp"
#include "graph_builder.hpp"
#include "image_manager.hpp"
#include "instruction_types.hpp"
#include "layer_cache_manager.hpp"
#include "spec_generator.hpp"
#include "thread_pool.hpp"
#include "utils.hpp"
#include "mount.hpp"
#include "scoped_guard.hpp"
#include <algorithm>
#include <cpr/api.h>
#include <cpr/cprtypes.h>
#include <cpr/error.h>
#include <cpr/ssl_options.h>
#include <cpr/verbose.h>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <libcuckoo/cuckoohash_map.hh>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <sys/mount.h>
#include <unordered_set>
#include <blake3.h>
#include <format>
#include <random>
#include <sys/stat.h>
#include <future>

auto BuildExecutor::execute_instructions(const std::vector<std::vector<size_t>>& graph,
                std::vector<GraphBuilder::Stage>& stages,
                GraphBuilder::ParsedInstructions& parsed_instructions,
                const GraphBuilder::ParsedInstructionsMaps& parsed_instruction_maps,
                const std::vector<BuildFileParser::BuildInstruction>& instructions,
                const fs::path& build_dir,
                const std::string& target) -> void {
        m_build_dir = build_dir;
        m_image_manager = &ImageManager::get_instance();
        m_in_flight_cache_manager = &InFlightCacheManager::get_instance();
        m_layer_cache_manager = &LayerCacheManager::get_instance();
        m_container_monitor = &ContainerMonitor::get_instance();
        m_layer_cache_manager->init();
        if (target.empty()) {
                m_target_node = stages.back().node_number;
        }
        else {
                size_t node{};
                if (!parsed_instruction_maps.stage_alias_to_node_number.find(target, node)) [[unlikely]] {
                        throw std::runtime_error(std::format("Build Executor Error: Unknown target node '{}'", target));
                }
                m_target_node = node;
        }
        if (detect_cycles(graph)) [[unlikely]] {
                throw std::runtime_error("Build Executor Error: Cycle detected in dependency graph.");
        }
        prepare(stages, parsed_instructions, parsed_instruction_maps);
        auto layers{get_topological_order(graph)};
        for(const auto& layer : layers) {
                std::vector<std::future<void>> workgroup{};
                for (size_t i{}; i < layer.size(); ++i) {
                        size_t stage_index{layer[i] - 1};
                        workgroup.emplace_back(m_thread_pool.submit([&, stage_index](){
                                                exec_stage(stages[stage_index], parsed_instructions,
                                                                parsed_instruction_maps, instructions);
                                        }));
                }
                for (auto& worker : workgroup) {
                        worker.get();
                }
        }
}

[[nodiscard]] auto BuildExecutor::detect_cycles(const std::vector<std::vector<size_t>>& graph) -> bool {
        std::vector<size_t> color(graph.size(), 0);
        std::function<bool(size_t)> dfs{[&](size_t v) -> bool {
                color[v] = 1;
                for (const auto& u : graph[v]) {
                        if (color[u] == 0) {
                                if (dfs(u)) return true;
                        }
                        else if (color[u] == 1) return true;
                }
                color[v] = 2;
                return false;
        }};
        for (size_t i{1}; i < graph.size(); ++i) {
                if (color[i] == 0 && dfs(i)) {
                        return true;
                }
        }
        return false;
}

[[nodiscard]] auto BuildExecutor::get_topological_order(const std::vector<std::vector<size_t>>& graph) -> std::vector<std::vector<size_t>> {
        std::vector<std::vector<size_t>> layers{};
        std::vector<size_t> indegree(graph.size(), 0);
        std::queue<size_t> q{};
        for (size_t i{1}; i < graph.size(); ++i) {
                for (const auto& v : graph[i]) {
                        ++indegree[v];
                }
        }
        for (size_t i{1}; i < indegree.size(); ++i) {
                if (indegree[i] == 0) {
                        q.push(i);
                }
        }
        while (!q.empty()) {
                std::vector<size_t> layer{};
                while(!q.empty()) {
                        layer.emplace_back(q.front());
                        q.pop();
                }
                for (const auto& u : layer) {
                        for (const auto& v : graph[u]) {
                                if (--indegree[v] == 0) {
                                        q.push(v);
                                }
                        }
                }
                layers.emplace_back(std::move(layer));
        }
        return layers;
}

auto BuildExecutor::exec_stage(GraphBuilder::Stage& stage,
                const GraphBuilder::ParsedInstructions& parsed_instructions,
                const GraphBuilder::ParsedInstructionsMaps& parsed_instruction_maps,
                const std::vector<BuildFileParser::BuildInstruction>& instructions) -> void {
        std::string current_digest{};
        size_t node{};
                if (!parsed_instruction_maps.stage_alias_to_node_number.find(stage.base_image, node)) {
                if (!m_image_top_layer_digest.find(stage.base_image, current_digest)) [[unlikely]] {
                        throw std::runtime_error(std::format("Build Executor Error: Image '{}' not found.", stage.base_image));
                }
                m_stage_lower_dirs.insert(stage.node_number, std::vector{Utils::get_image_path(stage.base_image).string()});

                fs::path image_path = Utils::get_image_path(stage.base_image);
                std::string config_str = Utils::read_file(image_path / "config.json");
                std::string manifest_str = Utils::read_file(image_path / "manifest.json");
                nlohmann::json config_json = nlohmann::json::parse(config_str);
                nlohmann::json manifest_json = nlohmann::json::parse(manifest_str);

                std::vector<std::string> diff_ids;
                if (config_json.contains("rootfs") && config_json["rootfs"].contains("diff_ids")) {
                        for (const auto& diff_id : config_json["rootfs"]["diff_ids"]) {
                                diff_ids.push_back(diff_id.get<std::string>());
                        }
                }
                m_stage_diff_ids.insert(stage.node_number, diff_ids);

                std::vector<std::string> layers;
                std::vector<LayerCache> layer_caches;
                if (manifest_json.contains("layers")) {
                        for (const auto& layer : manifest_json["layers"]) {
                                std::string digest = layer["digest"].get<std::string>();
                                if (digest.starts_with("sha256:")) {
                                        digest = digest.substr(7);
                                }
                                layers.push_back(digest);
                                std::string diff_id = diff_ids.size() > layer_caches.size() ? diff_ids[layer_caches.size()] : digest;
                                int64_t size = layer["size"].get<int64_t>();
                                layer_caches.push_back(LayerCache{digest, diff_id, "", size});
                        }
                }
                m_stage_layers.insert(stage.node_number, layers);
                m_stage_layer_caches.insert(stage.node_number, layer_caches);
        }
        else {
                std::vector<std::string> lower_dirs{};
                if (!m_stage_final_digest.find(node, current_digest)) {
                        throw std::runtime_error("Build Executor Error: ");
                }
                if (!m_stage_lower_dirs.find(node, lower_dirs)) {
                        throw std::runtime_error("Build Executor Error: ");
                }
                m_stage_lower_dirs.insert(stage.node_number, lower_dirs);

                std::vector<std::string> layers;
                if (m_stage_layers.find(node, layers)) {
                        m_stage_layers.insert(stage.node_number, layers);
                } else {
                        m_stage_layers.insert(stage.node_number, std::vector{current_digest});
                }

                std::vector<std::string> diff_ids;
                if (m_stage_diff_ids.find(node, diff_ids)) {
                        m_stage_diff_ids.insert(stage.node_number, diff_ids);
                } else {
                        m_stage_diff_ids.insert(stage.node_number, std::vector<std::string>{});
                }

                std::vector<LayerCache> layer_caches;
                if (m_stage_layer_caches.find(node, layer_caches)) {
                        m_stage_layer_caches.insert(stage.node_number, layer_caches);
                } else {
                        m_stage_layer_caches.insert(stage.node_number, std::vector<LayerCache>{});
                }
        }
        for (const auto& instruction_index : stage.instruction_indices) {
                switch (instructions[instruction_index].type) {
                        case Instruction::InstructionType::ADD:
                                {
                                        size_t index{};
                                        parsed_instruction_maps.index_to_offset.find(instruction_index, index);
                                        exec_add(stage, parsed_instructions.add_instructions[index],
                                                        instructions[instruction_index].raw_instruction, current_digest);
                                        break;
                                }
                        case Instruction::InstructionType::COPY:
                                {
                                        size_t index{};
                                        parsed_instruction_maps.index_to_offset.find(instruction_index, index);
                                        exec_copy(stage, parsed_instructions.copy_instructions[index], parsed_instruction_maps,
                                                        instructions[instruction_index].raw_instruction, current_digest);
                                        break;
                                }
                        case Instruction::InstructionType::RUN:
                                {
                                        size_t index{};
                                        parsed_instruction_maps.index_to_offset.find(instruction_index, index);
                                        exec_run(stage, parsed_instructions.run_instructions[index], parsed_instruction_maps,
                                                        instructions[instruction_index].raw_instruction, current_digest);
                                        break;
                                }
                        case Instruction::InstructionType::SHELL:
                                {
                                        size_t index{};
                                        parsed_instruction_maps.index_to_offset.find(instruction_index, index);
                                        exec_shell(stage, parsed_instructions.shell_instructions[index]);
                                        break;
                                }
                        case Instruction::InstructionType::USER:
                                {
                                        size_t index{};
                                        parsed_instruction_maps.index_to_offset.find(instruction_index, index);
                                        exec_user(stage, parsed_instructions.user_instructions[index]);
                                        break;
                                }
                        case Instruction::InstructionType::WORKDIR:
                                {
                                        size_t index{};
                                        parsed_instruction_maps.index_to_offset.find(instruction_index, index);
                                        exec_workdir(stage, parsed_instructions.workdir_instructions[index]);
                                        break;
                                }
                        default:
                                break;
                }
        }
        if (stage.node_number == m_target_node) {
                assemble_oci_image(stage, "quiver_build");
        }
        m_stage_final_digest.insert(stage.node_number, current_digest);
}

auto BuildExecutor::exec_add(const GraphBuilder::Stage& stage, const Instruction::AddInstruction& instruction,
                const std::string& raw_instruction, std::string& current_digest) -> void {
        std::vector<fs::path> srcs{instruction.srcs};
        for (const auto& url : instruction.urls) {
                fs::path file_path{};
                m_url_downloaded_file.find(url, file_path);
                srcs.emplace_back(file_path);
        }
        std::sort(srcs.begin(), srcs.end());
        Instruction::InstructionHash instruction_hash{};
        instruction_hash.expanded_raw_ins = raw_instruction;
        instruction_hash.parent_digest = current_digest;
        instruction_hash.file_checksum = compute_files_checksum(srcs, m_build_dir);
        std::string hash{get_instruction_hash(instruction_hash)};
        LayerCache cache_obj{};
        if (m_hash_digest.find(hash, cache_obj)) {
                m_stage_layers.update_fn(stage.node_number,
                                [&](std::vector<std::string>& layers) {
                                        layers.emplace_back(cache_obj.hash);
                                });
                m_stage_diff_ids.update_fn(stage.node_number,
                                [&](std::vector<std::string>& diff_ids) {
                                        diff_ids.emplace_back(cache_obj.diff_id);
                                });
                m_stage_lower_dirs.update_fn(stage.node_number,
                                [&](std::vector<std::string>& lower_dirs) {
                                        lower_dirs.emplace_back(cache_obj.lower_dir);
                                });
                m_stage_layer_caches.update_fn(stage.node_number,
                                [&](std::vector<LayerCache>& caches) {
                                        caches.emplace_back(cache_obj);
                                });
                current_digest = cache_obj.hash;
                return;
        }
        auto obj{m_layer_cache_manager->lookup(hash)};
        if (obj.has_value()) {
                m_stage_layers.update_fn(stage.node_number,
                                [&](std::vector<std::string>& layers) {
                                        layers.emplace_back(obj->hash);
                                });
                m_stage_diff_ids.update_fn(stage.node_number,
                                [&](std::vector<std::string>& diff_ids) {
                                        diff_ids.emplace_back(obj->diff_id);
                                });
                m_stage_lower_dirs.update_fn(stage.node_number,
                                [&](std::vector<std::string>& lower_dirs) {
                                        lower_dirs.emplace_back(obj->lower_dir);
                                });
                m_stage_layer_caches.update_fn(stage.node_number,
                                [&](std::vector<LayerCache>& caches) {
                                        caches.emplace_back(*obj);
                                });
                m_hash_digest.insert(hash, *obj);
                current_digest = obj->hash;
                return;
        }
        auto status{m_in_flight_cache_manager->acquire(hash)};
        if (status.is_owner) {
                fs::path dummy_path{std::format("{}/layer_snapshots/quiver_layer_{}", Utils::get_base_dir().string(), hash)};
                fs::path final_dst{dummy_path / instruction.dst};
                Utils::ensure_dir(dummy_path);
                try {
                        if (srcs.size() == 1) {
                                fs::path path{srcs.front().is_absolute() ? srcs.front() : m_build_dir / srcs.front()};
                                if (fs::exists(path)) {
                                        if (fs::is_directory(path) || instruction.dst.string().ends_with('/')) {
                                                Utils::ensure_dir(final_dst);
                                                Utils::copy_directory(path, final_dst);
                                                std::vector<std::string> current_lower_dirs{};
                                                m_stage_lower_dirs.find(stage.node_number, current_lower_dirs);
                                                change_permission_and_owners(final_dst, instruction.chmod, instruction.chown, current_lower_dirs);
                                        }
                                        else {
                                                const auto parent{final_dst.parent_path()};
                                                Utils::ensure_dir(parent);
                                                Utils::copy_directory(path, parent);
                                                Utils::rename_file_or_directory(parent / srcs.front().filename(), parent / instruction.dst);
                                                std::vector<std::string> current_lower_dirs{};
                                                m_stage_lower_dirs.find(stage.node_number, current_lower_dirs);
                                                change_permission_and_owners(parent / instruction.dst, instruction.chmod, instruction.chown, current_lower_dirs);
                                        }
                                }
                                else {
                                        throw std::runtime_error(std::format("ADD failed: source '{}' does not exist.", srcs.front().string()));
                                }
                        }
                        else {
                                Utils::ensure_dir(final_dst);
                                for (const auto& src : srcs) {
                                        fs::path path{src.is_absolute() ? src : m_build_dir / src};
                                        if (fs::exists(path)) {
                                                Utils::copy_directory(path, final_dst);
                                        }
                                        else {
                                                throw std::runtime_error(std::format("ADD failed: source '{}' does not exist.", src.string()));
                                        }
                                }
                                std::vector<std::string> current_lower_dirs{};
                                m_stage_lower_dirs.find(stage.node_number, current_lower_dirs);
                                change_permission_and_owners(final_dst, instruction.chmod, instruction.chown, current_lower_dirs);
                        }
                        auto layer_info{Utils::create_oci_layer(dummy_path, Utils::get_layers_path(hash + ".tar.gz"))};
                        std::string sha256_hash{layer_info.blob_digest};
                        m_stage_layers.update_fn(stage.node_number,
                                        [&](std::vector<std::string>& layers) {
                                                layers.emplace_back(sha256_hash);
                                        });
                        m_stage_diff_ids.update_fn(stage.node_number,
                                        [&](std::vector<std::string>& diff_ids) {
                                                diff_ids.emplace_back(layer_info.diff_id);
                                        });
                        m_stage_lower_dirs.update_fn(stage.node_number,
                                        [&](std::vector<std::string>& lower_dir) {
                                                lower_dir.emplace_back(dummy_path.string());
                                        });
                        current_digest = sha256_hash;
                        LayerCache new_cache{sha256_hash, layer_info.diff_id, dummy_path.string(), static_cast<int64_t>(layer_info.blob_size)};
                        m_stage_layer_caches.update_fn(stage.node_number,
                                        [&](std::vector<LayerCache>& caches) {
                                                caches.emplace_back(new_cache);
                                        });
                        m_hash_digest.insert(hash, new_cache);
                        m_layer_cache_manager->store(hash, new_cache);
                        m_in_flight_cache_manager->finish_success(hash, std::make_pair(new_cache, dummy_path));
                }
                catch (...) {
                        Utils::remove_directory(dummy_path);
                        m_in_flight_cache_manager->finish_failure(hash, std::current_exception());
                        throw;
                }
        }
        else {
                const auto& layer_cache{status.build->future.get()};
                m_stage_layers.update_fn(stage.node_number,
                                [&](std::vector<std::string>& layers) {
                                        layers.emplace_back(layer_cache.first.hash);
                                });
                m_stage_diff_ids.update_fn(stage.node_number,
                                [&](std::vector<std::string>& diff_ids) {
                                        diff_ids.emplace_back(layer_cache.first.diff_id);
                                });
                m_stage_lower_dirs.update_fn(stage.node_number,
                                [&](std::vector<std::string>& lower_dir) {
                                        lower_dir.emplace_back(layer_cache.second.string());
                                });
                m_stage_layer_caches.update_fn(stage.node_number,
                                [&](std::vector<LayerCache>& caches) {
                                        caches.emplace_back(layer_cache.first);
                                });
                current_digest = layer_cache.first.hash;
        }
}

auto BuildExecutor::exec_copy(const GraphBuilder::Stage& stage, const Instruction::CopyInstruction& instruction,
                const GraphBuilder::ParsedInstructionsMaps& maps, const std::string& raw_instruction, std::string& current_digest) -> void {
        fs::path final_dir{};
        std::vector<fs::path> final_srcs{};
        bool is_overlay{false};
        ScopeGuard guard{[&]() -> void {
                        if (is_overlay) {
                                umount2(final_dir.c_str(), MNT_DETACH);
                                Utils::remove_directory(final_dir);
                        }
                }};
        if (instruction.from_stage.has_value()) {
                if (instruction.is_dependency) {
                        size_t node{};
                        std::vector<std::string> lower_dirs{};
                        if (!maps.stage_alias_to_node_number.find(instruction.from_stage.value(), node)) [[unlikely]] {
                                throw std::runtime_error(std::format("COPY Failed: Unknown stage alias '{}'",
                                                        instruction.from_stage.value()));
                        }
                        if (!m_stage_lower_dirs.find(node, lower_dirs)) [[unlikely]] {
                                throw std::runtime_error(std::format("COPY Failed: Unknown stage node '{}'",
                                                        node));
                        }
                        std::string lower_dirs_str{};
                        for (auto it{lower_dirs.rbegin()}; it != lower_dirs.rend(); ++it) {
                                lower_dirs_str += *it + ':';
                        }
                        lower_dirs_str.pop_back();
                        fs::path merged{std::format("/tmp/quiver_merged_{}", get_temp_filename())};
                        Utils::ensure_dir(merged);
                        std::string opts{std::format("lowerdir={}", lower_dirs_str)};
                        if (!Mount::_overlay_fs(merged, opts)) {
                                throw std::runtime_error("COPY Failed: Overlay mount failed");
                        }
                        is_overlay = true;
                        final_dir = merged;
                }
                else {
                        final_dir = Utils::get_image_path(instruction.from_stage.value());
                }
                for (const auto& src : instruction.srcs) {
                        auto path{final_dir / src};
                        final_srcs.emplace_back(path);
                }
        }
        else {
                for (const auto& src : instruction.srcs) {
                        auto path{src.is_absolute() ? src : m_build_dir / src};
                        final_srcs.emplace_back(path);
                }
        }
        std::sort(final_srcs.begin(), final_srcs.end());
        Instruction::InstructionHash instruction_hash{};
        instruction_hash.parent_digest = current_digest;
        instruction_hash.expanded_raw_ins = raw_instruction;
        instruction_hash.source_stage = instruction.from_stage.has_value() ? instruction.from_stage.value() : "";
        instruction_hash.file_checksum = compute_files_checksum(final_srcs, instruction.from_stage.has_value() ? final_dir : m_build_dir);
        std::string hash{get_instruction_hash(instruction_hash)};
        LayerCache cache_obj{};
        if (m_hash_digest.find(hash, cache_obj)) {
                m_stage_layers.update_fn(stage.node_number,
                                [&](std::vector<std::string>& layers) {
                                        layers.emplace_back(cache_obj.hash);
                                });
                m_stage_diff_ids.update_fn(stage.node_number,
                                [&](std::vector<std::string>& diff_ids) {
                                        diff_ids.emplace_back(cache_obj.diff_id);
                                });
                m_stage_lower_dirs.update_fn(stage.node_number,
                                [&](std::vector<std::string>& lower_dirs) {
                                        lower_dirs.emplace_back(cache_obj.lower_dir);
                                });
                m_stage_layer_caches.update_fn(stage.node_number,
                                [&](std::vector<LayerCache>& caches) {
                                        caches.emplace_back(cache_obj);
                                });
                current_digest = cache_obj.hash;
                return;
        }
        auto obj{m_layer_cache_manager->lookup(hash)};
        if (obj.has_value()) {
                m_stage_layers.update_fn(stage.node_number,
                                [&](std::vector<std::string>& layers) {
                                        layers.emplace_back(obj->hash);
                                });
                m_stage_diff_ids.update_fn(stage.node_number,
                                [&](std::vector<std::string>& diff_ids) {
                                        diff_ids.emplace_back(obj->diff_id);
                                });
                m_stage_lower_dirs.update_fn(stage.node_number,
                                [&](std::vector<std::string>& lower_dirs) {
                                        lower_dirs.emplace_back(obj->lower_dir);
                                });
                m_stage_layer_caches.update_fn(stage.node_number,
                                [&](std::vector<LayerCache>& caches) {
                                        caches.emplace_back(*obj);
                                });
                m_hash_digest.insert(hash, *obj);
                current_digest = obj->hash;
                return;
        }
        auto status{m_in_flight_cache_manager->acquire(hash)};
        if (status.is_owner) {
                fs::path dummy_path{std::format("{}/layer_snapshots/quiver_layer_{}", Utils::get_base_dir().string(), hash)};
                fs::path final_dst{dummy_path / instruction.dst};
                try {
                        Utils::ensure_dir(final_dst);
                        for (const auto& src : final_srcs) {
                                fs::path path{src.is_absolute() ? src : m_build_dir / src};
                                if (fs::exists(path)) {
                                        Utils::copy_directory(path, final_dst);
                                }
                                else {
                                        throw std::runtime_error(std::format("COPY failed: source '{}' does not exist.", src.string()));
                                }
                        }
                        std::vector<std::string> current_lower_dirs{};
                        m_stage_lower_dirs.find(stage.node_number, current_lower_dirs);
                        change_permission_and_owners(final_dst, instruction.chmod, instruction.chown, current_lower_dirs);
                        auto layer_info{Utils::create_oci_layer(dummy_path, Utils::get_layers_path(hash + ".tar.gz"))};
                        std::string sha256_hash{layer_info.blob_digest};
                        m_stage_layers.update_fn(stage.node_number,
                                        [&](std::vector<std::string>& layers) {
                                                layers.emplace_back(sha256_hash);
                                        });
                        m_stage_diff_ids.update_fn(stage.node_number,
                                        [&](std::vector<std::string>& diff_ids) {
                                                diff_ids.emplace_back(layer_info.diff_id);
                                        });
                        m_stage_lower_dirs.update_fn(stage.node_number,
                                        [&](std::vector<std::string>& lower_dir) {
                                                lower_dir.emplace_back(dummy_path.string());
                                        });
                        current_digest = sha256_hash;
                        LayerCache new_cache{sha256_hash, layer_info.diff_id, dummy_path.string(), static_cast<int64_t>(layer_info.blob_size)};
                        m_stage_layer_caches.update_fn(stage.node_number,
                                        [&](std::vector<LayerCache>& caches) {
                                                caches.emplace_back(new_cache);
                                        });
                        m_hash_digest.insert(hash, new_cache);
                        m_layer_cache_manager->store(hash, new_cache);
                        m_in_flight_cache_manager->finish_success(hash, std::make_pair(new_cache, dummy_path));
                }
                catch (...) {
                        Utils::remove_directory(dummy_path);
                        m_in_flight_cache_manager->finish_failure(hash, std::current_exception());
                        throw;
                }
        }
        else {
                const auto& layer_cache{status.build->future.get()};
                m_stage_layers.update_fn(stage.node_number,
                                [&](std::vector<std::string>& layers) {
                                        layers.emplace_back(layer_cache.first.hash);
                                });
                m_stage_diff_ids.update_fn(stage.node_number,
                                [&](std::vector<std::string>& diff_ids) {
                                        diff_ids.emplace_back(layer_cache.first.diff_id);
                                });
                m_stage_lower_dirs.update_fn(stage.node_number,
                                [&](std::vector<std::string>& lower_dir) {
                                        lower_dir.emplace_back(layer_cache.second.string());
                                });
                m_stage_layer_caches.update_fn(stage.node_number,
                                [&](std::vector<LayerCache>& caches) {
                                        caches.emplace_back(layer_cache.first);
                                });
                current_digest = layer_cache.first.hash;
        }
}

auto BuildExecutor::exec_run(const GraphBuilder::Stage& stage, const Instruction::RunInstruction& instruction,
                const GraphBuilder::ParsedInstructionsMaps& maps, const std::string& raw_instruction, std::string& current_digest) -> void {
        Instruction::InstructionHash instruction_hash{};
        std::string env_str{};
        std::set<std::string> config_envs{};
        for (const auto& [key, value] : stage.local_envs) {
                config_envs.emplace(key + '=' + value);
        }
        for (const auto& env : config_envs) {
                env_str += env + ',';
        }
        if (env_str.size() > 0) env_str.pop_back();
        instruction_hash.parent_digest = current_digest;
        instruction_hash.expanded_raw_ins = raw_instruction;
        if (stage.current_user.has_value()) {
                instruction_hash.user = std::format("{}:{}", stage.current_user->first, stage.current_user->second);
        }
        if (stage.current_workdir.has_value()) {
                instruction_hash.workdir = stage.current_workdir.value().workdir;
        }
        instruction_hash.env = env_str;
        std::string hash{get_instruction_hash(instruction_hash)};
        LayerCache cache_obj{};
        if (m_hash_digest.find(hash, cache_obj)) {
                m_stage_layers.update_fn(stage.node_number,
                                [&](std::vector<std::string>& layers) {
                                        layers.emplace_back(cache_obj.hash);
                                });
                m_stage_diff_ids.update_fn(stage.node_number,
                                [&](std::vector<std::string>& diff_ids) {
                                        diff_ids.emplace_back(cache_obj.diff_id);
                                });
                m_stage_lower_dirs.update_fn(stage.node_number,
                                [&](std::vector<std::string>& lower_dirs) {
                                        lower_dirs.emplace_back(cache_obj.lower_dir);
                                });
                m_stage_layer_caches.update_fn(stage.node_number,
                                [&](std::vector<LayerCache>& caches) {
                                        caches.emplace_back(cache_obj);
                                });
                current_digest = cache_obj.hash;
                return;
        }
        auto obj{m_layer_cache_manager->lookup(hash)};
        if (obj.has_value()) {
                m_stage_layers.update_fn(stage.node_number,
                                [&](std::vector<std::string>& layers) {
                                        layers.emplace_back(obj->hash);
                                });
                m_stage_diff_ids.update_fn(stage.node_number,
                                [&](std::vector<std::string>& diff_ids) {
                                        diff_ids.emplace_back(obj->diff_id);
                                });
                m_stage_lower_dirs.update_fn(stage.node_number,
                                [&](std::vector<std::string>& lower_dirs) {
                                        lower_dirs.emplace_back(obj->lower_dir);
                                });
                m_stage_layer_caches.update_fn(stage.node_number,
                                [&](std::vector<LayerCache>& caches) {
                                        caches.emplace_back(*obj);
                                });
                m_hash_digest.insert(hash, *obj);
                current_digest = obj->hash;
                return;
        }
        auto status{m_in_flight_cache_manager->acquire(hash)};
        if (status.is_owner) {
                try {
                        std::string container_id{Utils::generate_container_id()};
                        std::string base_dir{Utils::get_base_dir()};
                        std::string rootfs{};
                        bool is_overlay{false};
                        ScopeGuard guard{[&]() {
                                if(is_overlay) {
                                        umount2(rootfs.c_str(), MNT_DETACH);
                                }
                        }};
                        size_t node{};
                        if (maps.stage_alias_to_node_number.find(stage.base_image, node)) {
                                std::vector<std::string> lower_dirs{};
                                std::vector<std::string> current_lower_dirs{};
                                if (!m_stage_lower_dirs.find(node, lower_dirs)) [[unlikely]] {
                                        throw std::runtime_error("RUN failed: Unable to find base stage lower dirs.");
                                }
                                if (!m_stage_lower_dirs.find(stage.node_number, current_lower_dirs)) [[unlikely]] {
                                        throw std::runtime_error("RUN failed: Unable to find current stage lower dirs.");
                                }
                                std::string lower_dirs_str{};
                                if (!current_lower_dirs.empty()) {
                                        for (auto it{current_lower_dirs.rbegin()}; it != current_lower_dirs.rend(); ++it) {
                                                lower_dirs_str += *it + ':';
                                        }
                                }
                                for (auto it{lower_dirs.rbegin()}; it != lower_dirs.rend(); ++it) {
                                        lower_dirs_str += *it + ':';
                                }
                                if (!lower_dirs_str.empty()) lower_dirs_str.pop_back();
                                rootfs = std::format("/tmp/quiver_merged_{}", get_temp_filename());
                                Utils::ensure_dir(rootfs);
                                std::string opts{std::format("lowerdir={}", lower_dirs_str)};
                                if (!Mount::_overlay_fs(rootfs, opts)) [[unlikely]] {
                                        throw std::runtime_error("RUN failed: overlay mount failed");
                                }
                                is_overlay = true;
                        }
                        else {
                                std::string lower_dirs_str{};
                                std::string image_path{Utils::get_image_path(stage.base_image)};
                                std::vector<std::string> current_lower_dirs{};
                                if (!m_stage_lower_dirs.find(stage.node_number, current_lower_dirs)) [[unlikely]] {
                                        throw std::runtime_error("RUN failed: Unable to find current stage lower dirs.");
                                }
                                if (!current_lower_dirs.empty()) {
                                        for (auto it{current_lower_dirs.rbegin()}; it != current_lower_dirs.rend(); ++it) {
                                                lower_dirs_str += *it + ':';
                                        }
                                        lower_dirs_str += image_path;
                                        rootfs = std::format("/tmp/quiver_merged_{}", get_temp_filename());
                                        Utils::ensure_dir(rootfs);
                                        std::string opts{std::format("lowerdir={}", lower_dirs_str)};
                                        if (!Mount::_overlay_fs(rootfs, opts)) [[unlikely]] {
                                                throw std::runtime_error("RUN failed: overlay mount failed");
                                        }
                                        is_overlay = true;
                                }
                                else {
                                        rootfs = image_path;
                                }
                        }
                        auto config{SpecGenerator::generate_default_rootless_spec(container_id, rootfs)};
                        std::vector<std::string> config_args{instruction.is_shell_form ? instruction.shell_args : instruction.json_args};
                        if (stage.current_shell.has_value()) {
                                std::vector<std::string> final_args{};
                                for (const auto& arg : stage.current_shell.value().shell_args) {
                                        final_args.emplace_back(arg);
                                }
                                for (const auto& arg : config_args) {
                                        final_args.emplace_back(arg);
                                }
                                config_args = std::move(final_args);
                        }
                        if (stage.current_workdir.has_value()) {
                                config.cwd.value = stage.current_workdir.value().workdir;
                        }
                        if (stage.current_user.has_value()) {
                                std::vector<std::string> current_lower_dirs{};
                                m_stage_lower_dirs.find(stage.node_number, current_lower_dirs);
                                auto uid_gid = Utils::resolve_user_group(current_lower_dirs, stage.current_user.value());
                                config.user.uid = uid_gid.first;
                                config.user.gid = uid_gid.second;
                        }
                        config.env.value = std::vector(config_envs.begin(), config_envs.end());
                        config.args.value = config_args;
                        {
                                std::lock_guard lock{m_run_mutex};
                                m_container_monitor->init(config);
                                m_container_monitor->invoke_container();
                        }
                        fs::path upper_path{std::format("{}/filesystems/quiver_{}/upper_dir", base_dir, container_id)};
                        fs::path layer_snapshot{std::format("{}/layer_snapshots/quiver_layer_{}", base_dir, hash)};
                        Utils::ensure_dir(layer_snapshot);
                        Utils::rename_file_or_directory(upper_path, layer_snapshot);
                        auto layer_info{Utils::create_oci_layer(layer_snapshot, Utils::get_layers_path(hash + ".tar.gz"))};
                        std::string sha256_hash{layer_info.blob_digest};
                        m_stage_layers.update_fn(stage.node_number,
                                        [&](std::vector<std::string>& layers) {
                                                layers.emplace_back(sha256_hash);
                                        });
                        m_stage_diff_ids.update_fn(stage.node_number,
                                        [&](std::vector<std::string>& diff_ids) {
                                                diff_ids.emplace_back(layer_info.diff_id);
                                        });
                        m_stage_lower_dirs.update_fn(stage.node_number,
                                        [&](std::vector<std::string>& lower_dir) {
                                                lower_dir.emplace_back(layer_snapshot.string());
                                        });
                        current_digest = sha256_hash;
                        LayerCache new_cache{sha256_hash, layer_info.diff_id, layer_snapshot.string(), static_cast<int64_t>(layer_info.blob_size)};
                        m_stage_layer_caches.update_fn(stage.node_number,
                                        [&](std::vector<LayerCache>& caches) {
                                                caches.emplace_back(new_cache);
                                        });
                        m_hash_digest.insert(hash, new_cache);
                        m_layer_cache_manager->store(hash, new_cache);
                        m_in_flight_cache_manager->finish_success(hash, std::make_pair(new_cache, layer_snapshot));
                }
                catch (...) {
                        m_in_flight_cache_manager->finish_failure(hash, std::current_exception());
                        throw;
                }
        }
        else {
                const auto& layer_cache{status.build->future.get()};
                m_stage_layers.update_fn(stage.node_number,
                                [&](std::vector<std::string>& layers) {
                                        layers.emplace_back(layer_cache.first.hash);
                                });
                m_stage_diff_ids.update_fn(stage.node_number,
                                [&](std::vector<std::string>& diff_ids) {
                                        diff_ids.emplace_back(layer_cache.first.diff_id);
                                });
                m_stage_lower_dirs.update_fn(stage.node_number,
                                [&](std::vector<std::string>& lower_dir) {
                                        lower_dir.emplace_back(layer_cache.second.string());
                                });
                m_stage_layer_caches.update_fn(stage.node_number,
                                [&](std::vector<LayerCache>& caches) {
                                        caches.emplace_back(layer_cache.first);
                                });
                current_digest = layer_cache.first.hash;
        }
}

auto BuildExecutor::exec_shell(GraphBuilder::Stage& stage, const Instruction::ShellInstruction& instruction) -> void {
        stage.current_shell = instruction;
}

auto BuildExecutor::exec_user(GraphBuilder::Stage& stage, const Instruction::UserInstruction& instruction) -> void {
        stage.current_user = instruction.user_group;
}

auto BuildExecutor::exec_workdir(GraphBuilder::Stage& stage, const Instruction::WorkdirInstruction& instruction) -> void {
        stage.current_workdir = instruction;
}

auto BuildExecutor::change_permission_and_owners(const fs::path& path,
                const std::optional<mode_t>& mode, const std::optional<std::string>& chown, const std::vector<std::string>& lower_dirs) -> void {
        std::optional<std::pair<uid_t, gid_t>> uid_gid = std::nullopt;
        if (chown.has_value()) {
                uid_gid = Utils::resolve_user_group(lower_dirs, chown.value());
        }

        if (mode.has_value() && uid_gid.has_value()) {
                if (fs::is_directory(path)) {
                        Utils::change_permissions(path, mode.value());
                        Utils::change_owners(path, uid_gid->first, uid_gid->second);
                        for (const auto& entry : fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied)) {
                                Utils::change_permissions(entry.path(), mode.value());
                                Utils::change_owners(entry.path(), uid_gid->first, uid_gid->second);
                        }
                }
                else {
                        Utils::change_permissions(path, mode.value());
                        Utils::change_owners(path, uid_gid->first, uid_gid->second);
                }
        }
        else if (mode.has_value()) {
                if (fs::is_directory(path)) {
                        Utils::change_permissions(path, mode.value());
                        for (const auto& entry : fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied)) {
                                Utils::change_permissions(entry.path(), mode.value());
                        }
                }
                else {
                        Utils::change_permissions(path, mode.value());
                }
        }
        else if (uid_gid.has_value()) {
                if (fs::is_directory(path)) {
                        Utils::change_owners(path, uid_gid->first, uid_gid->second);
                        for (const auto& entry : fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied)) {
                                Utils::change_owners(entry.path(), uid_gid->first, uid_gid->second);
                        }
                }
                else {
                        Utils::change_owners(path, uid_gid->first, uid_gid->second);
                }
        }
}

auto BuildExecutor::prepare(const std::vector<GraphBuilder::Stage>& stages, GraphBuilder::ParsedInstructions& parsed_instructions,
                const GraphBuilder::ParsedInstructionsMaps& parsed_instruction_maps) -> void {
        std::unordered_set<std::string> images{};
        std::unordered_set<std::string> urls{};
        for (const auto& stage : stages) {
                size_t node{};
                if (!parsed_instruction_maps.stage_alias_to_node_number.find(stage.base_image, node)) {
                        images.emplace(stage.base_image);
                }
        }
        for (const auto& copy_instruction : parsed_instructions.copy_instructions) {
                if (copy_instruction.from_stage.has_value() && !copy_instruction.is_dependency) {
                        images.emplace(copy_instruction.from_stage.value());
                }
        }
        for (const auto& add_instruction : parsed_instructions.add_instructions) {
                for (const auto& url : add_instruction.urls) {
                        urls.emplace(url);
                }
        }
        std::vector<std::future<void>> workgroup{};
        for (const auto& image : images) {
                workgroup.emplace_back(m_thread_pool.submit([&, image]() -> void {
                                                std::string error{};
                                                std::string out_path{};
                                                json manifest{m_image_manager->pull(image, out_path, error)};
                                                if (!error.empty()) {
                                                        throw std::runtime_error(error);
                                                }
                                                std::string top_layer_digest{manifest["layers"].back()["digest"].get<std::string>().substr(7)};
                                                m_image_top_layer_digest.insert(image, top_layer_digest);
                                        }));
        }
        for (auto& worker : workgroup) {
                worker.get();
        }
        workgroup.clear();
        for (const auto& url : urls) {
                workgroup.emplace_back(m_thread_pool.submit([&, url]() -> void {
                                        fs::path file_path{download_file(url, "/tmp/quiver_downloads")};
                                        m_url_downloaded_file.insert(url, file_path);
                                        }));
        }
        for (auto& worker : workgroup) {
                worker.get();
        }
        for (auto& add_instruction : parsed_instructions.add_instructions) {
                for (const auto& url : add_instruction.urls) {
                        fs::path path{};
                        if (!m_url_downloaded_file.find(url, path)) {
                                throw std::runtime_error(std::format("Build Executor Error: Unable to find '{}' file path",
                                                        url));
                        }
                        add_instruction.srcs.emplace_back(path);
                }
        }
        std::unordered_set<std::string> archive_hashes{};
        std::unordered_set<fs::path> archive_paths{};
        libcuckoo::cuckoohash_map<fs::path, std::string> archive_path_to_hash{};
        for (auto& add_instruction : parsed_instructions.add_instructions) {
                std::unordered_set<fs::path> seen_paths{};
                std::vector<fs::path> final_srcs{};
                for (const auto& src : add_instruction.srcs) {
                        auto path{src.is_absolute() ? src : m_build_dir / src};
                        if (Utils::is_archive(path)) {
                                std::string hash{Utils::sha256_file(path)};
                                auto [_, inserted]{archive_hashes.insert(hash)};
                                fs::path final_path{std::format("/tmp/quiver_archive_{}", hash)};
                                if (!seen_paths.count(final_path)) {
                                        final_srcs.emplace_back(final_path);
                                        seen_paths.emplace(final_path);
                                }
                                if (inserted) {
                                        archive_paths.emplace(path);
                                        archive_path_to_hash.insert(path, hash);
                                }
                        }
                        else {
                                final_srcs.emplace_back(path);
                        }
                }
                add_instruction.srcs = std::move(final_srcs);
        }
        workgroup.clear();
        for (const auto& archive_path : archive_paths) {
                workgroup.emplace_back(m_thread_pool.submit([&, archive_path]() -> void {
                                                std::string hash{};
                                                archive_path_to_hash.find(archive_path, hash);
                                                fs::path dst{std::format("/tmp/quiver_archive_{}", hash)};
                                                Utils::ensure_dir(dst);
                                                Utils::extract_tarball(archive_path, dst);
                                }));
        }
        for (auto& worker : workgroup) {
                worker.get();
        }
}

[[nodiscard]] auto BuildExecutor::compute_files_checksum(const std::vector<fs::path>& srcs, const fs::path& root_dir) -> std::string {
        struct Entry {
                std::string relative_path;
                fs::path absolute_path;
                bool is_directory;
                bool is_symlink; };
        std::vector<Entry> entries{};
        std::unordered_set<std::string> seen{};
        for (const auto& src : srcs) {
                fs::path abs_path{src.is_absolute() ? src : root_dir / src};

                if (!fs::exists(abs_path) && !fs::is_symlink(abs_path)) {
                        throw std::runtime_error(std::format("Build Executor Error: Source '{}' does not exist.", src.string()));
                }

                auto add_entry{[&](const fs::path& path) {
                        std::string relative_path{fs::relative(path, root_dir).generic_string()};
                        if (!seen.insert(relative_path).second) {
                                return;
                        }
                        entries.emplace_back(Entry{std::move(relative_path), path, fs::is_directory(path), fs::is_symlink(path)});
                }};
                if (fs::is_regular_file(abs_path) || fs::is_symlink(abs_path)) {
                        add_entry(abs_path);
                }
                else if (fs::is_directory(abs_path)) {
                        add_entry(abs_path);
                        for (const auto& dir_entry : fs::recursive_directory_iterator(abs_path, fs::directory_options::skip_permission_denied)) {
                                add_entry(dir_entry.path());
                        }
                }
                else {
                        throw std::runtime_error(std::format("Build Executor Error: Unsupported source '{}'.", src.string()));
                }
        }
        std::sort(entries.begin(), entries.end(),
                        [](const Entry& lhs, const Entry& rhs) {
                                return lhs.relative_path < rhs.relative_path;
                        });

        blake3_hasher hasher{};
        blake3_hasher_init(&hasher);
        constexpr uint8_t separator{0};
        std::array<char, 8192> buffer{};
        for (const auto& entry : entries) {
                blake3_hasher_update(&hasher, reinterpret_cast<const uint8_t*>(entry.relative_path.data()), entry.relative_path.size());
                blake3_hasher_update(&hasher, &separator, 1);
                const auto perms{static_cast<std::uint32_t>(fs::status(entry.absolute_path).permissions())};
                blake3_hasher_update(&hasher, reinterpret_cast<const uint8_t*>(&perms), sizeof(perms));
                blake3_hasher_update(&hasher, &separator, 1);
                if (entry.is_symlink) {
                        auto target{fs::read_symlink(entry.absolute_path).generic_string()};
                        blake3_hasher_update(&hasher, reinterpret_cast<const uint8_t*>(target.data()), target.size());
                        blake3_hasher_update(&hasher, &separator, 1);
                        continue;
                }
                if (entry.is_directory) {
                        continue;
                }
                std::ifstream in(entry.absolute_path, std::ios::binary);
                if (!in) {
                        throw std::runtime_error(std::format("Build Executor Error: Failed to open '{}'.", entry.absolute_path.string()));
                }
                while (in) {
                        in.read(buffer.data(), buffer.size());
                        const auto bytes{in.gcount()};
                        if (bytes > 0) {
                                blake3_hasher_update(&hasher, reinterpret_cast<const uint8_t*>(buffer.data()), static_cast<size_t>(bytes));
                        }
                }
                blake3_hasher_update(&hasher, &separator, 1);
        }
        std::array<uint8_t, BLAKE3_OUT_LEN> digest{};
        blake3_hasher_finalize(&hasher, digest.data(), digest.size());
        std::stringstream hex_stream{};
        hex_stream << std::hex << std::setfill('0');
        for (std::size_t i{0}; i < BLAKE3_OUT_LEN; ++i) {
                hex_stream << std::setw(2) << static_cast<unsigned int>(digest[i]);
        }
        return hex_stream.str();
}

[[nodiscard]] auto BuildExecutor::get_instruction_hash(const Instruction::InstructionHash& instruction_hash) -> std::string {
        std::string input {
                (!instruction_hash.parent_digest.empty() ? (instruction_hash.parent_digest + '\0') : "") +
                (!instruction_hash.expanded_raw_ins.empty() ? (instruction_hash.expanded_raw_ins + '\0') : "") +
                (!instruction_hash.source_stage.empty() ? (instruction_hash.source_stage + '\0') : "") +
                (!instruction_hash.file_checksum.empty() ? (instruction_hash.file_checksum + '\0') : "") +
                (!instruction_hash.workdir.empty() ? (instruction_hash.workdir + '\0') : "") +
                (!instruction_hash.user.empty() ? (instruction_hash.user + '\0') : "") +
                (!instruction_hash.env.empty() ? (instruction_hash.env + '\0') : "")
        };
        blake3_hasher hasher{};
        blake3_hasher_init(&hasher);
        blake3_hasher_update(&hasher, input.c_str(), input.length());

        std::array<uint8_t, BLAKE3_OUT_LEN> output{};
        blake3_hasher_finalize(&hasher, output.data(), output.size());
        std::stringstream hex_stream{};
        hex_stream << std::hex << std::setfill('0');
        for (std::size_t i{0}; i < BLAKE3_OUT_LEN; ++i) {
                hex_stream << std::setw(2) << static_cast<int>(output[i]);
        }
        return hex_stream.str();
}

[[nodiscard]] auto BuildExecutor::download_file(const std::string& url, const fs::path& dst) -> fs::path {
        auto head_response{cpr::Head(cpr::Url{url})};
        auto it{head_response.header.find("content-disposition")};
        std::optional<std::string> filename{};
        if (it != head_response.header.end()) {
                filename = get_filename_from_content_disposition(it->second);
                if (!filename.has_value()) {
                        filename = get_temp_filename();
                }
                else {
                        filename = sanitize_filename(filename.value());
                }
        }
        else {
                filename = get_temp_filename();
        }
        std::ofstream out(dst / filename.value(), std::ios::binary);
        if (!out) {
                throw std::runtime_error(std::format("File Error: Could not open '{}'.", fs::path{dst / filename.value()}.string()));
        }
        auto download_response{cpr::Download(out, cpr::Url{url}, cpr::Redirect{true}, cpr::VerifySsl{true})};
        out.close();
        if (download_response.error.code != cpr::ErrorCode::OK) {
                try {
                        Utils::remove_directory(fs::path{dst / filename.value()});
                }
                catch (const std::exception& e) {
                        throw std::runtime_error(e.what());
                }
                throw std::runtime_error(std::format("Download Error: '{}'", download_response.error.message));
        }
        if (download_response.status_code >= 400) {
                try {
                        Utils::remove_directory(fs::path{dst / filename.value()});
                }
                catch (const std::exception& e) {
                        throw std::runtime_error(e.what());
                }
                throw std::runtime_error(std::format("Download Error: '{}'", download_response.error.message));
        }
        return dst / filename.value();
}

[[nodiscard]] auto BuildExecutor::get_filename_from_content_disposition(const std::string& header) -> std::optional<std::string> {
        auto trim_str{[](std::string& s) {
                auto start = s.find_first_not_of(" \t\r\n");
                auto end   = s.find_last_not_of(" \t\r\n");
                s = (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
        }};

        // According to RFC 5987
        if (auto pos = header.find("filename*=");
            pos != std::string::npos) {
                pos += 10;
                auto end{header.find(';', pos)};
                size_t len{(end == std::string::npos) ? std::string::npos : end - pos};
                std::string value{header.substr(pos, len)};
                trim_str(value);

                auto first_quote{value.find('\'')};
                if (first_quote == std::string::npos) return std::nullopt;
                auto second_quote{value.find('\'', first_quote + 1)};
                if (second_quote == std::string::npos) return std::nullopt;

                return percent_decode(value.substr(second_quote + 1));
        }
        auto pos{header.find("filename=")};
        while (pos != std::string::npos) {
                if (pos == 0 || header[pos - 1] != '*') break;
                pos = header.find("filename=", pos + 1);
        }
        if (pos != std::string::npos) {
                pos += 9;
                auto end{header.find(';', pos)};
                size_t len{(end == std::string::npos) ? std::string::npos : end - pos};
                std::string filename{header.substr(pos, len)};
                trim_str(filename);
                if (filename.size() >= 2 &&
                    filename.front() == '"' &&
                    filename.back()  == '"') {
                        filename = filename.substr(1, filename.size() - 2);
                }
                return filename;
        }
        return std::nullopt;
}

[[nodiscard]] auto BuildExecutor::percent_decode(const std::string& encoded) -> std::string {
        std::string decoded{};
        decoded.reserve(encoded.size());
        for (size_t i{}; i < encoded.size(); ++i) {
                if (encoded[i] == '%' && i + 2 < encoded.size() &&
                    std::isxdigit(static_cast<unsigned char>(encoded[i + 1])) &&
                    std::isxdigit(static_cast<unsigned char>(encoded[i + 2]))) {
                        const auto hex{encoded.substr(i + 1, 2)};
                        decoded.push_back(static_cast<char>(std::stoi(hex, nullptr, 16)));
                        i += 2;
                }
                else {
                        decoded.push_back(encoded[i]);
                }
        }
        return decoded;
}

[[nodiscard]] auto BuildExecutor::sanitize_filename(const std::string& name) -> std::string {
        std::string result{};
        for (char c : name) {
                if (c == '/' || c == '\\' || c == '\0') continue;
                result += c;
        }
        size_t start{result.find_first_not_of(". ")};
        return (start == std::string::npos) ? get_temp_filename() : result.substr(start);
}

[[nodiscard]] auto BuildExecutor::get_temp_filename() -> std::string {
        std::array<uint8_t, 32> random{};
        std::random_device rd{};

        for (auto& b : random)
                b = static_cast<uint8_t>(rd());

        blake3_hasher hasher{};
        blake3_hasher_init(&hasher);
        blake3_hasher_update(&hasher, random.data(), random.size());

        std::array<uint8_t, BLAKE3_OUT_LEN> output{};
        blake3_hasher_finalize(&hasher, output.data(), output.size());
        std::stringstream hex_stream{};
        hex_stream << std::hex << std::setfill('0');
        for (std::size_t i{0}; i < BLAKE3_OUT_LEN; ++i) {
                hex_stream << std::setw(2) << static_cast<int>(output[i]);
        }
        return hex_stream.str();
}


auto BuildExecutor::assemble_oci_image(const GraphBuilder::Stage& stage, const std::string& target_name) -> void {
        std::vector<std::string> layers{};
        if (!m_stage_layers.find(stage.node_number, layers)) {
                throw std::runtime_error("Build Executor Error: Stage layers not found.");
        }
        std::vector<std::string> diff_ids{};
        if (!m_stage_diff_ids.find(stage.node_number, diff_ids)) {
                throw std::runtime_error("Build Executor Error: Stage diff_ids not found.");
        }

        nlohmann::json manifest = {
                {"schemaVersion", 2},
                {"mediaType", "application/vnd.oci.image.manifest.v1+json"},
                {"layers", nlohmann::json::array()}
        };
        nlohmann::json config = {
                {"architecture", "amd64"},
                {"os", "linux"},
                {"config", {
                        {"Env", nlohmann::json::array()},
                        {"Cmd", nlohmann::json::array()},
                        {"Entrypoint", nlohmann::json::array()},
                        {"WorkingDir", ""},
                        {"User", ""}
                }},
                {"rootfs", {
                        {"type", "layers"},
                        {"diff_ids", diff_ids}
                }}
        };

        if (stage.cmd_instruction.has_value()) {
                if (stage.cmd_instruction->is_shell_form) {
                        config["config"]["Cmd"] = stage.cmd_instruction->shell_args;
                } else if (stage.cmd_instruction->is_json_form) {
                        config["config"]["Cmd"] = stage.cmd_instruction->json_args;
                }
        }
        if (stage.entrypoint_instruction.has_value()) {
                if (stage.entrypoint_instruction->is_shell_form) {
                        config["config"]["Entrypoint"] = stage.entrypoint_instruction->shell_args;
                } else if (stage.entrypoint_instruction->is_json_form) {
                        config["config"]["Entrypoint"] = stage.entrypoint_instruction->json_args;
                }
        }
        if (stage.current_workdir.has_value()) {
                config["config"]["WorkingDir"] = stage.current_workdir->workdir;
        }
        if (stage.current_user.has_value()) {
                config["config"]["User"] = stage.current_user.value();
        }

        for (const auto& [key, value] : stage.local_envs) {
                config["config"]["Env"].push_back(key + "=" + value);
        }

        std::vector<LayerCache> layer_caches{};
        if (!m_stage_layer_caches.find(stage.node_number, layer_caches)) {
                throw std::runtime_error("Build Executor Error: Stage layer caches not found.");
        }

        for (const auto& cache : layer_caches) {
                nlohmann::json layer_json = {
                        {"mediaType", "application/vnd.oci.image.layer.v1.tar+gzip"},
                        {"digest", "sha256:" + cache.hash},
                        {"size", cache.blob_size}
                };
                manifest["layers"].push_back(layer_json);
        }

        std::string config_str = config.dump();
        std::string config_digest = Utils::sha256(config_str);
        manifest["config"] = {
                {"mediaType", "application/vnd.oci.image.config.v1+json"},
                {"digest", "sha256:" + config_digest},
                {"size", config_str.length()}
        };

        fs::path target_path = Utils::get_image_path(target_name);
        Utils::ensure_dir(target_path);

        nlohmann::json index = {
                {"schemaVersion", 2},
                {"manifests", {
                        {
                                {"mediaType", "application/vnd.oci.image.manifest.v1+json"},
                                {"digest", "sha256:" + Utils::sha256(manifest.dump())},
                                {"size", manifest.dump().length()},
                                {"annotations", {
                                        {"org.opencontainers.image.ref.name", target_name}
                                }}
                        }
                }}
        };
        Utils::write_file(target_path / "index.json", index.dump());
        Utils::write_file(target_path / "oci-layout", R"({"imageLayoutVersion": "1.0.0"})");

        fs::path blobs_dir = target_path / "blobs" / "sha256";
        Utils::ensure_dir(blobs_dir);
        Utils::write_file(blobs_dir / config_digest, config_str);
        Utils::write_file(blobs_dir / Utils::sha256(manifest.dump()), manifest.dump());

        for (const auto& layer_hash : layers) {
                fs::path source_layer = Utils::get_layers_path(layer_hash + ".tar.gz");
                fs::path target_blob = blobs_dir / layer_hash;
                if (!fs::exists(target_blob)) {
                        fs::create_hard_link(source_layer, target_blob);
                }
        }
}
