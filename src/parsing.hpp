#ifndef PARSING_HPP
#define PARSING_HPP

#include "netcdf.h"

#include <vector>
#include <string>
#include <cstdint>
#include <expected>
#include <any>
#include <tuple>
#include <map>
#include <regex>
#include <optional>
#include "spdlog/spdlog.h"

enum SchemaVersion {
    Schema_V1 = 1,
    Schema_V2 = 2,
    Schema_V3 = 3
};

struct ColumnSchema {
    std::string label;
    std::string unit;
    uint32_t netcdf_type;
};

struct ParsedLine {
    std::optional<double> cpu_time;
    std::optional<uint64_t> gps_time;
    bool has_gps;
    bool clipping;
    double sample_rate;
    double latitude;
    double longitude;
    double elevation;
    int32_t satellite_count;
    double speed;
    double heading;
    std::vector<int16_t> samples;
};

const std::array<ColumnSchema, 13> columns = {
    ColumnSchema{
        .label = "cpu_time",
        .unit = "s",
        .netcdf_type = NC_DOUBLE
    },
    ColumnSchema{
        .label = "gps_time",
        .unit = "s",
        .netcdf_type = NC_UINT64
    },
    ColumnSchema{
        .label = "has_gps",
        .unit = "",
        .netcdf_type = NC_BYTE
    },
    ColumnSchema{
        .label = "clipping",
        .unit = "",
        .netcdf_type = NC_BYTE
    },
    ColumnSchema{
        .label = "sample_rate",
        .unit = "Hz",
        .netcdf_type = NC_DOUBLE
    },
    ColumnSchema{
        .label = "latitude",
        .unit = "degrees",
        .netcdf_type = NC_DOUBLE
    },
    ColumnSchema{
        .label = "longitude",
        .unit = "degrees",
        .netcdf_type = NC_DOUBLE
    },
    ColumnSchema{
        .label = "elevation",
        .unit = "m",
        .netcdf_type = NC_DOUBLE
    },
    ColumnSchema{
        .label = "satellite_count",
        .unit = "",
        .netcdf_type = NC_INT
    },
    ColumnSchema{
        .label = "speed",
        .unit = "m/s",
        .netcdf_type = NC_DOUBLE
    },
    ColumnSchema{
        .label = "heading",
        .unit = "degrees",
        .netcdf_type = NC_DOUBLE
    },
    ColumnSchema{
        .label = "samples",
        .unit = "",
        .netcdf_type = NC_USHORT
    }
};


template<typename T, typename E>
T try_read_token(std::istringstream& stream, const std::string& token_name) {
    spdlog::trace("reading token: {}", token_name);
    std::string token;
    if (!std::getline(stream, token, ',').good()) {
        if (stream.eof()) {
            throw std::runtime_error("Failed to read token, reason: EOF");
        } else if (stream.fail()) {
            throw std::runtime_error("Failed to read token, reason: stream failed");
        } else if (stream.bad()) {
            throw std::runtime_error("Failed to read token, reason: stream bad");
        }
    }
    
    if constexpr (std::is_same_v<T, double>) {
        spdlog::trace("parsing double token: {}", token);
        return std::stod(token);
    } else if constexpr (std::is_same_v<T, int> || std::is_same_v<T, uint32_t>) {
        // spdlog::trace("parsing int token: {}", token);
        return static_cast<T>(std::stoi(token));
    } else if constexpr (std::is_same_v<T, uint64_t>) {
        // spdlog::trace("parsing uint64 token: {}", token);
        return static_cast<T>(std::stoull(token));
    } else if constexpr (std::is_same_v<T, unsigned long long>) {
        // spdlog::trace("parsing uint64 token: {}", token);
        return static_cast<T>(std::stoull(token));
    } else if constexpr (std::is_same_v<T, int64_t>) {
        // spdlog::trace("parsing int64 token: {}", token);
        return static_cast<T>(std::stoll(token));
    } else if constexpr (std::is_same_v<T, std::string>) {
        // spdlog::trace("parsing string token: {}", token);
        return token;
    }
    
    throw std::runtime_error("Invalid type for try_read_token");
}

std::map<std::string, std::string> parse_metadata(std::istream& stream);
ParsedLine parse_line_v2(const std::string& line);
ParsedLine parse_line_v3(const std::string& line);


#endif