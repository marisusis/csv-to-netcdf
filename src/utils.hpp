#pragma once

#include <cstdint>
#include <istream>
#include <map>
#include <string>
#include "parsing.hpp"

size_t count_data_lines(std::istream& file) {
    std::ios::sync_with_stdio(false);
    auto result = std::count(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>(), '\n');
    std::ios::sync_with_stdio(true);
    return result;
}

size_t count_data_lines_fast(std::filesystem::path file_path) {
    std::ostringstream cmd;
    cmd << "wc -l \"" << file_path.string() << "\"";
    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("Failed to run wc -l");
    }

    size_t line_count = 0;
    if (fscanf(pipe, "%zu", &line_count) != 1) {
        pclose(pipe);
        throw std::runtime_error("Failed to parse wc -l output");
    }
    pclose(pipe);

    return line_count;
}

int get_schema_version(std::istream& file) {
    file.clear();
    file.seekg(0, std::ios::beg);

    std::map<std::string, std::string> metadata = parse_metadata(file);

    for (const auto& [key, value] : metadata) {
        spdlog::debug("Metadata: {} = {}", key, value);
    }

    if (metadata.empty()) {
        return 1;
    } else if (metadata.find("gps_time") != metadata.end()) {
        return 2;
    } else if (metadata.find("version") != metadata.end()) {
        return 3;
    }

    for (const auto& [key, value] : metadata) {
        spdlog::debug("Metadata: {} = {}", key, value);
    }
    
    throw std::runtime_error("Unknown schema version");

}

