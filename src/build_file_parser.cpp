#include "build_file_parser.hpp"
#include <format>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
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
                bool is_heredoc_line{false};
                {
                        std::string peek{line};
                        trim(peek);
                        size_t sp{peek.find_first_of(" \t")};
                        if (sp != std::string::npos) {
                                std::string kw{peek.substr(0, sp)};
                                std::transform(kw.begin(), kw.end(), kw.begin(),
                                                [](unsigned char c) -> char {
                                                        return static_cast<char>(std::toupper(c));
                                                });
                                std::string rest{peek.substr(sp + 1)};
                                trim(rest);
                                while (rest.starts_with("--")) {
                                        size_t end{rest.find_first_of(" \t")};
                                        if (end == std::string::npos) {
                                                rest.clear();
                                                break;
                                        }
                                        rest = rest.substr(end + 1);
                                        trim(rest);
                                }
                                if (kw == "RUN" && rest.starts_with("<<")) {
                                        is_heredoc_line = true;
                                }
                        }
                }
                if (!is_heredoc_line) {
                        line = complete_escape_line(file, line);
                }
                line = strip_comments(line);
                trim(line);
                if (line.empty()) {
                        continue;
                }
                ++m_logical_current_line_number;

                BuildInstruction instruction{};
                instruction.line_number = m_current_line_number;
                size_t split_index{line.find_first_of(" \t")};

                std::string instruction_token{};
                std::string opts_line{};
                std::string args_line{};
                if (split_index == std::string::npos) {
                        instruction_token = line;
                }
                else {
                        instruction_token = line.substr(0, split_index);
                        opts_line = line.substr(split_index + 1);
                        trim(opts_line);
                }

                std::transform(instruction_token.begin(), instruction_token.end(), instruction_token.begin(),
                                [](unsigned char c) -> char {
                                        return static_cast<char>(std::toupper(c));
                                });

                auto it{INSTRUCTION_STR_TO_TYPE.find(instruction_token)};

                if (it == INSTRUCTION_STR_TO_TYPE.end()) {
                        throw std::runtime_error(std::format("Parser Error [Line {}]: Unknown instruction '{}'.",
                                                m_current_line_number, instruction_token));
                }

                InstructionType type{it->second};
                bool supports_instruction_options{
                        type == InstructionType::RUN ||
                        type == InstructionType::COPY ||
                        type == InstructionType::ADD ||
                        type == InstructionType::FROM
                };

                std::vector<InstructionOption> opts{};
                if (supports_instruction_options) {
                        opts = parse_instruction_options(opts_line);
                        args_line = strip_instruction_options(opts_line);
                } else {
                        args_line = opts_line;
                }
                trim(args_line);

                if (type == InstructionType::ONBUILD) {
                        instruction.type = type;
                        instruction.opts = opts;
                        instruction.raw_payload = args_line;
                        instruction.is_shell_form = true;
                        instruction.shell_form = args_line;
                        instruction.onbuild_inner = parse_onbuild_inner(args_line);
                        instructions.emplace_back(std::move(instruction));
                        continue;
                }

                bool supports_exec_form{
                        type == InstructionType::RUN ||
                        type == InstructionType::CMD ||
                        type == InstructionType::ENTRYPOINT ||
                        type == InstructionType::SHELL
                };
                bool looks_like_json{
                        supports_exec_form &&
                        !args_line.empty() &&
                        args_line.front() == '['
                };
                if (!looks_like_json) {
                        if (type == InstructionType::RUN) {
                                instruction.heredoc = parse_heredocs(file, args_line);
                        }
                }

                if (!instruction.heredoc.has_value()) {
                        if (looks_like_json) {
                                if (args_line.back() != ']') {
                                        throw std::runtime_error(std::format("Parser Error [Line {}]: Invalid JSON exec form.",
                                                                m_current_line_number));
                                }
                                parse_json_form(instruction, args_line);
                        }
                        else {
                                parse_shell_form(instruction, args_line);
                        }
                }
                else {
                        instruction.is_shell_form = true;
                        instruction.shell_form = args_line;
                        instruction.raw_payload = args_line;
                }
                instruction.type = type;
                instruction.opts = opts;
                instructions.emplace_back(std::move(instruction));
        }
        return instructions;
}

auto BuildFileParser::parse_parser_directives(std::ifstream& file) -> void {
        std::string line{};
        bool directive_phase_active{true};
        while (directive_phase_active) {
                std::streampos pos{file.tellg()};
                if (!std::getline(file, line)) {
                        break;
                }
                ++m_current_line_number;
                trim(line);

                if (line.empty()) {
                        continue;
                }
                if (line.front() != '#') {
                        file.seekg(pos);
                        --m_current_line_number;
                        break;
                }
                line.erase(0, 1);
                trim(line);
                if (line.find('=') == std::string::npos) {
                        directive_phase_active = false;
                        break;
                }
                size_t index{line.find('=')};
                std::string key{line.substr(0, index)};
                std::string value{line.substr(index + 1)};

                trim(key);
                trim(value);

                if (key.empty() || value.empty()) {
                        directive_phase_active = false;
                        continue;
                }

                std::transform(key.begin(), key.end(), key.begin(),
                                [](unsigned char c) -> char {
                                        return static_cast<char>(std::tolower(c));
                                });
                if (key == "escape") {
                        if (value.size() != 1 || (value[0] != '\\' && value[0] != '`')) {
                                throw std::runtime_error(std::format("Parser Error [Line {}]: Invalid escape directive '{}'.",
                                                        m_current_line_number, value));
                        }
                        m_parser_directive.escape = value[0];
                }

        }
}

[[nodiscard]] auto BuildFileParser::strip_comments(const std::string& line) -> std::string {
        std::string result{};

        bool in_single_quote{false};
        bool in_double_quote{false};

        for (size_t i{0}; i < line.size(); ++i) {
                char c{line[i]};
                bool escaped{false};
                size_t escape_count{0};
                for (size_t j{i}; j > 0 && line[j - 1] == m_parser_directive.escape; --j) {
                        escape_count++;
                }

                escaped = ((escape_count & 1));
                if (c == '\'' && !in_double_quote && !escaped) {
                        in_single_quote = !in_single_quote;
                }
                else if (c == '"' && !in_single_quote && !escaped) {
                        in_double_quote = !in_double_quote;
                }

                bool is_comment_start{c == '#' && !in_single_quote && !in_double_quote
                                      && (i == 0 || std::isspace(static_cast<unsigned char>(line[i - 1])))};
                if (is_comment_start) {
                        break;
                }

                result += c;
        }
        if (in_single_quote || in_double_quote) {
                throw std::runtime_error(std::format("Parser Error [Line{}]: Unterminated quote | Raw: {}", m_current_line_number, line));
        }
        trim(result);
        return result;
}

[[nodiscard]] auto BuildFileParser::strip_instruction_options(const std::string& opts_line) -> std::string {
        size_t i{0};
        while (i < opts_line.size()) {
                while (i < opts_line.size() && std::isspace(static_cast<unsigned char>(opts_line[i]))) {
                        ++i;
                }
                if (i < opts_line.size() && opts_line[i] == '-' && i + 1 < opts_line.size() && opts_line[i + 1] == '-') {
                        while (i < opts_line.size() && !std::isspace(static_cast<unsigned char>(opts_line[i]))) {
                                ++i;
                        }
                        continue;
                }
                break;
        }
        while (i < opts_line.size() && std::isspace(static_cast<unsigned char>(opts_line[i]))) {
                ++i;
        }
        std::string payload{};
        bool in_single_quote{false};
        bool in_double_quote{false};
        while (i < opts_line.size()) {
                char c{opts_line[i]};
                if (c == '\'' && !in_double_quote) {
                        in_single_quote = !in_single_quote;
                } else if (c == '"' && !in_single_quote) {
                        in_double_quote = !in_double_quote;
                }
                payload += c;
                ++i;
        }
        return payload;
}

[[nodiscard]] auto BuildFileParser::complete_escape_line(std::ifstream& file, const std::string& line) -> std::string {
        if (line.empty()) {
                return line;
        }
        size_t initial_escape_count{0};
        for (size_t i{line.size()}; i > 0 && line[i - 1] == m_parser_directive.escape; --i) {
                ++initial_escape_count;
        }
        bool initial_continuation{(initial_escape_count & 1) == 1};
        if (!initial_continuation) {
                return line;
        }
        std::string result_line{line.substr(0, line.size() - 1)};
        std::string new_line{};

        while (true) {
                ++m_current_line_number;
                if (!std::getline(file, new_line)) {
                        throw std::runtime_error(std::format("Parser Error [Line {}]: Unterminated continuation.",
                                                m_current_line_number));
                }

                trim(new_line);
                if (!result_line.empty() && !std::isspace(static_cast<unsigned char>(result_line.back())) && !new_line.empty() &&
                    !std::isspace(static_cast<unsigned char>(new_line.front()))) {
                        result_line += ' ';
                }
                size_t escape_count{0};
                for (size_t i{new_line.size()}; i>0 && new_line[i-1] == m_parser_directive.escape; --i) {
                        ++escape_count;
                }
                bool is_continuation{(escape_count&1) == 1};
                if (is_continuation) {
                        result_line += new_line.substr(0, new_line.size() - 1);
                }
                else {
                        result_line += new_line;
                        break;
                }
        }
        return result_line;
}

[[nodiscard]] auto BuildFileParser::parse_instruction_options(const std::string& opts_line) -> std::vector<InstructionOption> {
        std::vector<InstructionOption> opts{};
        std::stringstream iss{opts_line};
        std::string token{};
        while (iss >> token) {
                if (!token.starts_with("--")) {
                        break;
                }
                token.erase(0,2);
                InstructionOption opt{};
                size_t eq_index{token.find('=')};
                if (eq_index == std::string::npos) {
                        throw std::runtime_error(std::format("Parser Error [Line {}]: Invalid option syntax '--{}'. Expected --key=value format.",
                                                m_current_line_number, token));
                }
                else {
                        std::string key{token.substr(0, eq_index)};
                        std::string value{token.substr(eq_index+1)};
                        opt.key = key;
                        opt.value = value;
                        trim(opt.key);
                        trim(opt.value);
                        if (opt.key.empty()) {
                                throw std::runtime_error(std::format("Parser Error [Line {}]: Key is empty.",
                                                        m_current_line_number));
                        }
                        opts.emplace_back(std::move(opt));
                }
        }
        return opts;
}

[[nodiscard]] auto BuildFileParser::parse_heredocs(std::ifstream& file, const std::string& line) -> std::optional<Heredoc> {
        Heredoc heredoc{};
        if (!line.starts_with("<<")) {
                return std::nullopt;
        }
        heredoc.delimiter = line.substr(2);
        trim(heredoc.delimiter);
        if (heredoc.delimiter.empty()) {
                throw std::runtime_error(std::format("Parser Error [Line {}]: Empty heredoc delimiter.",
                                        m_current_line_number));
        }
        if (heredoc.delimiter.find(' ') != std::string::npos) {
                throw std::runtime_error(std::format("Parser Error [Line {}]: Advanced heredoc syntax unsupported.",
                                        m_current_line_number));
        }
        std::string heredoc_line{};
        while (true) {
                if (!std::getline(file, heredoc_line)) {
                        throw std::runtime_error(std::format("Parser Error [Line {}]: Unterminated heredoc.",
                                                m_current_line_number));
                }
                ++m_current_line_number;
                std::string compare_line{heredoc_line};
                trim(compare_line);
                if (compare_line == heredoc.delimiter) {
                        break;
                }
                heredoc.content.emplace_back(heredoc_line);
        }
        return heredoc;
}

auto BuildFileParser::parse_shell_form(BuildInstruction& instruction, const std::string& args_line) -> void {
        instruction.is_shell_form = true;
        instruction.raw_payload = args_line;
        instruction.shell_form = args_line;
}

auto BuildFileParser::parse_json_form(BuildInstruction& instruction, const std::string& args_line) -> void {
        instruction.is_json_form = true;
        instruction.raw_payload = args_line;
        try {
                nlohmann::json json = nlohmann::json::parse(args_line);
                if (!json.is_array()) {
                        throw std::runtime_error(std::format("Parser Error [Line {}] : Malformed json exec form must be array.",
                                                m_current_line_number));
                }
                for(const auto& elem : json) {
                        if (!elem.is_string()) {
                                throw std::runtime_error(std::format("Parser Error [Line {}] : Malformed json exec form elements must be string.",
                                                        m_current_line_number));
                        }
                        instruction.json_args.emplace_back(elem.get<std::string>());
                }
        }
        catch (const nlohmann::json::exception& e) {
                throw std::runtime_error(std::format("Parser Error [Line {}]: Unexpected error '{}'.",
                                        m_current_line_number, e.what()));
        }
}

[[nodiscard]] auto BuildFileParser::parse_onbuild_inner(const std::string& args_line) -> std::shared_ptr<BuildInstruction> {
        size_t split{args_line.find_first_of(" \t")};
        std::string token{};
        std::string opts_line{};
        std::string inner_args_line{};
        if (split == std::string::npos) {
                token = args_line;
        } else {
                token = args_line.substr(0, split);
                opts_line = args_line.substr(split + 1);
                trim(opts_line);
        }
        std::transform(token.begin(), token.end(), token.begin(),
                        [](unsigned char c) -> char {
                                return static_cast<char>(std::toupper(c));
                        });
        auto it{INSTRUCTION_STR_TO_TYPE.find(token)};
        if (it == INSTRUCTION_STR_TO_TYPE.end()) {
                throw std::runtime_error(std::format("Parser Error [Line {}]: Unknown instruction '{}' inside ONBUILD.",
                                        m_current_line_number, token));
        }
        InstructionType type{it->second};
        auto inner = std::make_shared<BuildInstruction>();
        inner->type = type;
        inner->line_number = m_current_line_number;

        bool supports_opts{
                type == InstructionType::RUN ||
                type == InstructionType::COPY ||
                type == InstructionType::ADD ||
                type == InstructionType::FROM
        };
        if (supports_opts) {
                inner->opts = parse_instruction_options(opts_line);
                inner_args_line = strip_instruction_options(opts_line);
        } else {
                inner_args_line = opts_line;
        }
        trim(inner_args_line);

        bool supports_exec{
                type == InstructionType::RUN ||
                type == InstructionType::CMD ||
                type == InstructionType::ENTRYPOINT ||
                type == InstructionType::SHELL
        };
        bool looks_json{
                supports_exec &&
                !inner_args_line.empty() &&
                inner_args_line.front() == '['
        };
        if (looks_json) {
                if (inner_args_line.back() != ']') {
                        throw std::runtime_error(std::format("Parser Error [Line {}]: Invalid JSON exec form.",
                                        m_current_line_number));
                }
                parse_json_form(*inner, inner_args_line);
        } else {
                parse_shell_form(*inner, inner_args_line);
        }
        return inner;
}

auto BuildFileParser::trim(std::string& line) -> void {
        if (line.empty()) {
                return;
        }
        size_t start{0};
        while (start < line.size() && std::isspace(static_cast<unsigned char>(line[start]))) {
                ++start;
        }
        if (start == line.size()) {
                line.clear();
                return;
        }
        size_t end{line.size() - 1};
        while (end > start && std::isspace(static_cast<unsigned char>(line[end]))) {
                --end;
        }
        line.erase(end + 1);
        line.erase(0, start);
}
