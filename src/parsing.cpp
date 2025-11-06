#include "parsing.hpp"

#include "netcdf.h"

#include <vector>
#include <sstream>
#include <string>
#include <cstdint>
#include <numeric>
#include <expected>
#include <any>
#include <tuple>
#include <map>


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

ParsedLine parse_line_v2(const std::string& line) {
    std::string token;
    std::istringstream tokenStream(line);

    unsigned long long gps_time = try_read_token<unsigned long long, std::string>(tokenStream, "gps_time");
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

    return ParsedLine {
        .cpu_time = std::nullopt,
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
        .samples = std::vector<int16_t>(tokens.begin(), tokens.end())
    };

}

ParsedLine parse_line_v3(const std::string& line) {
    std::string token;
    std::istringstream tokenStream(line);

    double computer_time = try_read_token<double, std::string>(tokenStream, "cpu_time");
    unsigned long long gps_time = try_read_token<unsigned long long, std::string>(tokenStream, "gps_time");
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

    return ParsedLine{
        .cpu_time = computer_time,
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
        .samples = std::vector<int16_t>(tokens.begin(), tokens.end())
    };

}