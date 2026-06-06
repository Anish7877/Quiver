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
                const std::unordered_map<std::string, std::string>& build_context) -> BuildGraph {
        BuildGraph graph{};
        graph.stages = split_into_stages(instructions, build_context);
        graph.ops = create_build_ops(graph.stages);
        return graph;
}

[[nodiscard]] auto GraphBuilder::create_build_ops(const std::vector<Stage>& stages) -> std::vector<BuildOp> {
        std::vector<BuildOp> ops{};
        return ops;
}

[[nodiscard]] auto GraphBuilder::split_into_stages(std::vector<BuildFileParser::BuildInstruction>& instructions, const std::unordered_map<std::string, std::string>& build_context) -> std::vector<Stage> {
        std::unordered_map<std::string, std::string> global_args{};
        std::vector<Stage> stages{};
        size_t stage_name_count{0};
        Stage* current_stage{nullptr};

        for (size_t i{0}; i < instructions.size(); ++i) {
                auto& instruction{instructions[i]};
                if (!current_stage && instruction.type == Instruction::InstructionType::ARG) {
                        auto expanded_form{expand_args(instruction.shell_form, global_args)};
                        auto parsed_args{parse_arg(expanded_form, instruction.line_number)};
                        auto it{build_context.find(parsed_args.first)};

                        if (it != build_context.end()) {
                                parsed_args.second = it->second;
                        }
                        else if (!parsed_args.second.has_value()) {
                                parsed_args.second = "";
                        }
                        global_args[parsed_args.first] = parsed_args.second.value();
                        continue;
                }

                if (!current_stage && instruction.type != Instruction::InstructionType::FROM) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Instruction before first FROM.",
                                                instruction.line_number));
                }

                if (instruction.type == Instruction::InstructionType::FROM) {
                        Stage stage{};
                        auto expanded_form{expand_args(instruction.shell_form, global_args)};
                        auto parsed{parse_from_as(expanded_form, instruction.line_number)};
                        stage.base_image = parsed.first;

                        if (parsed.second.has_value()) {
                                stage.name = parsed.second.value();
                        }
                        else {
                                stage.name = std::format("stage_{}", stage_name_count++);
                        }

                        stage.local_args = global_args;
                        stages.emplace_back(std::move(stage));
                        current_stage =&stages.back();
                }

                if (current_stage && instruction.type == Instruction::InstructionType::ARG) {
                        auto expanded_form{expand_args(instruction.shell_form, global_args)};
                        auto parsed_args{parse_arg(expanded_form, instruction.line_number)};
                        auto it{build_context.find(parsed_args.first)};
                        if (it != build_context.end()) {
                                parsed_args.second = it->second;
                        }
                        else if (!parsed_args.second.has_value()) {
                                parsed_args.second = "";
                        }
                        current_stage->local_args[parsed_args.first] = parsed_args.second.value();
                }
                if (current_stage && instruction.type == Instruction::InstructionType::ENV) {
                }
                current_stage->instruction_indices.emplace_back(i);
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


[[nodiscard]] auto GraphBuilder::parse_arg(const std::string& line, std::uint32_t line_number) -> std::pair<std::string, std::optional<std::string>> {
        if (line.empty()) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: No argument provided in ARG.",
                                        line_number));
        }
        std::pair<std::string, std::optional<std::string>> key_value;
        size_t eq_index{line.find('=')};
        if (eq_index == std::string::npos) {
                key_value.first = line;
                BuildFileParser::trim(key_value.first);
                if (!is_valid_variable_name(key_value.first)) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Invalid variable name '{}'.",
                                                line_number, key_value.first));
                }
                key_value.second = std::nullopt;
                return key_value;
        }
        key_value.first = line.substr(0, eq_index);
        BuildFileParser::trim(key_value.first);
        if (!is_valid_variable_name(key_value.first)) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Invalid variable name '{}'.",
                                        line_number, key_value.first));
        }
        std::string value{line.substr(eq_index+1)};
        BuildFileParser::trim(value);
        key_value.second = value;
        return key_value;
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

auto GraphBuilder::parse_copy_instruction(BuildFileParser::BuildInstruction& instruction,
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

auto GraphBuilder::parse_cmd_instruction(BuildFileParser::BuildInstruction& instruction, size_t instruction_index) -> void {
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
        m_index_to_type[instruction_index] = Instruction::InstructionType::CMD;
        m_index_to_offset[instruction_index] = m_parsed_instructions.cmd_instructions.size();
        m_parsed_instructions.cmd_instructions.emplace_back(std::move(cmd_ins));
}

auto GraphBuilder::parse_entrypoint_instruction(BuildFileParser::BuildInstruction& instruction, size_t instruction_index) -> void {
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
        m_index_to_type[instruction_index] = Instruction::InstructionType::ENTRYPOINT;
        m_index_to_offset[instruction_index] = m_parsed_instructions.entrypoint_instructions.size();
        m_parsed_instructions.entrypoint_instructions.emplace_back(std::move(entrypoint_ins));
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
                BuildFileParser::trim(key);
                if (key.empty()) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Empty key '{}'.",
                                                instruction.line_number, token));
                }
                std::string value{token.substr(eq_index+1)};
                BuildFileParser::trim(value);
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
                BuildFileParser::trim(token);
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
                const std::unordered_map<std::string, std::string>& global_args) -> Stage {
        if (!instruction.opts.empty()) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Invalid options provided in from instruction.",
                                        instruction.line_number));
        }
        if (instruction.shell_form.empty()) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Argument not provided in from instruction.",
                                        instruction.line_number));
        }
        Stage stage{};
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
        std::string stage_name{};
        if (!(iss >> stage_name)) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Missing stage alias after AS.",
                                        instruction.line_number));
        }
        stage.name = std::move(stage_name);
        if (iss >> stage_name) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: More than one stage name provided.",
                                        instruction.line_number));
        }
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
                BuildFileParser::trim(key);
                if (key.empty()) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Empty key '{}'.",
                                                instruction.line_number, token));
                }
                std::string value{token.substr(eq_index+1)};
                BuildFileParser::trim(value);
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
