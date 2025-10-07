#pragma once

#include <string>
#include <vector>
#include <map>
#include "utils.hpp" // For using the Utils namespace functions

/**
 * @brief CommandParser namespace encapsulates all logic for parsing command-line tokens.
 */
namespace CommandParser {

    /**
     * @brief Enum class representing all possible command keywords, options, and value types.
     */
    enum class CommandKey {
        QUIVER,         // "quiver"
        RUN,            // "run"
        OPEN,           // "open"
        DELETE,         // "delete"
        CREATE,         // "create"
        FILE,           // "file"
        VOLUME_FLAG,    // "-v"
        COLON,          // ":"
        TYPE_FLAG,      // "-t"
        TEMPORARY,      // "temporary"
        IMAGE,          // "image"
        PATH,           // Represents a filesystem path argument
        VALUE           // Represents a generic value argument
    };

    /**
     * @brief A map to convert CommandKey enums to their corresponding numeric string representation.
     */
    const std::map<CommandKey, std::string> key_to_string_map = {
        {CommandKey::QUIVER, "100"},
        {CommandKey::RUN, "101"},
        {CommandKey::OPEN, "102"},
        {CommandKey::DELETE, "103"},
        {CommandKey::CREATE, "104"},
        {CommandKey::FILE, "105"},
        {CommandKey::VOLUME_FLAG, "106"},
        {CommandKey::COLON, "107"},
        {CommandKey::TYPE_FLAG, "108"},
        {CommandKey::TEMPORARY, "109"},
        {CommandKey::IMAGE, "110"},
        {CommandKey::PATH, "000"},
        {CommandKey::VALUE, "001"}
    };

    /**
     * @brief Parses a vector of command tokens, validates its structure, and returns a vector of corresponding keys.
     *
     * @param tokens A vector of strings where each element is a part of the command.
     * @return A vector of strings containing the numeric keys for the parsed command.
     * @throws Exits the program via `Utils::handle_error` on any parsing or validation failure.
     * @warning 🛡️ The caller is responsible for sanitizing any path arguments returned by this parser.
     * The parser only validates path existence, not its security implications (e.g., preventing
     * path traversal attacks like '../../etc/passwd').
     */
    std::vector<std::string> parse_command(const std::vector<std::string>& tokens);

} // namespace CommandParser