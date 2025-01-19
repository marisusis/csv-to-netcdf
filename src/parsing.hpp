#ifndef PARSING_HPP
#define PARSING_HPP

#include "netcdf.h"

#include <vector>
#include <string>
#include <cstdint>
#include <expected>
#include <any>
#include <tuple>

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
            .netcdf_type = NC_INT
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
            .label = "computer_time",
            .unit = "s",
            .netcdf_type = NC_DOUBLE
        },
        ColumnSchema{
            .label = "gps_time",
            .unit = "s",
            .netcdf_type = NC_INT
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
        return static_cast<T>(std::stoi(token));
    } else if constexpr (std::is_same_v<T, std::string>) {
        return token;
    }
    
    throw std::runtime_error("Invalid type for try_read_token");
}

std::map<std::string, std::string> parse_metadata(std::istream& stream) {
    enum class ReadState {
        Scanning,
        Metadata,
        Data
    };

    std::string line;
    std::map <std::string, std::string> metadata;

    ReadState readState = ReadState::Scanning;
    for (std::string line; std::getline(stream, line);) {

        // each line should be small enough to not be a memory issue since it's less than a second of data
        if (!stream.good()) {
            spdlog::error("Failed to read the first line of the input file");
            exit(EXIT_FAILURE);
        }

        if (line == "## BEGIN METADATA ##") {
            readState = ReadState::Metadata;
            continue;
        }

        if (readState == ReadState::Metadata) {
            if (line.length() <= 1) {
                continue;
            }

            if (line.at(0) != '#') {
                spdlog::debug("Found line missing # in metadata");
                continue;
            }

            line = line.substr(1);

            // Read the line as metadata
            std::regex metadata_regex("\\s*([A-Z_]+)\\s+(.*)");
            std::smatch match;
            if (std::regex_match(line, match, metadata_regex)) {
                std::string key = match[1];
                std::transform(key.begin(), key.end(), key.begin(), ::tolower);
                std::string value = match[2];
                metadata[key] = value;
            }
        }

        if (line.find("END METADATA") != std::string::npos) {
            if (readState != ReadState::Metadata) {
                spdlog::error("Unexpected end of metadata section");
                exit(EXIT_FAILURE);
            }

            readState = ReadState::Data;
            break;
        }
    
    }

    return metadata;
}

std::map<std::string, std::any> parse_line_v2(const std::string& line) {
    std::string token;
    std::istringstream tokenStream(line);

    int gps_time = try_read_token<int, std::string>(tokenStream, "gps_time");
    std::string flags = try_read_token<std::string, std::string>(tokenStream, "flags");
    double sample_rate = try_read_token<double, std::string>(tokenStream, "sample_rate");
    double latitude = try_read_token<double, std::string>(tokenStream, "latitude");
    double longitude = try_read_token<double, std::string>(tokenStream, "longitude");
    double elevation = try_read_token<double, std::string>(tokenStream, "elevation");
    int satellite_count = try_read_token<int, std::string>(tokenStream, "satellite_count");
    double speed = try_read_token<double, std::string>(tokenStream, "speed");
    double heading = try_read_token<double, std::string>(tokenStream, "heading");
    int count_samples = try_read_token<int, std::string>(tokenStream, "count_samples");

    char clipping = flags.find('C') != std::string::npos;
    char has_gps = flags.find('G') != std::string::npos;

    // Read data
    std::vector<int> tokens;
    tokens.reserve(count_samples);
    while (std::getline(tokenStream, token, ',')) {
        tokens.push_back(std::stoi(token));
    }

    auto checksum = tokens.back();
    tokens.pop_back();

    auto sum = std::accumulate(tokens.begin(), tokens.end(), 0);

    if (sum != checksum) {
        throw std::runtime_error("Checksum failed");
    }

    return std::map<std::string, std::any> {
        {"gps_time", gps_time},
        {"has_gps", has_gps},
        {"clipping", clipping},
        {"sample_rate", sample_rate},
        {"latitude", latitude},
        {"longitude", longitude},
        {"elevation", elevation},
        {"satellite_count", satellite_count},
        {"speed", speed},
        {"heading", heading},
        {"count_samples", count_samples},
        {"samples", tokens}
    };

}

std::map<std::string, std::any> parse_line_v3(const std::string& line) {
    std::string token;
    std::istringstream tokenStream(line);

    double computer_time = try_read_token<double, std::string>(tokenStream, "computer_time");
    int gps_time = try_read_token<int, std::string>(tokenStream, "gps_time");
    std::string flags = try_read_token<std::string, std::string>(tokenStream, "flags");
    double sample_rate = try_read_token<double, std::string>(tokenStream, "sample_rate");
    double latitude = try_read_token<double, std::string>(tokenStream, "latitude");
    double longitude = try_read_token<double, std::string>(tokenStream, "longitude");
    double elevation = try_read_token<double, std::string>(tokenStream, "elevation");
    int satellite_count = try_read_token<int, std::string>(tokenStream, "satellite_count");
    double speed = try_read_token<double, std::string>(tokenStream, "speed");
    double heading = try_read_token<double, std::string>(tokenStream, "heading");
    int count_samples = try_read_token<int, std::string>(tokenStream, "count_samples");

    char clipping = flags.find('C') != std::string::npos;
    char has_gps = flags.find('G') != std::string::npos;

    // Read data
    std::vector<int> tokens;
    tokens.reserve(count_samples);
    while (std::getline(tokenStream, token, ',')) {
        tokens.push_back(std::stoi(token));
    }

    auto checksum = tokens.back();
    tokens.pop_back();

    auto sum = std::accumulate(tokens.begin(), tokens.end(), 0);

    if (sum != checksum) {
        throw std::runtime_error("Checksum failed");
    }

    return std::map<std::string, std::any> {
        {"computer_time", computer_time},
        {"gps_time", gps_time},
        {"has_gps", has_gps},
        {"clipping", clipping},
        {"sample_rate", sample_rate},
        {"latitude", latitude},
        {"longitude", longitude},
        {"elevation", elevation},
        {"satellite_count", satellite_count},
        {"speed", speed},
        {"heading", heading},
        {"count_samples", count_samples},
        {"samples", tokens}
    };

}

#endif