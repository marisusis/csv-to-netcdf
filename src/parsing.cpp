#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <numeric>
#include <expected>

#include "spdlog/spdlog.h"

#include "parsing.hpp"

std::expected<Line_v2, std::string> parse_line_v2(const std::string& line) {
    std::string token;
    std::istringstream tokenStream(line);

    auto label_it = CaptureSchemaV2.labels.begin();
    auto type_it = CaptureSchemaV2.types.begin();

    // Read normal tokens
    if (!std::getline(tokenStream, token, ',').good()) {
        return std::unexpected("failed to read gps_time");
    }
    double gps_time = std::stod(token);

    if (!std::getline(tokenStream, token, ',').good()) {
        return std::unexpected("failed to read flags");
    }
    std::string flags = token;

    if (!std::getline(tokenStream, token, ',').good()) {
        return std::unexpected("failed to read sample_rate");
    }
    double sample_rate = std::stod(token);

    if (!std::getline(tokenStream, token, ',').good()) {
        return std::unexpected("failed to read latitude");
    }
    double latitude = std::stod(token);

    if (!std::getline(tokenStream, token, ',').good()) {
        return std::unexpected("failed to read longitude");
    }
    double longitude = std::stod(token);

    if (!std::getline(tokenStream, token, ',').good()) {
        return std::unexpected("failed to read elevation");
    }
    double elevation = std::stod(token);

    if (!std::getline(tokenStream, token, ',').good()) {
        return std::unexpected("failed to read satellite_count");
    }
    int satellite_count = std::stoi(token);

    if (!std::getline(tokenStream, token, ',').good()) {
        return std::unexpected("failed to read speed");
    }
    double speed = std::stod(token);

    if (!std::getline(tokenStream, token, ',').good()) {
        return std::unexpected("failed to read heading");
    }
    double heading = std::stod(token);

    if (!std::getline(tokenStream, token, ',').good()) {
        return std::unexpected("failed to read count_samples");
    }
    int count_samples = std::stoi(token);

    // Read data
    std::vector<uint32_t> tokens;
    while (std::getline(tokenStream, token, ',')) {
        tokens.push_back(std::stoi(token));
    }

    auto checksum = tokens.back();
    tokens.pop_back();

    auto sum = std::accumulate(tokens.begin(), tokens.end(), 0);

    if (sum != checksum) {
        return std::unexpected("Checksum failed");
    }

    return Line_v2 {
        .gps_time = gps_time,
        .flags = flags,
        .sample_rate = sample_rate,
        .latitude = latitude,
        .longitude = longitude,
        .elevation = elevation,
        .satellite_count = satellite_count,
        .speed = speed,
        .heading = heading,
        .count_samples = count_samples,
        .samples = std::vector<uint16_t>(tokens.size())
    };
}