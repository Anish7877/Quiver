#pragma once
#include <optional>
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>

namespace Instruction {
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
                ONBUILD,
                RUN,
                SHELL,
                STOPSIGNAL,
                USER,
                VOLUME,
                WORKDIR
        };
        struct InstructionOption {
                std::string key{};
                std::string value{};
        };
        struct Heredoc {
                std::vector<std::string> content{};
                std::string delimiter{};
        };
        struct AddInstruction {
                std::optional<std::string> chown{};
                std::optional<std::string> chmod{};
                std::vector<std::string> srcs{};
                std::string dst{};
        };
        struct CopyInstruction {
                std::optional<std::string> from_stage{};
                std::optional<std::string> chown{};
                std::optional<std::string> chmod{};
                std::vector<std::string> srcs{};
                std::string dst{};
        };
        struct CmdInstruction {
                bool is_json_form{};
                bool is_shell_form{};
                std::vector<std::string> shell_args{};
                std::vector<std::string> json_args{};
        };
        struct EntrypointInstruction {
                bool is_json_form{};
                bool is_shell_form{};
                std::vector<std::string> shell_args{};
                std::vector<std::string> json_args{};
        };
        struct ExposeInstruction {
                std::uint16_t port{};
                std::string protocol{"tcp"};
        };
        struct RunInstruction {
                bool is_json_form{};
                bool is_shell_form{};
                std::vector<std::string> shell_args{};
                std::vector<std::string> json_args{};
        };
        struct ShellInstruction {
                std::vector<std::string> shell_args{};
        };
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
                {"ONBUILD", InstructionType::ONBUILD},
                {"RUN", InstructionType::RUN},
                {"SHELL", InstructionType::SHELL},
                {"STOPSIGNAL", InstructionType::STOPSIGNAL},
                {"USER", InstructionType::USER},
                {"VOLUME", InstructionType::VOLUME},
                {"WORKDIR", InstructionType::WORKDIR}
        };
}
