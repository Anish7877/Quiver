#pragma once
#include "build_file_parser.hpp"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class GraphBuilder{
        public:
                struct BuildOp {
                        std::string id{};
                        std::string stage_name{};
                        std::vector<std::string> depends_on{};
                        BuildFileParser::BuildInstruction instruction{};
                };
                struct Stage {
                        std::string name{};
                        std::string base_image{};
                        std::vector<size_t> instruction_indices{};
                        std::unordered_map<std::string, std::string> local_args{};
                        std::unordered_map<std::string, std::string> local_envs{};
                };
                struct BuildGraph {
                        std::vector<BuildOp> ops{};
                        std::vector<Stage> stages{};
                        std::unordered_map<std::string, size_t> id_to_index{};
                        std::unordered_map<std::string, size_t> name_to_stage{};
                        std::string target_stage{};
                };
                GraphBuilder() = default;
                GraphBuilder(GraphBuilder&&) = delete;
                GraphBuilder(const GraphBuilder&) = delete;
                GraphBuilder &operator=(GraphBuilder&&) = delete;
                GraphBuilder &operator=(const GraphBuilder&) = delete;
                ~GraphBuilder() = default;

                [[nodiscard]] auto build_graph(const std::vector<BuildFileParser::BuildInstruction>&, const std::unordered_map<std::string, std::string>&) -> BuildGraph;
        private:
                [[nodiscard]] auto create_build_ops(const std::vector<Stage>) -> std::vector<BuildOp>;
                [[nodiscard]] auto split_into_stages(const std::vector<BuildFileParser::BuildInstruction>&, const std::unordered_map<std::string, std::string>&) -> std::vector<Stage>;
                [[nodiscard]] auto expand_args(const std::string&, const std::unordered_map<std::string, std::string>&) -> std::string;
                [[nodiscard]] auto parse_arg(const std::string&, std::uint32_t) -> std::pair<std::string, std::optional<std::string>>;
                [[nodiscard]] auto parse_from_as(const std::string&, std::uint32_t) -> std::pair<std::string, std::optional<std::string>>;
};
