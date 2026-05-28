#include "graph_builder.hpp"
#include "build_file_parser.hpp"
#include <algorithm>
#include <cctype>
#include <format>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

[[nodiscard]] auto GraphBuilder::build_graph(const std::vector<BuildFileParser::BuildInstruction>& instructions,
                const std::unordered_map<std::string, std::string>& build_context) -> BuildGraph {
        BuildGraph graph{};
        graph.stages = split_into_stages(instructions, build_context);
        graph.ops = create_build_ops(graph.stages);
        return graph;
}

[[nodiscard]] auto GraphBuilder::create_build_ops(const std::vector<Stage> stages) -> std::vector<BuildOp> {
        std::vector<BuildOp> ops{};
        return ops;
}

[[nodiscard]]
auto GraphBuilder::split_into_stages(const std::vector<BuildFileParser::BuildInstruction>& instructions, const std::unordered_map<std::string, std::string>& build_context) -> std::vector<Stage> {
        std::unordered_map<std::string, std::string> global_args{};
        std::vector<Stage> stages{};
        size_t stage_name_count{0};
        Stage* current_stage{nullptr};

        for (size_t i{0}; i < instructions.size(); ++i) {
                const auto& instruction{instructions[i]};
                if (!current_stage && instruction.type == BuildFileParser::InstructionType::ARG) {
                        auto parsed_args{parse_arg(instruction.shell_form, instruction.line_number)};
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

                if (!current_stage && instruction.type != BuildFileParser::InstructionType::FROM) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Instruction before first FROM.",
                                                instruction.line_number));
                }

                if (instruction.type == BuildFileParser::InstructionType::FROM) {
                        Stage stage{};
                        std::string expanded_from{expand_args(instruction.shell_form, global_args)};
                        auto parsed{parse_from_as(expanded_from, instruction.line_number)};
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

                if (current_stage && instruction.type == BuildFileParser::InstructionType::ARG) {
                        auto parsed_args{parse_arg(instruction.shell_form, instruction.line_number)};
                        auto it{build_context.find(parsed_args.first)};
                        if (it != build_context.end()) {
                                parsed_args.second = it->second;
                        }
                        else if (!parsed_args.second.has_value()) {
                                parsed_args.second = "";
                        }
                        current_stage->local_args[parsed_args.first] = parsed_args.second.value();
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
                if (key_value.first.empty()) {
                        throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Empty Key.",
                                                line_number));
                }
                key_value.second = std::nullopt;
                return key_value;
        }
        key_value.first = line.substr(0, eq_index);
        std::string value{line.substr(eq_index+1)};
        BuildFileParser::trim(key_value.first);
        BuildFileParser::trim(value);
        key_value.second = value;
        if (key_value.first.empty()) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Empty Key.",
                                        line_number));
        }
        return key_value;
}

[[nodiscard]] auto GraphBuilder::parse_from_as(const std::string& line, std::uint32_t line_number) -> std::pair<std::string, std::optional<std::string>> {
        std::stringstream iss{line};
        std::string base_image{};
        if (!(iss >> base_image)) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Missing base image.",
                                        line_number));
        }
        std::string transformed_base{base_image};
        std::transform(transformed_base.begin(), transformed_base.end(), transformed_base.begin(),
                        [](unsigned char c) -> char {
                                return static_cast<char>(std::toupper(c));
                        });
        if (transformed_base == "AS") {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Missing base image.",
                                        line_number));
        }
        std::string maybe_as{};
        if (!(iss >> maybe_as)) {
                return std::make_pair(base_image, std::nullopt);
        }

        std::string transformed_as{maybe_as};
        std::transform(transformed_as.begin(), transformed_as.end(), transformed_as.begin(),
                        [](unsigned char c) -> char {
                                return static_cast<char>(std::toupper(c));
                        });
        if (transformed_as != "AS") {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: More than one base image provided.",
                                        line_number));
        }
        std::string stage_name{};
        if (!(iss >> stage_name)) {
                throw std::runtime_error(std::format("Graph Builder Error [Line {}]: Missing stage alias after AS.",
                                        line_number));
        }
        return std::make_pair(base_image, stage_name);
}
