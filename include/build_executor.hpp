#pragma once
#include "build_file_parser.hpp"
#include "graph_builder.hpp"
#include <vector>

class BuildExecutor {
        public:
                BuildExecutor() = default;
                ~BuildExecutor() = default;
                BuildExecutor(BuildExecutor&&) = delete;
                BuildExecutor(const BuildExecutor&) = delete;
                auto operator=(BuildExecutor&&) -> BuildExecutor& = delete;
                auto operator=(const BuildExecutor&) -> BuildExecutor& = delete;

                auto execute_instructions(const std::vector<std::vector<size_t>>&, const std::vector<GraphBuilder::Stage>&, const GraphBuilder::ParsedInstructions&, const GraphBuilder::ParsedInstructionsMaps&, const std::vector<BuildFileParser::BuildInstruction>&) -> void;
        private:
                [[nodiscard]] auto detect_cycles(const std::vector<std::vector<size_t>>&) -> bool;
                [[nodiscard]] auto get_topological_order(const std::vector<std::vector<size_t>>&) -> std::vector<std::vector<size_t>>;
                auto exec_stage(const GraphBuilder::Stage&) -> void;
                auto exec_add() -> void;
                auto exec_copy() -> void;
                auto exec_run() -> void;
                auto exec_shell() -> void;
                auto exec_user() -> void;
                auto exec_work() -> void;
};
