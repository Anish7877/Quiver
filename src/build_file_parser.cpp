#include "build_file_parser.hpp"
#include <algorithm>
#include <format>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <string>

[[nodiscard]] auto BuildFileParser::parse(const fs::path& path) -> std::vector<BuildInstruction> {
        std::vector<BuildInstruction> instructions{};

        std::ifstream file{path};
        if (!file.is_open()) [[unlikely]] {
                throw std::runtime_error("Parser Error: Quiverfile not found or doesn't exist.");
        }

        m_current_line_number = 0;
        m_logical_current_line_number = 0;

        parse_parser_directives(file);

        std::string line{};
        while (std::getline(file, line)) {
                ++m_current_line_number;

                line = complete_escape_line(file, line);
                line = strip_comments(line);
                trim(line);
                if (line.empty()) continue;
                ++m_logical_current_line_number;

                BuildInstruction instruction{};
                instruction.line_number = m_current_line_number;
                instruction.raw_instruction = line;

                size_t split_index{line.find_first_of(" \t")};
                std::string instruction_token{};
                std::string opts_line{};

                if (split_index == std::string::npos) {
                        instruction_token = line;
                } else {
                        instruction_token = line.substr(0, split_index);
                        opts_line         = line.substr(split_index + 1);
                        trim(opts_line);
                }

                std::transform(instruction_token.begin(), instruction_token.end(),
                                instruction_token.begin(),
                                [](unsigned char c) -> char {
                                return static_cast<char>(std::toupper(c));
                                });

                auto it{Instruction::INSTRUCTION_STR_TO_TYPE.find(instruction_token)};
                if (it == Instruction::INSTRUCTION_STR_TO_TYPE.end()) {
                        throw std::runtime_error(std::format(
                                                "Parser Error [Line {}]: Unknown instruction '{}'.",
                                                m_current_line_number, instruction_token));
                }
                Instruction::InstructionType type{it->second};
                bool supports_options{
                        type == Instruction::InstructionType::RUN  ||
                                type == Instruction::InstructionType::COPY ||
                                type == Instruction::InstructionType::ADD
                };
                std::vector<Instruction::InstructionOption> opts{};
                std::string args_line{};

                if (supports_options) {
                        opts      = parse_instruction_options(opts_line);
                        args_line = strip_instruction_options(opts_line);
                } else {
                        args_line = opts_line;
                }
                trim(args_line);

                bool supports_json_form{
                        type == Instruction::InstructionType::RUN        ||
                                type == Instruction::InstructionType::CMD        ||
                                type == Instruction::InstructionType::ENTRYPOINT ||
                                type == Instruction::InstructionType::SHELL      ||
                                type == Instruction::InstructionType::ADD        ||
                                type == Instruction::InstructionType::COPY       ||
                                type == Instruction::InstructionType::VOLUME
                };
                bool looks_like_json{
                        supports_json_form &&
                                !args_line.empty() &&
                                args_line.front() == '['
                };

                if (looks_like_json) {
                        if (args_line.back() != ']') {
                                throw std::runtime_error(std::format(
                                                        "Parser Error [Line {}]: Invalid JSON exec form — missing closing ']'.",
                                                        m_current_line_number));
                        }
                        parse_json_form(instruction, args_line);
                } else {
                        if (args_line.find("<<") != std::string::npos) {
                                throw std::runtime_error(std::format(
                                                        "Parser Error [Line {}]: Heredoc syntax is not supported.",
                                                        m_current_line_number));
                        }
                        parse_shell_form(instruction, args_line);
                }

                instruction.type = type;
                instruction.opts = opts;
                instructions.emplace_back(std::move(instruction));
        }

        return instructions;
}

auto BuildFileParser::parse_parser_directives(std::ifstream& file) -> void {
        std::string line{};

        while (true) {
                std::streampos pos{file.tellg()};
                if (!std::getline(file, line)) break;
                ++m_current_line_number;

                trim(line);
                if (line.empty()) continue;

                if (line.front() != '#') {
                        file.seekg(pos);
                        --m_current_line_number;
                        break;
                }

                line.erase(0, 1);
                trim(line);

                if (line.find('=') == std::string::npos) break;

                size_t      index{line.find('=')};
                std::string key{line.substr(0, index)};
                std::string value{line.substr(index + 1)};
                trim(key);
                trim(value);

                // Malformed directive (empty key or value) — end directive phase
                if (key.empty() || value.empty()) break;

                std::transform(key.begin(), key.end(), key.begin(),
                                [](unsigned char c) -> char {
                                return static_cast<char>(std::tolower(c));
                                });

                if (key == "escape") {
                        if (value.size() != 1 || (value[0] != '\\' && value[0] != '`')) {
                                throw std::runtime_error(std::format(
                                                        "Parser Error [Line {}]: Invalid escape directive '{}'. "
                                                        "Only '\\' and '`' are allowed.",
                                                        m_current_line_number, value));
                        }
                        m_parser_directive.escape = value[0];
                }
        }
}

[[nodiscard]] auto BuildFileParser::complete_escape_line(std::ifstream& file, const std::string& line) -> std::string {
        if (line.empty()) return line;

        size_t escape_count{0};
        for (size_t i{line.size()}; i > 0 && line[i - 1] == m_parser_directive.escape; --i) {
                ++escape_count;
        }
        if ((escape_count & 1) == 0) return line;

        std::string result{line.substr(0, line.size() - 1)};
        std::string next_line{};

        while (true) {
                ++m_current_line_number;
                if (!std::getline(file, next_line)) {
                        throw std::runtime_error(std::format(
                                                "Parser Error [Line {}]: Unterminated continuation — "
                                                "reached EOF while expecting more input.",
                                                m_current_line_number));
                }

                trim(next_line);

                if (!result.empty() &&
                                !std::isspace(static_cast<unsigned char>(result.back())) &&
                                !next_line.empty() &&
                                !std::isspace(static_cast<unsigned char>(next_line.front()))) {
                        result += ' ';
                }

                // Check if this new line is itself a continuation
                size_t next_escape_count{0};
                for (size_t i{next_line.size()};
                                i > 0 && next_line[i - 1] == m_parser_directive.escape; --i) {
                        ++next_escape_count;
                }

                if ((next_escape_count & 1) == 1) {
                        result += next_line.substr(0, next_line.size() - 1);
                } else {
                        result += next_line;
                        break;
                }
        }

        return result;
}

[[nodiscard]] auto BuildFileParser::strip_comments(const std::string& line) -> std::string {
        std::string result{};
        bool        in_single_quote{false};
        bool        in_double_quote{false};

        for (size_t i{0}; i < line.size(); ++i) {
                char c{line[i]};

                size_t escape_count{0};
                for (size_t j{i}; j > 0 && line[j - 1] == m_parser_directive.escape; --j) {
                        ++escape_count;
                }
                bool escaped{(escape_count & 1) == 1};

                if      (c == '\'' && !in_double_quote && !escaped) in_single_quote = !in_single_quote;
                else if (c == '"'  && !in_single_quote && !escaped) in_double_quote = !in_double_quote;

                bool is_comment_start{
                        c == '#'         &&
                                !in_single_quote &&
                                !in_double_quote &&
                                !escaped         &&
                                (i == 0 || std::isspace(static_cast<unsigned char>(line[i - 1])))
                };
                if (is_comment_start) break;

                result += c;
        }

        if (in_single_quote || in_double_quote) {
                throw std::runtime_error(std::format(
                                        "Parser Error [Line {}]: Unterminated quote | Raw: {}",
                                        m_current_line_number, line));
        }

        trim(result);
        return result;
}

[[nodiscard]] auto BuildFileParser::parse_instruction_options(const std::string& opts_line) -> std::vector<Instruction::InstructionOption> {
        std::vector<Instruction::InstructionOption> opts{};
        std::stringstream              iss{opts_line};
        std::string                    token{};

        while (iss >> token) {
                if (!token.starts_with("--")) break;
                token.erase(0, 2);

                size_t eq_index{token.find('=')};
                if (eq_index == std::string::npos) {
                        throw std::runtime_error(std::format(
                                                "Parser Error [Line {}]: Invalid option '--{}'. "
                                                "Expected --key=value format.",
                                                m_current_line_number, token));
                }

                Instruction::InstructionOption opt{};
                opt.key   = token.substr(0, eq_index);
                opt.value = token.substr(eq_index + 1);
                trim(opt.key);
                trim(opt.value);

                if (opt.key.empty()) {
                        throw std::runtime_error(std::format(
                                                "Parser Error [Line {}]: Option key is empty.",
                                                m_current_line_number));
                }
                if (opt.value.empty()) {
                        throw std::runtime_error(std::format(
                                                "Parser Error [Line {}]: Option value is empty for key '{}'.",
                                                m_current_line_number, opt.key));
                }

                opts.emplace_back(std::move(opt));
        }

        return opts;
}

[[nodiscard]] auto BuildFileParser::strip_instruction_options(const std::string& opts_line) -> std::string {
        size_t i{0};

        while (i < opts_line.size()) {
                while (i < opts_line.size() &&
                                std::isspace(static_cast<unsigned char>(opts_line[i]))) {
                        ++i;
                }
                if (i < opts_line.size()     &&
                                opts_line[i] == '-'      &&
                                i + 1 < opts_line.size() &&
                                opts_line[i + 1] == '-') {
                        while (i < opts_line.size() &&
                                        !std::isspace(static_cast<unsigned char>(opts_line[i]))) {
                                ++i;
                        }
                        continue;
                }
                break;
        }

        while (i < opts_line.size() &&
                        std::isspace(static_cast<unsigned char>(opts_line[i]))) {
                ++i;
        }

        std::string payload{};
        bool        in_single_quote{false};
        bool        in_double_quote{false};

        while (i < opts_line.size()) {
                char c{opts_line[i]};
                if      (c == '\'' && !in_double_quote) in_single_quote = !in_single_quote;
                else if (c == '"'  && !in_single_quote) in_double_quote = !in_double_quote;
                payload += c;
                ++i;
        }

        return payload;
}

auto BuildFileParser::parse_shell_form(
                BuildInstruction& instruction, const std::string& args_line) -> void
{
        instruction.is_shell_form = true;
        instruction.shell_form    = args_line;
}

auto BuildFileParser::parse_json_form(BuildInstruction& instruction, const std::string& args_line) -> void {
        instruction.is_json_form = true;
        try {
                nlohmann::json json{nlohmann::json::parse(args_line)};

                if (!json.is_array()) {
                        throw std::runtime_error(std::format(
                                                "Parser Error [Line {}]: JSON exec form must be an array.",
                                                m_current_line_number));
                }

                for (const auto& elem : json) {
                        if (!elem.is_string()) {
                                throw std::runtime_error(std::format(
                                                        "Parser Error [Line {}]: JSON exec form elements must be strings.",
                                                        m_current_line_number));
                        }
                        instruction.json_args.emplace_back(elem.get<std::string>());
                }
        }
        catch (const nlohmann::json::exception& e) {
                throw std::runtime_error(std::format(
                                        "Parser Error [Line {}]: Malformed JSON — {}.",
                                        m_current_line_number, e.what()));
        }
}

auto BuildFileParser::trim(std::string& line) -> void {
        if (line.empty()) return;

        size_t start{0};
        while (start < line.size() &&
                        std::isspace(static_cast<unsigned char>(line[start]))) {
                ++start;
        }
        if (start == line.size()) {
                line.clear();
                return;
        }

        size_t end{line.size() - 1};
        while (end > start &&
                        std::isspace(static_cast<unsigned char>(line[end]))) {
                --end;
        }

        line.erase(end + 1);
        line.erase(0, start);
}
