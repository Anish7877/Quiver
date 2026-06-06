#pragma once
#include "build_file_parser.hpp"
#include "instruction_types.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#define MIN_PORT_RANGE 1
#define MAX_PORT_RANGE 65535

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
                        std::vector<Instruction::ExposeInstruction> local_exposed_ports{};
                        std::unordered_map<std::string, std::string> local_args{};
                        std::unordered_map<std::string, std::string> local_envs{};
                        std::unordered_map<std::string, std::string> local_labels{};
                };
                struct BuildGraph {
                        std::vector<BuildOp> ops{};
                        std::vector<Stage> stages{};
                        std::unordered_map<std::string, size_t> id_to_index{};
                        std::unordered_map<std::string, size_t> name_to_stage{};
                        std::string target_stage{};
                };
                struct ParsedInstructions {
                        std::vector<Instruction::CopyInstruction> copy_instructions{};
                        std::vector<Instruction::AddInstruction> add_instructions{};
                        std::vector<Instruction::CmdInstruction> cmd_instructions{};
                        std::vector<Instruction::EntrypointInstruction> entrypoint_instructions{};
                        std::vector<Instruction::RunInstruction> run_instructions{};
                        std::vector<Instruction::ShellInstruction> shell_instructions{};
                };
                GraphBuilder() = default;
                GraphBuilder(GraphBuilder&&) = delete;
                GraphBuilder(const GraphBuilder&) = delete;
                GraphBuilder &operator=(GraphBuilder&&) = delete;
                GraphBuilder &operator=(const GraphBuilder&) = delete;
                ~GraphBuilder() = default;

                [[nodiscard]] auto build_graph(std::vector<BuildFileParser::BuildInstruction>&, const std::unordered_map<std::string, std::string>&) -> BuildGraph;
        private:
                [[nodiscard]] auto create_build_ops(const std::vector<Stage>&) -> std::vector<BuildOp>;
                [[nodiscard]] auto split_into_stages(std::vector<BuildFileParser::BuildInstruction>&, const std::unordered_map<std::string, std::string>&) -> std::vector<Stage>;
                [[nodiscard]] auto expand_args(const std::string&, const std::unordered_map<std::string, std::string>&) -> std::string;
                [[nodiscard]] auto expand_args(const std::vector<std::string>&, const std::unordered_map<std::string, std::string>&) -> std::vector<std::string>;
                [[nodiscard]] auto parse_arg(const std::string&, std::uint32_t) -> std::pair<std::string, std::optional<std::string>>;
                auto parse_add_instruction(BuildFileParser::BuildInstruction&, const std::unordered_map<std::string, std::string>&, const std::unordered_map<std::string, std::string>&, const std::unordered_map<std::string, std::string>&, size_t) -> void;
                auto parse_copy_instruction(BuildFileParser::BuildInstruction&, const std::unordered_map<std::string, std::string>&, const std::unordered_map<std::string, std::string>&, const std::unordered_map<std::string, std::string>&, size_t) -> void;
                auto parse_cmd_instruction(BuildFileParser::BuildInstruction&, size_t) -> void;
                auto parse_entrypoint_instruction(BuildFileParser::BuildInstruction&, size_t) -> void;
                auto parse_env_instruction(Stage*, BuildFileParser::BuildInstruction&, const std::unordered_map<std::string, std::string>&)-> void;
                auto parse_expose_instruction(Stage*, BuildFileParser::BuildInstruction&, const std::unordered_map<std::string, std::string>&)-> void;
                [[nodiscard]] auto parse_from_instruction(BuildFileParser::BuildInstruction&, const std::unordered_map<std::string, std::string>&) -> Stage;
                auto parse_label_instruction(Stage*, BuildFileParser::BuildInstruction&, const std::unordered_map<std::string, std::string>&)-> void;
                auto parse_run_instruction(BuildFileParser::BuildInstruction&, const std::unordered_map<std::string, std::string>&, const std::unordered_map<std::string, std::string>&, const std::unordered_map<std::string, std::string>&, size_t) -> void;
                auto parse_shell_instruction(BuildFileParser::BuildInstruction&, size_t) -> void;
                auto get_port(const std::string&) -> std::uint16_t;
                auto is_valid_variable_name(const std::string&) -> bool;
                ParsedInstructions m_parsed_instructions{};
                std::unordered_map<size_t, Instruction::InstructionType> m_index_to_type{};
                std::unordered_map<size_t, size_t> m_index_to_offset{};
};
