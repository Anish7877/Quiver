#pragma once
#include "build_file_parser.hpp"
#include "instruction_types.hpp"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#define MIN_PORT_RANGE 1
#define MAX_PORT_RANGE 65535

class GraphBuilder{
        public:
                struct Stage {
                        size_t node_number{0};
                        std::vector<size_t> depends_on{0};
                        std::string base_image{};
                        std::optional<std::string> stop_signal{std::nullopt};
                        std::optional<std::string> current_user{std::nullopt};
                        std::optional<Instruction::ShellInstruction> current_shell{std::nullopt};
                        std::optional<Instruction::WorkdirInstruction> current_workdir{std::nullopt};
                        std::vector<size_t> instruction_indices{};
                        std::vector<std::string> volumes{};
                        std::vector<Instruction::ExposeInstruction> local_exposed_ports{};
                        std::unordered_map<std::string, std::string> local_args{};
                        std::unordered_map<std::string, std::string> local_envs{};
                        std::unordered_map<std::string, std::string> local_labels{};
                        std::vector<Instruction::CmdInstruction> cmd_instructions{};
                        std::vector<Instruction::EntrypointInstruction> entrypoint_instructions{};
                };
                struct ParsedInstructions {
                        std::vector<Instruction::CopyInstruction> copy_instructions{};
                        std::vector<Instruction::AddInstruction> add_instructions{};
                        std::vector<Instruction::RunInstruction> run_instructions{};
                        std::vector<Instruction::ShellInstruction> shell_instructions{};
                        std::vector<Instruction::UserInstruction> user_instructions{};
                        std::vector<Instruction::WorkdirInstruction> workdir_instructions{};
                };
                struct ParsedInstructionsMaps {
                        std::unordered_map<size_t, Instruction::InstructionType> index_to_type{};
                        std::unordered_map<size_t, size_t> index_to_offset{};
                };
                GraphBuilder() = default;
                GraphBuilder(GraphBuilder&&) = delete;
                GraphBuilder(const GraphBuilder&) = delete;
                auto operator=(GraphBuilder&&) -> GraphBuilder& = delete;
                auto operator=(const GraphBuilder&) -> GraphBuilder& = delete;
                ~GraphBuilder() = default;

                [[nodiscard]] auto build_graph(std::vector<BuildFileParser::BuildInstruction>&, const std::unordered_map<std::string, std::string>&) -> std::vector<std::vector<size_t>>;
                [[nodiscard]] auto get_stages() -> std::vector<Stage>&;
                [[nodiscard]] auto get_parsed_instruction() -> ParsedInstructions&;
                [[nodiscard]] auto get_instruction_maps() -> ParsedInstructionsMaps;
        private:
                [[nodiscard]] auto split_into_stages(std::vector<BuildFileParser::BuildInstruction>&, const std::unordered_map<std::string, std::string>&) -> std::vector<Stage>;
                [[nodiscard]] auto expand_args(const std::string&, const std::unordered_map<std::string, std::string>&) -> std::string;
                [[nodiscard]] auto expand_args(const std::vector<std::string>&, const std::unordered_map<std::string, std::string>&) -> std::vector<std::string>;
                [[nodiscard]] auto parse_arg_instruction(BuildFileParser::BuildInstruction&) -> std::pair<std::string, std::optional<std::string>>;
                auto parse_add_instruction(BuildFileParser::BuildInstruction&, const std::unordered_map<std::string, std::string>&, const std::unordered_map<std::string, std::string>&, const std::unordered_map<std::string, std::string>&, size_t) -> void;
                auto parse_copy_instruction(Stage*, BuildFileParser::BuildInstruction&, const std::unordered_map<std::string, std::string>&, const std::unordered_map<std::string, std::string>&, const std::unordered_map<std::string, std::string>&, size_t) -> void;
                auto parse_cmd_instruction(Stage*, BuildFileParser::BuildInstruction&) -> void;
                auto parse_entrypoint_instruction(Stage*, BuildFileParser::BuildInstruction&) -> void;
                auto parse_env_instruction(Stage*, BuildFileParser::BuildInstruction&, const std::unordered_map<std::string, std::string>&)-> void;
                auto parse_expose_instruction(Stage*, BuildFileParser::BuildInstruction&, const std::unordered_map<std::string, std::string>&)-> void;
                [[nodiscard]] auto parse_from_instruction(BuildFileParser::BuildInstruction&, const std::unordered_map<std::string, std::string>&, size_t) -> Stage;
                auto parse_label_instruction(Stage*, BuildFileParser::BuildInstruction&, const std::unordered_map<std::string, std::string>&)-> void;
                auto parse_run_instruction(BuildFileParser::BuildInstruction&, const std::unordered_map<std::string, std::string>&, const std::unordered_map<std::string, std::string>&, const std::unordered_map<std::string, std::string>&, size_t) -> void;
                auto parse_shell_instruction(BuildFileParser::BuildInstruction&, size_t) -> void;
                auto parse_stop_signal_instruction(Stage*, BuildFileParser::BuildInstruction&) -> void;
                auto parse_user_instruction(Stage*, BuildFileParser::BuildInstruction&, const std::unordered_map<std::string, std::string>&, size_t) -> void;
                auto parse_volume_instruction(Stage*, BuildFileParser::BuildInstruction&) -> void;
                auto parse_workdir_instruction(BuildFileParser::BuildInstruction&, const std::unordered_map<std::string, std::string>&, const std::unordered_map<std::string, std::string>&, const std::unordered_map<std::string, std::string>&, size_t) -> void;
                auto get_port(const std::string&) -> std::uint16_t;
                auto is_valid_variable_name(const std::string&) -> bool;
                ParsedInstructions m_parsed_instructions{};
                std::vector<Stage> m_stages{};
                std::unordered_map<size_t, Instruction::InstructionType> m_index_to_type{};
                std::unordered_map<size_t, size_t> m_index_to_offset{};
                std::unordered_map<std::string, size_t> m_stage_alias_to_node_number{};
};
