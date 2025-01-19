#ifndef PARSING_HPP
#define PARSING_HPP

#include "netcdf.h"

#include <vector>
#include <string>
#include <cstdint>
#include <expected>

struct CaptureSchema {
    uint8_t regular_column_count;
    std::vector<std::string> types;
    std::vector<uint32_t> netcdf_types;
    std::vector<std::string> labels;
    std::vector<std::string> units;
};

const CaptureSchema CaptureSchemaV1 = {
    .regular_column_count =  1,
    .types = {"double", "uint16_t"},
    .netcdf_types = {NC_DOUBLE, NC_USHORT},
    .labels = {"computer_time", "samples"},
    .units = {"s", ""}
};

const CaptureSchema CaptureSchemaV2 = {
    .regular_column_count = 10,
    .types = {"int", "int", "int", "double", "double", "double", "double", "int", "double", "double", "int", "uint16_t", "int"},
    .netcdf_types = {NC_INT, NC_BYTE, NC_BYTE, NC_DOUBLE, NC_DOUBLE, NC_DOUBLE, NC_DOUBLE, NC_INT, NC_DOUBLE, NC_DOUBLE, NC_INT, NC_USHORT, NC_INT},
    .labels = {"gps_time", "has_gps", "clipping", "sample_rate", "latitude", "longitude", "elevation", "satellite_count", "speed", "heading", "count_samples", "samples", "checksum"},
    .units = {"s", "", "", "Hz", "degrees", "degrees", "m", "", "m/s", "degrees", "", "", ""}
};

const CaptureSchema CaptureSchemaV3 = {
    .regular_column_count = 11,
    .types = {"double", "double", "int", "int", "double", "double", "double", "double", "int", "double", "double", "int", "uint16_t", "int"},
    .netcdf_types = {NC_DOUBLE, NC_INT, NC_BYTE, NC_BYTE, NC_DOUBLE, NC_DOUBLE, NC_DOUBLE, NC_DOUBLE, NC_INT, NC_DOUBLE, NC_DOUBLE, NC_INT, NC_USHORT, NC_INT},
    .labels = {"computer_time", "gps_time", "has_gps", "clipping", "sample_rate", "latitude", "longitude", "elevation", "satellite_count", "speed", "heading", "count_samples", "samples", "checksum"},
    .units = {"s", "s", "", "", "Hz", "degrees", "degrees", "m", "", "m/s", "degrees", "", "", ""}
};

struct Line_v2 {
    int gps_time;
    bool has_gps;
    bool clipping;
    double sample_rate;
    double latitude;
    double longitude;
    double elevation;
    int satellite_count;
    double speed;
    double heading;
    int count_samples;
    std::vector<int> samples;
};

struct Line_v3 {
    double computer_time;
    int gps_time;
    bool has_gps;
    bool clipping;
    double sample_rate;
    double latitude;
    double longitude;
    double elevation;
    int satellite_count;
    double speed;
    double heading;
    int count_samples;
    std::vector<int> samples;
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

Line_v2 parse_line_v2(const std::string& line) {
    std::string token;
    std::istringstream tokenStream(line);

    auto label_it = CaptureSchemaV2.labels.begin();
    auto type_it = CaptureSchemaV2.types.begin();

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

    bool clipping = flags.find('C') != std::string::npos;
    bool has_gps = flags.find('G') != std::string::npos;

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

    return Line_v2 {
        .gps_time = gps_time,
        .has_gps = has_gps,
        .clipping = clipping,
        .sample_rate = sample_rate,
        .latitude = latitude,
        .longitude = longitude,
        .elevation = elevation,
        .satellite_count = satellite_count,
        .speed = speed,
        .heading = heading,
        .count_samples = count_samples,
        .samples = tokens
    };

}

Line_v3 parse_line_v3(const std::string& line) {
    std::string token;
    std::istringstream tokenStream(line);

    auto label_it = CaptureSchemaV3.labels.begin();
    auto type_it = CaptureSchemaV3.types.begin();

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

    bool clipping = flags.find('C') != std::string::npos;
    bool has_gps = flags.find('G') != std::string::npos;

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

    return Line_v3 {
        .computer_time = computer_time,
        .gps_time = gps_time,
        .has_gps = has_gps,
        .clipping = clipping,
        .sample_rate = sample_rate,
        .latitude = latitude,
        .longitude = longitude,
        .elevation = elevation,
        .satellite_count = satellite_count,
        .speed = speed,
        .heading = heading,
        .count_samples = count_samples,
        .samples = tokens
    };

}

#endif