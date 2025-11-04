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
#include "spdlog/spdlog.h"

struct ColumnSchema {
    std::string label;
    std::string unit;
    uint32_t netcdf_type;
};


class CaptureSchema2 {
public:
    const std::vector<ColumnSchema> columns;
};;

const CaptureSchema2 v1_schema = {
    .columns = {
        ColumnSchema{
            .label = "computer_time",
            .unit = "s",
            .netcdf_type = NC_DOUBLE
        },
        ColumnSchema{
            .label = "samples",
            .unit = "",
            .netcdf_type = NC_USHORT
        }
    }
};

const CaptureSchema2 v2_schema = {
    .columns = {
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
        // ColumnSchema{
        //     .label = "count_samples",
        //     .unit = "",
        //     .netcdf_type = NC_INT
        // },
        ColumnSchema{
            .label = "samples",
            .unit = "",
            .netcdf_type = NC_USHORT
        }
    }
};

const CaptureSchema2 v3_schema = {
    .columns = {
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
            .label = "count_samples",
            .unit = "",
            .netcdf_type = NC_INT
        },
        ColumnSchema{
            .label = "samples",
            .unit = "",
            .netcdf_type = NC_USHORT
        }
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
        return std::stod(token);
    } else if constexpr (std::is_same_v<T, int> || std::is_same_v<T, uint32_t>) {
        // spdlog::trace("parsing int token: {}", token);
        return static_cast<T>(std::stoi(token));
    } else if constexpr (std::is_same_v<T, uint64_t>) {
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

std::map<std::string, std::any> parse_line_v2(const std::string& line);

std::map<std::string, std::any> parse_line_v3(const std::string& line);


#endif