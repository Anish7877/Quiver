#include "graph_builder.hpp"
#include "build_file_parser.hpp"
#include "instruction_types.hpp"
#include <algorithm>
#include <cctype>
#include <format>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

[[nodiscard]] auto GraphBuilder::build_graph(std::vector<BuildFileParser::BuildInstruction>& instructions,
                const std::unordered_map<std::string, std::string>& build_context) -> std::vector<std::vector<size_t>> {
        std::vector<std::vector<size_t>> graph{};
        m_stages = split_into_stages(instructions, build_context);
        graph.resize(m_stages.size()+1);
        for (const auto& stage : m_stages) {
                for(const auto& dependency : stage.depends_on) {
                        graph[dependency].emplace_back(stage.node_number);
                }
        }
        return graph;
}

[[nodiscard]] auto GraphBuilder::get_stages() -> std::vector<Stage>& {
        return m_stages;
}

[[nodiscard]] auto GraphBuilder::get_parsed_instruction() -> ParsedInstructions& {
        return m_parsed_instructions;
}

[[nodiscard]] auto GraphBuilder::get_instruction_maps() -> ParsedInstructionsMaps {
        ParsedInstructionsMaps maps{};
        maps.index_to_offset = m_index_to_offset;
        maps.index_to_type = m_index_to_type;
        return maps;
}

[[nodiscard]] auto GraphBuilder::split_into_stages(std::vector<BuildFileParser::BuildInstruction>& instructions, const std::unordered_map<std::string, std::string>& build_context) -> std::vector<Stage> {
        std::unordered_map<std::string, std::string> global_args{};
        std::vector<Stage> stages{};
        size_t node_number{1};
        Stage* current_stage{nullptr};

        for (size_t i{}; i < instructions.size(); ++i) {
                auto& instruction{instructions[i]};
                if (current_stage) {
                        switch (instruction.type) {
                                case Instruction::InstructionType::ADD:
                                        {
                                                parse_add_instruction(instruction, global_args, current_stage->local_args, current_stage->local_envs, i);
                                                current_stage->instruction_indices.emplace_back(i);
                                                break;
                                        }
                                case Instruction::InstructionType::ARG:
                                        {
                                                auto parsed_args{parse_arg_instruction(instruction)};
                                                auto it{build_context.find(parsed_args.first)};
                                                if (parsed_args.second != std::nullopt) {
                                                        if (it != build_context.end()) {
                                                                parsed_args.second = it->second;
                                                        }
                                                }
                                                else {
                                                        if (it != build_context.end()) {
                                                                parsed_args.second = it->second;
                                                        }
                                                        else {
                                                                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Build context not provided for key '{}'.",
                                                                                        instruction.line_number, parsed_args.first));
                                                        }
                                                }
                                                current_stage->local_args[parsed_args.first] = parsed_args.second.value();
                                                break;
                                        }
                                case Instruction::InstructionType::CMD:
                                        {
                                                parse_cmd_instruction(current_stage, instruction);
                                                current_stage->instruction_indices.emplace_back(i);
                                                break;
                                        }
                                case Instruction::InstructionType::COPY:
                                        {
                                                parse_copy_instruction(current_stage, instruction, global_args, current_stage->local_args, current_stage->local_envs, i);
                                                current_stage->instruction_indices.emplace_back(i);
                                                break;
                                        }
                                case Instruction::InstructionType::ENTRYPOINT:
                                        {
                                                parse_entrypoint_instruction(current_stage, instruction);
                                                current_stage->instruction_indices.emplace_back(i);
                                                break;
                                        }
                                case Instruction::InstructionType::ENV:
                                        {
                                                parse_env_instruction(current_stage, instruction, global_args);
                                                break;
                                        }
                                case Instruction::InstructionType::EXPOSE:
                                        {
                                                parse_expose_instruction(current_stage, instruction, global_args);
                                                break;
                                        }
                                case Instruction::InstructionType::FROM:
                                        {
                                                auto stage{parse_from_instruction(instruction, global_args, node_number)};
                                                stages.emplace_back(std::move(stage));
                                                ++node_number;
                                                current_stage = &stages.back();
                                                break;
                                        }
                                case Instruction::InstructionType::HEALTHCHECK:
                                        {
                                                // TODO:
                                                break;
                                        }
                                case Instruction::InstructionType::LABEL:
                                        {
                                                parse_label_instruction(current_stage, instruction, global_args);
                                                break;
                                        }
                                case Instruction::InstructionType::ONBUILD:
                                        {
                                                // TODO:
                                                break;
                                        }
                                case Instruction::InstructionType::RUN:
                                        {
                                                parse_run_instruction(instruction, global_args, current_stage->local_args, current_stage->local_envs, i);
                                                current_stage->instruction_indices.emplace_back(i);
                                                break;
                                        }
                                case Instruction::InstructionType::SHELL:
                                        {
                                                parse_shell_instruction(instruction, i);
                                                current_stage->instruction_indices.emplace_back(i);
                                                break;
                                        }
                                case Instruction::InstructionType::STOPSIGNAL:
                                        {
                                                parse_stop_signal_instruction(current_stage, instruction);
                                                break;
                                        }
                                case Instruction::InstructionType::USER:
                                        {
                                                parse_user_instruction(current_stage, instruction, global_args, i);
                                                break;
                                        }
                                case Instruction::InstructionType::VOLUME:
                                        {
                                                parse_volume_instruction(current_stage, instruction);
                                                break;
                                        }
                                case Instruction::InstructionType::WORKDIR:
                                        {
                                                parse_workdir_instruction(instruction, global_args, current_stage->local_args, current_stage->local_envs, i);
                                                current_stage->instruction_indices.emplace_back(i);
                                                break;
                                        }
                                default:
                                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Unknown instruction.",
                                                                instruction.line_number));
                        }
                }
                else {
                        switch (instruction.type) {
                                case Instruction::InstructionType::ARG:
                                        {
                                                auto parsed_args{parse_arg_instruction(instruction)};
                                                auto it{build_context.find(parsed_args.first)};
                                                if (parsed_args.second != std::nullopt) {
                                                        if (it != build_context.end()) {
                                                                parsed_args.second = it->second;
                                                        }
                                                }
                                                else {
                                                        if (it != build_context.end()) {
                                                                parsed_args.second = it->second;
                                                        }
                                                        else {
                                                                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Build context not provided for key '{}'.",
                                                                                        instruction.line_number, parsed_args.first));
                                                        }
                                                }
                                                global_args[parsed_args.first] = parsed_args.second.value();
                                                break;
                                        }
                                case Instruction::InstructionType::FROM:
                                        {
                                                auto stage{parse_from_instruction(instruction, global_args, node_number)};
                                                stages.emplace_back(std::move(stage));
                                                ++node_number;
                                                current_stage = &stages.back();
                                                break;
                                        }
                                default:
                                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Instruction before FROM.",
                                                                instruction.line_number));
                        }
                }
        }
        if (stages.empty()) {
                throw std::runtime_error("Graph Builder Error: Minimum one from instruction required.");
        }
        return stages;
}

[[nodiscard]] auto GraphBuilder::expand_args(const std::string& line, const std::unordered_map<std::string, std::string>& args) -> std::string {
        std::string result{};
        for (size_t i{0}; i < line.size(); ++i) {
                if (line[i] == '$' && i + 1 < line.size() && line[i + 1] == '{') {
                        size_t start{i + 2};
                        size_t end{line.find('}', start)};
                        if (end == std::string::npos) {
                                result += line[i];
                                continue;
                        }
                        std::string var_name{line.substr(start, end - start)};
                        auto it{args.find(var_name)};
                        if (it != args.end()) {
                                result += it->second;
                        }
                        i = end;
                        continue;
                }
                result += line[i];
        }
        return result;
}

[[nodiscard]] auto GraphBuilder::expand_args(const std::vector<std::string>& json_args, const std::unordered_map<std::string, std::string>& args) -> std::vector<std::string> {
        std::vector<std::string> expanded_args{};
        for (const auto& json_arg : json_args) {
                expanded_args.emplace_back(expand_args(json_arg, args));
        }
        return expanded_args;
}


[[nodiscard]] auto GraphBuilder::parse_arg_instruction(BuildFileParser::BuildInstruction& instruction) -> std::pair<std::string, std::optional<std::string>> {
        if (!instruction.opts.empty()) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Invalid options provided in arg instruction.",
                                        instruction.line_number));
        }
        if (instruction.shell_form.empty()) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Empty shell arguments in arg instruction.",
                                                instruction.line_number));
        }
        std::stringstream iss{instruction.shell_form};
        std::string token{};
        std::string extra{};
        iss >> token;
        if (iss >> extra) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Only one argument definition should be provided.",
                                        instruction.line_number));
        }
        size_t eq_index{token.find('=')};
        if (eq_index == std::string::npos) {
                if (is_valid_variable_name(token)) {
                        return std::make_pair(token, std::nullopt);
                }
                else {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Invalid key name '{}'.",
                                                instruction.line_number, token));
                }
        }
        std::string key{token.substr(0, eq_index)};
        std::string value{token.substr(eq_index+1)};
        if (key.empty()) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Empty key in token '{}'.",
                                        instruction.line_number,token));
        }
        if (!is_valid_variable_name(key)) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Invalid key name '{}'.",
                                                instruction.line_number, key));
        }
        return std::make_pair(key, value);
}

auto GraphBuilder::parse_add_instruction(BuildFileParser::BuildInstruction& instruction,
                const std::unordered_map<std::string, std::string>& global_args,
                const std::unordered_map<std::string, std::string>& local_args,
                const std::unordered_map<std::string, std::string>& local_envs, size_t instruction_index) -> void {
        Instruction::AddInstruction add_ins{};
        std::unordered_set<std::string> seen{};
        std::unordered_map<std::string, std::string> extended_args{global_args};
        for (const auto& [key, value] : local_args) {
                extended_args[key] = value;
        }
        for (const auto& [key, value] : local_envs) {
                extended_args[key] = value;
        }
        if (!instruction.opts.empty()) {
                for (auto& opt : instruction.opts) {
                        opt.value = expand_args(opt.value, extended_args);
                        auto [_, inserted]{seen.insert(opt.key)};
                        if (opt.key == "chown") {
                                if (!inserted) {
                                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Multiple chown detected in add instruction.",
                                                                instruction.line_number));
                                }
                                add_ins.chown = opt.value;
                        }
                        else if (opt.key == "chmod") {
                                if (!inserted) {
                                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Multiple chmod detected in add instruction.",
                                                                instruction.line_number));
                                }
                                add_ins.chmod = opt.value;
                        }
                        else {
                                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Unknown option '{}'.",
                                                        instruction.line_number, opt.key));
                        }
                }
        }
        if (instruction.is_shell_form) {
                if (instruction.shell_form.empty()) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Empty shell arguments in add instruction.",
                                                instruction.line_number));
                }
                instruction.shell_form = expand_args(instruction.shell_form, extended_args);
                std::stringstream iss{instruction.shell_form};
                std::vector<std::string> tokens{};
                std::string token{};
                while (iss >> token) {
                        tokens.emplace_back(token);
                }
                if (tokens.size() < 2) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Source or destination not specified.",
                                                instruction.line_number));
                }
                add_ins.dst = std::move(tokens.back());
                tokens.pop_back();
                add_ins.srcs = std::move(tokens);
        }
        else if (instruction.is_json_form) {
                if (instruction.json_args.empty()) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Empty json arguments in add instruction.",
                                                instruction.line_number));
                }
                instruction.json_args = expand_args(instruction.json_args, extended_args);
                if (instruction.json_args.size() < 2) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Source or destination not specified.",
                                                instruction.line_number));
                }
                add_ins.dst = std::move(instruction.json_args.back());
                instruction.json_args.pop_back();
                add_ins.srcs = std::move(instruction.json_args);
        }
        m_index_to_type[instruction_index] = Instruction::InstructionType::ADD;
        m_index_to_offset[instruction_index] = m_parsed_instructions.add_instructions.size();
        m_parsed_instructions.add_instructions.emplace_back(std::move(add_ins));
}

auto GraphBuilder::parse_copy_instruction(Stage* stage, BuildFileParser::BuildInstruction& instruction,
                const std::unordered_map<std::string, std::string>& global_args,
                const std::unordered_map<std::string, std::string>& local_args,
                const std::unordered_map<std::string, std::string>& local_envs, size_t instruction_index) -> void {
        Instruction::CopyInstruction copy_ins{};
        std::unordered_set<std::string> seen{};
        std::unordered_map<std::string, std::string> extended_args{global_args};
        for (const auto& [key, value] : local_args) {
                extended_args[key] = value;
        }
        for (const auto& [key, value] : local_envs) {
                extended_args[key] = value;
        }
        if (!instruction.opts.empty()) {
                for (auto& opt : instruction.opts) {
                        opt.value = expand_args(opt.value, extended_args);
                        auto [_, inserted]{seen.insert(opt.key)};
                        if (opt.key == "from") {
                                if (!inserted) {
                                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Multiple from detected in copy instruction.",
                                                                instruction.line_number));
                                }
                                copy_ins.from_stage = opt.value;
                                auto it{m_stage_alias_to_node_number.find(opt.value)};
                                if (it != m_stage_alias_to_node_number.end()) {
                                        copy_ins.is_dependency = true;
                                        stage->depends_on.emplace_back(it->second);
                                }
                        }
                        else if (opt.key == "chown") {
                                if (!inserted) {
                                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Multiple chown detected in copy instruction.",
                                                                instruction.line_number));
                                }
                                copy_ins.chown = opt.value;
                        }
                        else if (opt.key == "chmod") {
                                if (!inserted) {
                                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Multiple chmod detected in copy instruction.",
                                                                instruction.line_number));
                                }
                                copy_ins.chmod = opt.value;
                        }
                        else {
                                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Unknown option '{}'.",
                                                        instruction.line_number, opt.key));
                        }
                }
        }
        if (instruction.is_shell_form) {
                if (instruction.shell_form.empty()) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Empty shell arguments in copy instruction.",
                                                instruction.line_number));
                }
                instruction.shell_form = expand_args(instruction.shell_form, extended_args);
                std::stringstream iss{instruction.shell_form};
                std::vector<std::string> tokens{};
                std::string token{};
                while (iss >> token) {
                        tokens.emplace_back(token);
                }
                if (tokens.size() < 2) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Source or destination not specified.",
                                                instruction.line_number));
                }
                copy_ins.dst = std::move(tokens.back());
                tokens.pop_back();
                copy_ins.srcs = std::move(tokens);
        }
        else if (instruction.is_json_form) {
                if (instruction.json_args.empty()) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Empty json arguments in copy instruction.",
                                                instruction.line_number));
                }
                instruction.json_args = expand_args(instruction.json_args, extended_args);
                if (instruction.json_args.size() < 2) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Source or destination not specified.",
                                                instruction.line_number));
                }
                copy_ins.dst = std::move(instruction.json_args.back());
                instruction.json_args.pop_back();
                copy_ins.srcs = std::move(instruction.json_args);
        }
        m_index_to_type[instruction_index] = Instruction::InstructionType::COPY;
        m_index_to_offset[instruction_index] = m_parsed_instructions.copy_instructions.size();
        m_parsed_instructions.copy_instructions.emplace_back(std::move(copy_ins));
}

auto GraphBuilder::parse_cmd_instruction(Stage* stage, BuildFileParser::BuildInstruction& instruction) -> void {
        Instruction::CmdInstruction cmd_ins{};
        if (!instruction.opts.empty()) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Invalid options provided in cmd instruction.",
                                        instruction.line_number));
        }
        if (instruction.is_shell_form) {
                if (instruction.shell_form.empty()) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Empty shell arguments in cmd instruction.",
                                                instruction.line_number));
                }
                cmd_ins.is_shell_form = true;
                std::stringstream iss{instruction.shell_form};
                std::string token{};
                while (iss >> token) {
                        cmd_ins.shell_args.emplace_back(std::move(token));
                }
        }
        else if (instruction.is_json_form) {
                if (instruction.json_args.empty()) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Empty json arguments in cmd instruction.",
                                                instruction.line_number));
                }
                cmd_ins.is_json_form = true;
                cmd_ins.json_args = instruction.json_args;
        }
        else {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Unexpected error.",
                                        instruction.line_number));
        }
        stage->cmd_instructions.emplace_back(cmd_ins);
}

auto GraphBuilder::parse_entrypoint_instruction(Stage* stage, BuildFileParser::BuildInstruction& instruction) -> void {
        Instruction::EntrypointInstruction entrypoint_ins{};
        if (!instruction.opts.empty()) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Invalid options provided in entrypoint instruction.",
                                        instruction.line_number));
        }
        if (instruction.is_shell_form) {
                if (instruction.shell_form.empty()) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Empty shell arguments in entrypoint instruction.",
                                                instruction.line_number));
                }
                entrypoint_ins.is_shell_form = true;
                std::stringstream iss{instruction.shell_form};
                std::string token{};
                while (iss >> token) {
                        entrypoint_ins.shell_args.emplace_back(std::move(token));
                }
        }
        else if (instruction.is_json_form) {
                if (instruction.json_args.empty()) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Empty json arguments in entrypoint instruction.",
                                                instruction.line_number));
                }
                entrypoint_ins.is_json_form = true;
                entrypoint_ins.json_args = instruction.json_args;
        }
        else {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Unexpected error.",
                                        instruction.line_number));
        }
        stage->entrypoint_instructions.emplace_back(entrypoint_ins);
}

auto GraphBuilder::parse_env_instruction(Stage* stage, BuildFileParser::BuildInstruction& instruction,
                const std::unordered_map<std::string, std::string>& global_args) -> void {
        if (!instruction.opts.empty()) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Invalid options provided in env instruction.",
                                        instruction.line_number));
        }
        if (instruction.shell_form.empty()) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Argument not provided in env instruction.",
                                        instruction.line_number));
        }
        std::unordered_map<std::string, std::string> extended_args{global_args};
        for (const auto& [key, value] : stage->local_args) {
                extended_args[key] = value;
        }
        for (const auto& [key, value] : stage->local_envs) {
                extended_args[key] = value;
        }
        std::stringstream iss{instruction.shell_form};
        std::string token{};
        while (iss >> token) {
                token = expand_args(token, extended_args);
                size_t eq_index{token.find('=')};
                if (eq_index == std::string::npos) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Invalid token '{}'.",
                                                instruction.line_number, token));
                }
                std::string key{token.substr(0, eq_index)};
                if (key.empty()) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Empty key '{}'.",
                                                instruction.line_number, token));
                }
                if (!is_valid_variable_name(key)) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Invalid key name '{}'.",
                                                instruction.line_number, key));
                }
                std::string value{token.substr(eq_index+1)};
                stage->local_envs[key] = value;
                extended_args[key] = value;
        }
}

auto GraphBuilder::parse_expose_instruction(Stage* stage, BuildFileParser::BuildInstruction& instruction,
                const std::unordered_map<std::string, std::string>& global_args) -> void {
        if (!instruction.opts.empty()) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Invalid options provided in expose instruction.",
                                        instruction.line_number));
        }
        if (instruction.shell_form.empty()) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Argument not provided in expose instruction.",
                                        instruction.line_number));
        }
        std::unordered_map<std::string, std::string> extended_args{global_args};
        for (const auto& [key, value] : stage->local_args) {
                extended_args[key] = value;
        }
        for (const auto& [key, value] : stage->local_envs) {
                extended_args[key] = value;
        }
        std::stringstream iss{instruction.shell_form};
        std::string token{};
        while (iss >> token) {
                Instruction::ExposeInstruction expo_ins{};
                token = expand_args(token, extended_args);
                size_t slash_index{token.find('/')};
                if (slash_index == std::string::npos) {
                        std::uint16_t port{get_port(token)};
                        if (port == 0) {
                                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Invalid port '{}'.",
                                                        instruction.line_number, token));
                        }
                        expo_ins.port = port;
                        stage->local_exposed_ports.emplace_back(std::move(expo_ins));
                }
                else {
                        std::string port{token.substr(0, slash_index)};
                        std::string protocol{token.substr(slash_index+1)};
                        std::uint16_t final_port{get_port(port)};
                        if (final_port == 0) {
                                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Invalid port '{}'.",
                                                        instruction.line_number, token));
                        }
                        if (protocol.empty()) {
                                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Protocol not found after '/'.",
                                                        instruction.line_number));
                        }
                        std::transform(protocol.begin(), protocol.end(), protocol.begin(),
                                        [](unsigned char c) -> char {
                                                return static_cast<char>(std::tolower(c));
                                        });
                        if (protocol == "udp" || protocol == "tcp") {
                                expo_ins.port = final_port;
                                expo_ins.protocol = std::move(protocol);
                                stage->local_exposed_ports.emplace_back(std::move(expo_ins));
                        }
                        else  {
                                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Invalid protocol '{}'.",
                                                        instruction.line_number, protocol));
                        }
                }
        }
}

[[nodiscard]] auto GraphBuilder::parse_from_instruction(BuildFileParser::BuildInstruction& instruction,
                const std::unordered_map<std::string, std::string>& global_args, size_t node_number) -> Stage {
        if (!instruction.opts.empty()) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Invalid options provided in from instruction.",
                                        instruction.line_number));
        }
        if (instruction.shell_form.empty()) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Argument not provided in from instruction.",
                                        instruction.line_number));
        }
        Stage stage{};
        stage.node_number = node_number;
        instruction.shell_form = expand_args(instruction.shell_form, global_args);
        std::stringstream iss{instruction.shell_form};
        std::string base_image{};
        if (!(iss >> base_image)) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Missing base image.",
                                        instruction.line_number));
        }
        std::string transformed_base{base_image};
        std::transform(transformed_base.begin(), transformed_base.end(), transformed_base.begin(),
                        [](unsigned char c) -> char {
                        return static_cast<char>(std::toupper(c));
                        });
        if (transformed_base == "AS") {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Missing base image.",
                                        instruction.line_number));
        }
        stage.base_image = std::move(base_image);
        auto it{m_stage_alias_to_node_number.find(base_image)};
        if (it != m_stage_alias_to_node_number.end()) {
                stage.depends_on.emplace_back(it->second);
        }
        std::string maybe_as{};
        if (!(iss >> maybe_as)) {
                return stage;
        }
        std::string transformed_as{maybe_as};
        std::transform(transformed_as.begin(), transformed_as.end(), transformed_as.begin(),
                        [](unsigned char c) -> char {
                        return static_cast<char>(std::toupper(c));
                        });
        if (transformed_as != "AS") {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Expected 'AS' after base image name.",
                                        instruction.line_number));
        }
        std::string stage_alias{};
        if (!(iss >> stage_alias)) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Missing stage alias after AS.",
                                        instruction.line_number));
        }
        if (iss >> stage_alias) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: More than one stage alias provided.",
                                        instruction.line_number));
        }
        m_stage_alias_to_node_number[stage_alias] = node_number;
        return stage;
}

auto GraphBuilder::parse_label_instruction(Stage* stage, BuildFileParser::BuildInstruction& instruction,
                const std::unordered_map<std::string, std::string>& global_args) -> void {
        if (!instruction.opts.empty()) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Invalid options provided in label instruction.",
                                        instruction.line_number));
        }
        if (instruction.shell_form.empty()) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Argument not provided in label instruction.",
                                        instruction.line_number));
        }
        std::unordered_map<std::string, std::string> extended_args{global_args};
        for (const auto& [key, value] : stage->local_args) {
                extended_args[key] = value;
        }
        for (const auto& [key, value] : stage->local_envs) {
                extended_args[key] = value;
        }
        std::stringstream iss{instruction.shell_form};
        std::string token{};
        while (iss >> token) {
                token = expand_args(token, extended_args);
                size_t eq_index{token.find('=')};
                if (eq_index == std::string::npos) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Invalid token '{}'.",
                                                instruction.line_number, token));
                }
                std::string key{token.substr(0, eq_index)};
                if (key.empty()) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Empty key '{}'.",
                                                instruction.line_number, token));
                }
                std::string value{token.substr(eq_index+1)};
                stage->local_labels[key] = std::move(value);
        }
}

auto GraphBuilder::parse_run_instruction(BuildFileParser::BuildInstruction& instruction,
                const std::unordered_map<std::string, std::string>& global_args,
                const std::unordered_map<std::string, std::string>& local_args,
                const std::unordered_map<std::string, std::string>& local_envs, size_t instruction_index) -> void {
        Instruction::RunInstruction run_ins{};
        if (!instruction.opts.empty()) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Invalid options provided in run instruction.",
                                        instruction.line_number));
        }
        std::unordered_map<std::string, std::string> extended_args{global_args};
        for (const auto& [key, value] : local_args) {
                extended_args[key] = value;
        }
        for (const auto& [key, value] : local_envs) {
                extended_args[key] = value;
        }
        if (instruction.is_shell_form) {
                if (instruction.shell_form.empty()) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Empty shell arguments in run instruction.",
                                                instruction.line_number));
                }
                run_ins.is_shell_form = true;
                instruction.shell_form = expand_args(instruction.shell_form, extended_args);
                std::stringstream iss{instruction.shell_form};
                std::string token{};
                while (iss >> token) {
                        run_ins.shell_args.emplace_back(std::move(token));
                }
        }
        else if (instruction.is_json_form) {
                if (instruction.json_args.empty()) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Empty json arguments in run instruction.",
                                                instruction.line_number));
                }
                instruction.json_args = expand_args(instruction.json_args, extended_args);
                run_ins.is_json_form = true;
                run_ins.json_args = instruction.json_args;
        }
        else {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Unexpected error.",
                                        instruction.line_number));
        }
        m_index_to_type[instruction_index] = Instruction::InstructionType::RUN;
        m_index_to_offset[instruction_index] = m_parsed_instructions.run_instructions.size();
        m_parsed_instructions.run_instructions.emplace_back(std::move(run_ins));
}

auto GraphBuilder::parse_shell_instruction(BuildFileParser::BuildInstruction& instruction, size_t instruction_index) -> void {
        Instruction::ShellInstruction shell_ins{};
        if (!instruction.opts.empty()) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Invalid options provided in shell instruction.",
                                        instruction.line_number));
        }
        if (instruction.is_json_form) {
                if (instruction.json_args.empty()) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Empty json arguments in shell instruction.",
                                                instruction.line_number));
                }
                shell_ins.shell_args = instruction.json_args;
        }
        else {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Invalid args format. Expected json form.",
                                        instruction.line_number));
        }
        m_index_to_type[instruction_index] = Instruction::InstructionType::SHELL;
        m_index_to_offset[instruction_index] = m_parsed_instructions.shell_instructions.size();
        m_parsed_instructions.shell_instructions.emplace_back(std::move(shell_ins));
}

auto GraphBuilder::parse_stop_signal_instruction(Stage* stage, BuildFileParser::BuildInstruction& instruction) -> void {
        if (!instruction.opts.empty()) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Invalid options provided in stopsignal instruction.",
                                        instruction.line_number));
        }
        if (instruction.shell_form.empty()) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Empty shell arguments in stopsignal instruction.",
                                        instruction.line_number));
        }
        std::stringstream iss{instruction.shell_form};
        std::string signal{};
        std::string extra{};
        iss >> signal;
        if (iss >> extra) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Only one signal should be provided.",
                                        instruction.line_number));
        }
        std::transform(signal.begin(), signal.end(), signal.begin(),
                        [](unsigned char c) -> char {
                                return static_cast<char>(std::toupper(c));
                        });
        auto it{Instruction::SIGNAL_STR_TO_MASK.find(signal)};
        if (it != Instruction::SIGNAL_STR_TO_MASK.end()) {
                stage->stop_signal = std::move(signal);
        }
        else {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Unknown stopsignal '{}'.",
                                        instruction.line_number, instruction.shell_form));
        }
}

auto GraphBuilder::parse_user_instruction(Stage* stage, BuildFileParser::BuildInstruction& instruction,
                const std::unordered_map<std::string, std::string>& global_args, size_t instruction_index) -> void {
        if (!instruction.opts.empty()) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Invalid options provided in user instruction.",
                                        instruction.line_number));
        }
        if (instruction.shell_form.empty()) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Empty shell arguments in user instruction.",
                                        instruction.line_number));
        }
        std::unordered_map<std::string, std::string> extended_args{global_args};
        for (const auto& [key, value] : stage->local_args) {
                extended_args[key] = value;
        }
        for (const auto& [key, value] : stage->local_envs) {
                extended_args[key] = value;
        }
        instruction.shell_form = expand_args(instruction.shell_form, extended_args);
        std::stringstream iss{instruction.shell_form};
        std::string user{};
        std::string extra{};
        iss >> user;
        if (iss >> extra) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Only one user should be provided.",
                                        instruction.line_number));
        }
        Instruction::UserInstruction user_ins{std::move(user)};
        m_index_to_type[instruction_index] = Instruction::InstructionType::USER;
        m_index_to_offset[instruction_index] = m_parsed_instructions.shell_instructions.size();
        m_parsed_instructions.user_instructions.emplace_back(std::move(user_ins));
}

auto GraphBuilder::parse_volume_instruction(Stage* stage, BuildFileParser::BuildInstruction& instruction) -> void {
        if (!instruction.opts.empty()) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Invalid options provided in volume instruction.",
                                        instruction.line_number));
        }
        if (instruction.is_shell_form) {
                if (instruction.shell_form.empty()) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Empty shell arguments in volume instruction.",
                                                instruction.line_number));
                }
                std::stringstream iss{instruction.shell_form};
                std::string token{};
                while (iss >> token) {
                        stage->volumes.emplace_back(std::move(token));
                }
        }
        else if (instruction.is_json_form) {
                if (instruction.json_args.empty()) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Empty json arguments in volume instruction.",
                                                instruction.line_number));
                }
                for (const auto& arg : instruction.json_args) {
                        stage->volumes.emplace_back(arg);
                }
        }
        else {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Unexpected error.",
                                        instruction.line_number));
        }
}

auto GraphBuilder::parse_workdir_instruction(BuildFileParser::BuildInstruction& instruction,
                const std::unordered_map<std::string, std::string>& global_args,
                const std::unordered_map<std::string, std::string>& local_args,
                const std::unordered_map<std::string, std::string>& local_envs, size_t instruction_index) -> void {
        if (!instruction.opts.empty()) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Invalid options provided in workdir instruction.",
                                        instruction.line_number));
        }
        std::unordered_map<std::string, std::string> extended_args{global_args};
        for (const auto& [key, value] : local_args) {
                extended_args[key] = value;
        }
        for (const auto& [key, value] : local_envs) {
                extended_args[key] = value;
        }
        instruction.shell_form = expand_args(instruction.shell_form, extended_args);
        if (instruction.shell_form.empty()) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Empty shell arguments in workdir instruction.",
                                        instruction.line_number));
        }
        std::stringstream iss{instruction.shell_form};
        std::string path{};
        std::string extra{};
        iss >> path;
        if (iss >> extra) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Only one path should be provided.",
                                        instruction.line_number));
        }
        Instruction::WorkdirInstruction workdir_ins{};
        workdir_ins.workdir = std::move(path);
        m_index_to_type[instruction_index] = Instruction::InstructionType::WORKDIR;
        m_index_to_offset[instruction_index] = m_parsed_instructions.workdir_instructions.size();
        m_parsed_instructions.workdir_instructions.emplace_back(std::move(workdir_ins));
}

auto GraphBuilder::get_port(const std::string& token) -> std::uint16_t {
        if (token.empty()) return 0;
        for (char c : token) {
                if (!std::isdigit(static_cast<unsigned char>(c))) return 0;
        }
        int port{0};
        try {
                port = std::stoi(token);
        }
        catch (...) {
                return 0;
        }
        if (port < MIN_PORT_RANGE || port > MAX_PORT_RANGE) {
                return 0;
        }
        return port;
}

auto GraphBuilder::is_valid_variable_name(const std::string& name) -> bool {
        if (name.empty()) {
                return false;
        }
        if (!(std::isalpha(static_cast<unsigned char>(name[0])) || name[0] == '_')) {
                return false;
        }
        for (size_t i{1}; i < name.size(); ++i) {
                if (!(std::isalnum(static_cast<unsigned char>(name[i])) || name[i] == '_')) return false;
        }
        return true;
}
