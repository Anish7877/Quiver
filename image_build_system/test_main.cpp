#include "build_file_parser.hpp"

#include <format>
#include <iostream>
#include <unordered_map>

const std::unordered_map<
        BuildFileParser::InstructionType,
        std::string> INSTRUCTION_TYPE_TO_STR {

        {BuildFileParser::InstructionType::FROM, "FROM"},
        {BuildFileParser::InstructionType::RUN, "RUN"},
        {BuildFileParser::InstructionType::COPY, "COPY"},
        {BuildFileParser::InstructionType::ADD, "ADD"},
        {BuildFileParser::InstructionType::ENV, "ENV"},
        {BuildFileParser::InstructionType::ARG, "ARG"},
        {BuildFileParser::InstructionType::CMD, "CMD"},
        {BuildFileParser::InstructionType::ENTRYPOINT, "ENTRYPOINT"},
        {BuildFileParser::InstructionType::EXPOSE, "EXPOSE"},
        {BuildFileParser::InstructionType::LABEL, "LABEL"},
        {BuildFileParser::InstructionType::USER, "USER"},
        {BuildFileParser::InstructionType::WORKDIR, "WORKDIR"},
        {BuildFileParser::InstructionType::VOLUME, "VOLUME"},
        {BuildFileParser::InstructionType::SHELL, "SHELL"},
        {BuildFileParser::InstructionType::ONBUILD, "ONBUILD"},
        {BuildFileParser::InstructionType::STOPSIGNAL, "STOPSIGNAL"},
        {BuildFileParser::InstructionType::HEALTHCHECK, "HEALTHCHECK"},
        {BuildFileParser::InstructionType::MAINTAINER, "MAINTAINER"}
};

auto print_instruction(
        const BuildFileParser::BuildInstruction& instruction,
        int indent = 0) -> void {

        std::string padding(indent, ' ');

        auto it{
                INSTRUCTION_TYPE_TO_STR.find(
                        instruction.type
                )
        };

        std::string instruction_name{
                it != INSTRUCTION_TYPE_TO_STR.end()
                ? it->second
                : "UNKNOWN"
        };

        std::cout << std::format(
                "{}[{}] {}\n",
                padding,
                instruction.line_number,
                instruction_name
        );

        if (!instruction.opts.empty()) {

                std::cout << padding
                          << "  OPTIONS:\n";

                for (const auto& opt : instruction.opts) {

                        std::cout << std::format(
                                "{}    --{}={}\n",
                                padding,
                                opt.key,
                                opt.value
                        );
                }
        }

        if (instruction.is_shell_form) {

                std::cout << std::format(
                        "{}  SHELL FORM: {}\n",
                        padding,
                        instruction.shell_form
                );
        }

        if (instruction.is_json_form) {

                std::cout << padding
                          << "  JSON ARGS:\n";

                for (const auto& arg :
                     instruction.json_args) {

                        std::cout << std::format(
                                "{}    {}\n",
                                padding,
                                arg
                        );
                }
        }

        if (instruction.heredoc.has_value()) {

                std::cout << std::format(
                        "{}  HEREDOC DELIMITER: {}\n",
                        padding,
                        instruction.heredoc->delimiter
                );

                std::cout << padding
                          << "  HEREDOC CONTENT:\n";

                for (const auto& line :
                     instruction.heredoc->content) {

                        std::cout << std::format(
                                "{}    {}\n",
                                padding,
                                line
                        );
                }
        }

        if (instruction.onbuild_inner) {

                std::cout << padding
                          << "  ONBUILD INNER:\n";

                print_instruction(
                        *instruction.onbuild_inner,
                        indent + 4
                );
        }

        std::cout << std::format(
                "{}  RAW PAYLOAD: {}\n",
                padding,
                instruction.raw_payload
        );

        std::cout << '\n';
}

auto main() -> int {

        try {

                fs::path path{"./Quiverfile"};

                BuildFileParser parser{};

                auto parsed{parser.parse(path)};

                std::cout << std::format(
                        "Parsed instructions: {}\n\n",
                        parsed.size()
                );

                for (const auto& instruction :
                     parsed) {

                        print_instruction(
                                instruction
                        );
                }
        }
        catch (const std::exception& e) {

                std::cerr << std::format(
                        "Parser Error: {}\n",
                        e.what()
                );

                return 1;
        }

        return 0;
}
