#pragma once

#include "orchestrator.hpp" // For AppDefinition and ServiceConfig structs.
#include <string>

// A dedicated class for parsing .qivr (YAML) files into C++ structs.
class QivrParser {
public:
    // Default constructor.
    QivrParser() = default;

    // Parses a .qivr file and returns a structured AppDefinition.
    AppDefinition parse(const std::string& file_path);
};