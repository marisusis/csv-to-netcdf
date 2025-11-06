#pragma once

#include <cstdint>
#include <istream>
#include <map>
#include <string>
#include "netcdf.h"
// #include "parsing.hpp"

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
    } else if (!metadata.empty() && metadata.find("version") == metadata.end()) {
        return 2;
    } else if (metadata.find("version") != metadata.end()) {
        return std::stoi(metadata["version"]);
    }

    for (const auto& [key, value] : metadata) {
        spdlog::debug("Metadata: {} = {}", key, value);
    }
    
    throw std::runtime_error("Unknown schema version");

}

// by Useless from https://stackoverflow.com/questions/1088622/how-do-i-create-an-array-of-strings-in-c
std::vector<char*> strlist(std::vector<std::string> &input) {
    std::vector<char*> result;

    // remember the nullptr terminator
    result.reserve(input.size()+1);

    std::transform(begin(input), end(input),
                   std::back_inserter(result),
                   [](std::string &s) { return s.data(); }
                  );
    result.push_back(nullptr);
    return result;
}

template<typename T> void nc_put_value(
    const int ncid,
    const int varid,
    const size_t* coord,
    const T& value
) {
    if constexpr (std::is_same_v<T, int>) {
        nc_put_var1_int(ncid, varid, coord, &value);
    } else if constexpr (std::is_same_v<T, double>) {
        nc_put_var1_double(ncid, varid, coord, &value);
    } else if constexpr (std::is_same_v<T, char>) {
        int int_value = static_cast<int>(value);
        nc_put_var1_int(ncid, varid, coord, &int_value);
    } else if constexpr (std::is_same_v<T, short>) {
        nc_put_var1_short(ncid, varid, coord, &value);
    } else if constexpr (std::is_same_v<T, std::string>) {
        nc_put_var1_text(ncid, varid, coord, value.c_str());
    } else if constexpr (std::is_same_v<T, unsigned long long>) {
        nc_put_var1_ulonglong(ncid, varid, coord, &value);
    } else if constexpr (std::is_same_v<T, long long>) {
        nc_put_var1_longlong(ncid, varid, coord, &value);
    } else if constexpr (std::is_same_v<T, unsigned int>) {
        nc_put_var1_uint(ncid, varid, coord, &value);
    } else {
        static_assert(false, "Unsupported type for nc_put_value");
    }
}

template<typename T, size_t dims> void nc_put_value_array(
    const int ncid,
    const int varid,
    const std::array<size_t, dims>& start,
    const std::array<size_t, dims>& count,
    const std::vector<T>& data
) {
    if constexpr (std::is_same_v<T, int>) {
        // nc_put_var1_int(ncid, varid, coord, &value);
    } else if constexpr (std::is_same_v<T, double>) {
        // nc_put_var1_double(ncid, varid, coord, &value);
    } else if constexpr (std::is_same_v<T, char>) {
        // int int_value = static_cast<int>(value);
        // nc_put_var1_int(ncid, varid, coord, &int_value);
    } else if constexpr (std::is_same_v<T, short>) {
        nc_put_vara_short(ncid, varid, start.data(), count.data(), data.data());
    } else if constexpr (std::is_same_v<T, std::string>) {
        // nc_put_var1_text(ncid, varid, coord, value.c_str());
    } else if constexpr (std::is_same_v<T, unsigned long long>) {
        // nc_put_var1_ulonglong(ncid, varid, coord, &value);
    } else if constexpr (std::is_same_v<T, long long>) {
        // nc_put_var1_longlong(ncid, varid, coord, &value);
    } else if constexpr (std::is_same_v<T, unsigned int>) {
        // nc_put_var1_uint(ncid, varid, coord, &value);
    } else {
        static_assert(false, "Unsupported type for nc_put_value");
    }
}