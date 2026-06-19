#include "build_executor.hpp"
#include "image_manager.hpp"
#include <exception>
#include <functional>
#include <queue>
#include <stdexcept>
#include <thread>
#include <unordered_set>

auto BuildExecutor::execute_instructions(const std::vector<std::vector<size_t>>& graph,
                const std::vector<GraphBuilder::Stage>& stages,
                const GraphBuilder::ParsedInstructions& parsed_instructions,
                const GraphBuilder::ParsedInstructionsMaps& parsed_instruction_maps,
                const std::vector<BuildFileParser::BuildInstruction>& instructions) -> void {
        m_image_manager.reset(&ImageManager::get_instance());
        if (detect_cycles(graph)) {
                throw std::runtime_error("Build Executor Error: Cycle detected in dependency graph.");
        }
        std::unordered_set<std::string> images{};
        for (const auto& stage : stages) {
                auto it{parsed_instruction_maps.stage_alias_to_node_number.find(stage.base_image)};
                if (it == parsed_instruction_maps.stage_alias_to_node_number.end()) {
                        images.insert(stage.base_image);
                }
        }
        for (const auto& copy_instruction : parsed_instructions.copy_instructions) {
                if (!copy_instruction.is_dependency) {
                        images.insert(copy_instruction.from_stage.value());
                }
        }
        std::vector<std::thread> image_pull_workgroup(images.size());
        std::vector<std::exception_ptr> image_pulling_errors(images.size());

        auto it{images.begin()};
        for (size_t i{}; i < images.size(); ++i) {
                std::string image{*it};
                image_pull_workgroup[i] = std::thread([&, i, image] {
                                try {
                                        std::string error;
                                        std::string out_path;

                                        m_image_manager->pull(image, out_path, error);

                                        if (!error.empty()) {
                                                throw std::runtime_error(error);
                                        }
                                }
                                catch (...) {
                                        image_pulling_errors[i] = std::current_exception();
                                }});
                ++it;
        }
        for (auto& worker : image_pull_workgroup) {
                if (worker.joinable())
                        worker.join();
        }
        for (const auto& error : image_pulling_errors) {
                if (error) {
                        std::rethrow_exception(error);
                }
        }
        auto layers{get_topological_order(graph)};
        std::vector<std::exception_ptr> stage_errors(stages.size());
        for(const auto& layer : layers) {
                std::vector<std::thread> workers(layer.size());
                for (size_t i{}; i < layer.size(); ++i) {
                        size_t stage_index{layer[i] - 1};
                        workers[i] = std::thread([&, stage_index](){
                                        try {
                                                exec_stage(stages[stage_index], parsed_instructions,
                                                                parsed_instruction_maps, instructions);
                                        }
                                        catch (...) {
                                                stage_errors[stage_index] = std::current_exception();
                                        }
                                        });
                }
                for (size_t i{}; i < layer.size(); ++i) {
                        if (workers[i].joinable()) {
                                workers[i].join();
                        }
                }
                for (const auto& stage : layer) {
                        if (stage_errors[stage-1]) {
                                std::rethrow_exception(stage_errors[stage-1]);
                        }
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

auto BuildExecutor::exec_stage(const GraphBuilder::Stage& stage,
                const GraphBuilder::ParsedInstructions& parsed_instructions,
                const GraphBuilder::ParsedInstructionsMaps& parsed_instruction_maps,
                const std::vector<BuildFileParser::BuildInstruction>& instructions) -> void {
}
