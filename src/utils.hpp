#pragma once

#include <cstdint>
#include <istream>

size_t count_data_lines(std::istream& file) {
    size_t lines = 0;
    for (std::string line; std::getline(file, line);) {
        lines++;
    }

    return lines;
}