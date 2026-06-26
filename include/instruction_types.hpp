#pragma once
#include <csignal>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>
namespace fs = std::filesystem;

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
        struct InstructionHash {
                std::string parent_digest{};
                std::string expanded_raw_ins{};
                std::string source_stage{};
                std::string file_checksum{};
                std::string workdir{};
                std::string user{};
                std::string env{};
        };
        struct AddInstruction {
                std::optional<std::pair<uid_t, gid_t>> chown{std::nullopt};
                std::optional<mode_t> chmod{std::nullopt};
                std::vector<std::string> urls{};
                std::vector<fs::path> srcs{};
                fs::path dst{};
        };
        struct CopyInstruction {
                bool is_dependency{false};
                std::optional<std::string> from_stage{};
                std::optional<std::pair<uid_t, gid_t>> chown{std::nullopt};
                std::optional<mode_t> chmod{std::nullopt};
                std::vector<fs::path> srcs{};
                fs::path dst{};
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
        struct UserInstruction {
                std::optional<uid_t> uid{};
                std::optional<gid_t> gid{};
        };
        struct WorkdirInstruction {
                fs::path workdir{};
        };
        const std::unordered_map<std::string, InstructionType> INSTRUCTION_STR_TO_TYPE{
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
        const std::unordered_map<std::string, int> SIGNAL_STR_TO_MASK{
                {"SIGHUP", SIGHUP},   {"SIGINT", SIGINT},       {"SIGQUIT", SIGQUIT},
                {"SIGILL", SIGILL},   {"SIGTRAP", SIGTRAP},     {"SIGABRT", SIGABRT},
                {"SIGIOT", SIGIOT},   {"SIGBUS", SIGBUS},       {"SIGFPE", SIGFPE},
                {"SIGKILL", SIGKILL}, {"SIGUSR1", SIGUSR1},     {"SIGSEGV", SIGSEGV},
                {"SIGUSR2", SIGUSR2}, {"SIGPIPE", SIGPIPE},     {"SIGALRM", SIGALRM},
                {"SIGTERM", SIGTERM}, {"SIGSTKFLT", SIGSTKFLT}, {"SIGCHLD", SIGCHLD},
                {"SIGCONT", SIGCONT}, {"SIGSTOP", SIGSTOP},     {"SIGTSTP", SIGTSTP},
                {"SIGTTIN", SIGTTIN}, {"SIGTTOU", SIGTTOU},     {"SIGURG", SIGURG},
                {"SIGXCPU", SIGXCPU}, {"SIGXFSZ", SIGXFSZ},     {"SIGVTALRM", SIGVTALRM},
                {"SIGPROF", SIGPROF}, {"SIGWINCH", SIGWINCH},   {"SIGIO", SIGIO},
                {"SIGPOLL", SIGPOLL}, {"SIGPWR", SIGPWR},       {"SIGSYS", SIGSYS}
        };
}
