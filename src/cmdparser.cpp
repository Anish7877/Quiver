#include "cmdparser.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <algorithm> // For std::transform to convert to lowercase

namespace CommandParser {

// --- Parser Configuration ---
const std::string PATH_IDENTIFIER = "<PATH>";
const std::string VALUE_IDENTIFIER = "<VALUE>";

static const std::map<std::string, std::string> keyword_to_key_map = {
    {"quiver", key_to_string_map.at(CommandKey::QUIVER)},
    {"run", key_to_string_map.at(CommandKey::RUN)},
    {"open", key_to_string_map.at(CommandKey::OPEN)},
    {"delete", key_to_string_map.at(CommandKey::DELETE)},
    {"create", key_to_string_map.at(CommandKey::CREATE)},
    {"file", key_to_string_map.at(CommandKey::FILE)},
    {"-v", key_to_string_map.at(CommandKey::VOLUME_FLAG)},
    {":", key_to_string_map.at(CommandKey::COLON)},
    {"-t", key_to_string_map.at(CommandKey::TYPE_FLAG)},
    {"temporary", key_to_string_map.at(CommandKey::TEMPORARY)},
    {"image", key_to_string_map.at(CommandKey::IMAGE)},
    {PATH_IDENTIFIER, key_to_string_map.at(CommandKey::PATH)},
    {VALUE_IDENTIFIER, key_to_string_map.at(CommandKey::VALUE)}
};

/**
 * @brief Defines the valid command structures. The order is strictly enforced.
 *
 *💡MAINTAINER NOTE: The parser uses a "first-match-wins" logic. To avoid ambiguity,
 * always place more specific command templates BEFORE more generic ones.
 * For example, a template matching a specific keyword like 'status' should appear
 * before a template that accepts any generic <VALUE>.
 */
static const std::vector<std::vector<std::string>> command_templates = {
    {"open", "file", PATH_IDENTIFIER},
    {"delete", "file", PATH_IDENTIFIER},
    {"quiver", "run", "-v", PATH_IDENTIFIER, ":", PATH_IDENTIFIER, "-t", VALUE_IDENTIFIER, VALUE_IDENTIFIER}
};

// --- Public Function Implementation ---
std::vector<std::string> parse_command(const std::vector<std::string>& tokens) {
    if (tokens.empty()) {
        Utils::handle_error("Received an empty command token vector.");
    }

    int matched_template_index = -1;

    // 1. Find a matching command template.
    for (size_t i = 0; i < command_templates.size(); ++i) {
        const auto& tpl = command_templates[i];
        if (tpl.size() != tokens.size()) {
            continue;
        }

        bool current_template_matches = true;
        for (size_t j = 0; j < tpl.size(); ++j) {
            const std::string& token_from_user = tokens[j];
            const std::string& token_from_template = tpl[j];

            if (token_from_template == PATH_IDENTIFIER) {
                if (!Utils::path_exists(token_from_user)) {
                    Utils::handle_error("Path does not exist: " + token_from_user);
                }
            } else if (token_from_template == VALUE_IDENTIFIER) {
                continue;
            } else {
                std::string lower_token = token_from_user;
                std::transform(lower_token.begin(), lower_token.end(), lower_token.begin(), ::tolower);
                if (token_from_template != lower_token) {
                    current_template_matches = false;
                    break;
                }
            }
        }

        if (current_template_matches) {
            matched_template_index = static_cast<int>(i);
            break;
        }
    }

    if (matched_template_index == -1) {
        Utils::handle_error("Invalid command. Check syntax, keyword order, and number of arguments.");
    }

    // 3. Generate the output vector of keys.
    std::vector<std::string> result_keys;
    const auto& matched_template = command_templates[matched_template_index];

    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& token_type = matched_template[i];
        
        if (token_type == PATH_IDENTIFIER || token_type == VALUE_IDENTIFIER) {
            result_keys.push_back(keyword_to_key_map.at(token_type));
            result_keys.push_back(tokens[i]);
        } else {
            std::string lower_token = tokens[i];
            std::transform(lower_token.begin(), lower_token.end(), lower_token.begin(), ::tolower);
            
            // 🐛 BUG FIX: Use .find() instead of .at() to prevent crashing.
            auto it = keyword_to_key_map.find(lower_token);
            if (it != keyword_to_key_map.end()) {
                result_keys.push_back(it->second);
            } else {
                // This is a critical internal error, meaning a keyword in a template
                // is missing from the keyword-to-key map.
                Utils::handle_error("Internal parser misconfiguration: No key found for token '" + tokens[i] + "'");
            }
        }
    }

    return result_keys;
}

} // namespace CommandParser