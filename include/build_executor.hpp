#pragma once
#include "build_file_parser.hpp"
#include "graph_builder.hpp"
#include "instruction_types.hpp"
#include "thread_pool.hpp"
#include <filesystem>
#include <libcuckoo/cuckoohash_map.hh>
#include <vector>

class ImageManager;
class InFlightCacheManager;
class LayerCacheManager;
class BuildExecutor {
        public:
                BuildExecutor() = default;
                ~BuildExecutor() = default;
                BuildExecutor(BuildExecutor&&) = delete;
                BuildExecutor(const BuildExecutor&) = delete;
                auto operator=(BuildExecutor&&) -> BuildExecutor& = delete;
                auto operator=(const BuildExecutor&) -> BuildExecutor& = delete;

                auto execute_instructions(const std::vector<std::vector<size_t>>&, const std::vector<GraphBuilder::Stage>&, GraphBuilder::ParsedInstructions&, const GraphBuilder::ParsedInstructionsMaps&, const std::vector<BuildFileParser::BuildInstruction>&) -> void;
        private:
                [[nodiscard]] auto detect_cycles(const std::vector<std::vector<size_t>>&) -> bool;
                [[nodiscard]] auto get_topological_order(const std::vector<std::vector<size_t>>&) -> std::vector<std::vector<size_t>>;
                auto exec_stage(const GraphBuilder::Stage&, const GraphBuilder::ParsedInstructions&, const GraphBuilder::ParsedInstructionsMaps&, const std::vector<BuildFileParser::BuildInstruction>&) -> void;
                auto exec_add(const GraphBuilder::Stage&, const Instruction::AddInstruction&, const std::string&, std::string&) -> void;
                auto exec_copy(const GraphBuilder::Stage&, const Instruction::CopyInstruction&, const GraphBuilder::ParsedInstructionsMaps&, const std::string&, std::string&) -> void;
                auto exec_run(const GraphBuilder::Stage&, const Instruction::RunInstruction&, const std::string&, std::string&) -> void;
                auto exec_shell() -> void;
                auto exec_user() -> void;
                auto exec_workdir() -> void;
                auto change_permission_and_owners(const fs::path&, const std::optional<mode_t>&, const std::optional<std::pair<uid_t, gid_t>>&) -> void;
                auto prepare(const std::vector<GraphBuilder::Stage>&, GraphBuilder::ParsedInstructions&, const GraphBuilder::ParsedInstructionsMaps&) -> void;
                [[nodiscard]] auto compute_files_checksum(const std::vector<fs::path>&) -> std::string;
                [[nodiscard]] auto get_instruction_hash(const Instruction::InstructionHash&) -> std::string;
                [[nodiscard]] auto download_file(const std::string&, const fs::path&) -> fs::path;
                [[nodiscard]] auto get_filename_from_content_disposition(const std::string&) -> std::optional<std::string>;
                [[nodiscard]] auto percent_decode(const std::string&) -> std::string;
                [[nodiscard]] auto sanitize_filename(const std::string&) -> std::string;
                [[nodiscard]] auto get_temp_filename() -> std::string;
                fs::path m_build_dir{"/"};
                ImageManager* m_image_manager{};
                InFlightCacheManager* m_in_flight_cache_manager{};
                LayerCacheManager* m_layer_cache_manager{};
                ThreadPool m_thread_pool{static_cast<size_t>(std::jthread::hardware_concurrency())};
                libcuckoo::cuckoohash_map<size_t, std::vector<std::string>> m_stage_lower_dirs{};
                libcuckoo::cuckoohash_map<size_t, std::vector<std::string>> m_stage_layers{};
                libcuckoo::cuckoohash_map<std::string, std::string> m_hash_digest{};
                libcuckoo::cuckoohash_map<std::string, fs::path> m_url_downloaded_file{};
                libcuckoo::cuckoohash_map<std::string, std::string> m_image_top_layer_digest{};
                libcuckoo::cuckoohash_map<size_t, std::string> m_stage_final_digest{};
};
