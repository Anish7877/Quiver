#pragma once
#include "instruction_types.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
namespace fs = std::filesystem;

class BuildFileParser {
        public:
                struct ParserDirectives {
                        char escape{'\\'};
                };
                struct BuildInstruction {
                        Instruction::InstructionType type{};
                        std::vector<Instruction::InstructionOption> opts{};
                        std::vector<std::string> json_args{};
                        std::string raw_instruction{};
                        std::string shell_form{};
                        std::uint32_t line_number{};
                        std::string stage_name{};
                        bool is_json_form{false};
                        bool is_shell_form{false};
                };
                BuildFileParser() = default;
                ~BuildFileParser() = default;
                BuildFileParser(BuildFileParser&&) = delete;
                BuildFileParser(const BuildFileParser&) = delete;
                auto operator=(BuildFileParser&&) -> BuildFileParser& = delete;
                auto operator=(const BuildFileParser&) -> BuildFileParser& = delete;
                [[nodiscard]] auto parse(const fs::path&) -> std::vector<BuildInstruction>;
                static auto trim(std::string&) -> void;
        private:
                auto parse_parser_directives(std::ifstream&) -> void;
                [[nodiscard]] auto strip_comments(const std::string&) -> std::string;
                [[nodiscard]] auto strip_instruction_options(const std::string&) -> std::string;
                [[nodiscard]] auto complete_escape_line(std::ifstream&, const std::string&) -> std::string;
                [[nodiscard]] auto parse_instruction_options(const std::string&) -> std::vector<Instruction::InstructionOption>;
                auto parse_shell_form(BuildInstruction&, const std::string&) -> void;
                auto parse_json_form(BuildInstruction&, const std::string&) -> void;

                std::uint32_t m_current_line_number{0};
                std::uint32_t m_logical_current_line_number{0};
                ParserDirectives m_parser_directive{};
};
