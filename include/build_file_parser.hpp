#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
namespace fs = std::filesystem;

class BuildFileParser {
        public:
                enum class InstructionType {
                        ADD,
                        ARG,
                        CMD,
                        COPY,
                        ENTRYPOINT,
                        ENV,
                        EXPOSE,
                        FROM,
                        HEALTHCHECK,
                        LABEL,
                        MAINTAINER,
                        ONBUILD,
                        RUN,
                        SHELL,
                        STOPSIGNAL,
                        USER,
                        VOLUME,
                        WORKDIR
                };
                struct ParserDirectives {
                        char escape{'\\'};
                };
                struct InstructionOption {
                        std::string key{};
                        std::string value{};
                };
                struct Heredoc {
                        std::vector<std::string> content{};
                        std::string delimiter{};
                };
                struct BuildInstruction {
                        InstructionType type{};
                        std::vector<InstructionOption> opts{};
                        std::optional<Heredoc> heredoc{};
                        std::vector<std::string> json_args{};
                        std::string raw_payload{};
                        std::string shell_form{};
                        std::uint32_t line_number{};
                        std::string stage_name{};
                        bool is_json_form{false};
                        bool is_shell_form{false};
                        std::shared_ptr<BuildInstruction> onbuild_inner{};
                };
                BuildFileParser() = default;
                BuildFileParser(BuildFileParser&&) = delete;
                BuildFileParser(const BuildFileParser&) = delete;
                BuildFileParser &operator=(BuildFileParser&&) = delete;
                BuildFileParser &operator=(const BuildFileParser&) = delete;
                ~BuildFileParser() = default;
                [[nodiscard]] auto parse(const fs::path&) -> std::vector<BuildInstruction>;
                static auto trim(std::string&) -> void;
        private:
                auto parse_parser_directives(std::ifstream&) -> void;
                [[nodiscard]] auto strip_comments(const std::string&) -> std::string;
                [[nodiscard]] auto strip_instruction_options(const std::string&) -> std::string;
                [[nodiscard]] auto complete_escape_line(std::ifstream&, const std::string&) -> std::string;
                [[nodiscard]] auto parse_instruction_options(const std::string&) -> std::vector<InstructionOption>;
                [[nodiscard]] auto parse_heredocs(std::ifstream&, const std::string&) -> std::optional<Heredoc>;
                auto parse_shell_form(BuildInstruction&, const std::string&) -> void;
                auto parse_json_form(BuildInstruction&, const std::string&) -> void;
                [[nodiscard]] auto parse_onbuild_inner(const std::string&) -> std::shared_ptr<BuildInstruction>;

                std::uint32_t m_current_line_number{0};
                std::uint32_t m_logical_current_line_number{0};
                ParserDirectives m_parser_directive{};

                const std::unordered_map<std::string, InstructionType> INSTRUCTION_STR_TO_TYPE {
                        {"ADD", InstructionType::ADD},
                        {"ARG", InstructionType::ARG},
                        {"CMD", InstructionType::CMD},
                        {"COPY", InstructionType::COPY},
                        {"ENTRYPOINT", InstructionType::ENTRYPOINT},
                        {"ENV", InstructionType::ENV},
                        {"EXPOSE", InstructionType::EXPOSE},
                        {"FROM", InstructionType::FROM},
                        {"HEALTHCHECK", InstructionType::HEALTHCHECK},
                        {"LABEL", InstructionType::LABEL},
                        {"MAINTAINER", InstructionType::MAINTAINER},
                        {"ONBUILD", InstructionType::ONBUILD},
                        {"RUN", InstructionType::RUN},
                        {"SHELL", InstructionType::SHELL},
                        {"STOPSIGNAL", InstructionType::STOPSIGNAL},
                        {"USER", InstructionType::USER},
                        {"VOLUME", InstructionType::VOLUME},
                        {"WORKDIR", InstructionType::WORKDIR}
                };
};
