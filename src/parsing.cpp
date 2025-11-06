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
#include <charconv>
#include <cstring>   // added for memchr
#include <algorithm> // added for count


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

// Fast parse function: parse comma-separated integers from a string_view into a vector<int16_t>.
// Uses memchr + std::from_chars to avoid allocations and temporary strings.
static size_t parse_comma_separated_shorts(std::string_view sv, std::vector<int16_t>& out, size_t start, size_t count) {
	// reserve based on comma count (commas + 1 => elements)
    out.clear();
    out.reserve(count);

    size_t parsed_count = 0;
    size_t end = 0;
    while (parsed_count < count) {
        end = sv.find(',', start);
        int value = 0;

        auto res = std::from_chars(sv.data() + start, sv.data() + (end == std::string_view::npos ? sv.size() : end), value);
        if (res.ec != std::errc()) {
            throw std::runtime_error("Failed to parse integer token");
        }
        out.push_back(static_cast<int16_t>(value));
        ++parsed_count;

        if (end == std::string_view::npos) {
            spdlog::error("Reached end of string_view while parsing shorts");
            break;
        }

        start = end + 1;
    }

    return start;
}

ParsedLine parse_line_v2(const std::string& line) {
    std::string_view view(line);
    std::from_chars_result result;

    size_t end = view.find(',');
    unsigned long long gps_time;
    result = std::from_chars(view.data(), view.data() + end, gps_time);
    size_t start = end == std::string_view::npos ? view.size() : end + 1;

    std::string flags;
    end = view.find(',', start);
    flags = std::string(view.substr(start, end - start));
    start = end == std::string_view::npos ? view.size() : end + 1;

    double sample_rate;
    end = view.find(',', start);
    result = std::from_chars(view.data() + start, view.data() + end, sample_rate);
    start = end == std::string_view::npos ? view.size() : end + 1;

    double latitude;
    end = view.find(',', start);
    result = std::from_chars(view.data() + start, view.data() + end, latitude);
    start = end == std::string_view::npos ? view.size() : end + 1;

    double longitude;
    end = view.find(',', start);
    result = std::from_chars(view.data() + start, view.data() + end, longitude);
    start = end == std::string_view::npos ? view.size() : end + 1;

    double elevation;
    end = view.find(',', start);
    result = std::from_chars(view.data() + start, view.data() + end, elevation);
    start = end == std::string_view::npos ? view.size() : end + 1;

    int satellite_count;
    end = view.find(',', start);
    result = std::from_chars(view.data() + start, view.data() + end, satellite_count);
    start = end == std::string_view::npos ? view.size() : end + 1;

    double speed;
    end = view.find(',', start);
    result = std::from_chars(view.data() + start, view.data() + end, speed);
    start = end == std::string_view::npos ? view.size() : end + 1;

    double heading;
    end = view.find(',', start);
    result = std::from_chars(view.data() + start, view.data() + end, heading);
    start = end == std::string_view::npos ? view.size() : end + 1;

    int count_samples;
    end = view.find(',', start);
    result = std::from_chars(view.data() + start, view.data() + end, count_samples);
    start = end == std::string_view::npos ? view.size() : end + 1;

    bool clipping = flags.find('C') != std::string::npos;
    bool has_gps = flags.find('G') != std::string::npos;

    // Parse samples using the zero-allocation helper
    std::vector<int16_t> tokens;
    start = parse_comma_separated_shorts(view, tokens, start, static_cast<size_t>(count_samples));

    // Parse checksum after samples
    uint64_t checksum = 0;
    if (start < view.size()) {
        result = std::from_chars(view.data() + start, view.data() + view.size(), checksum);
    } else {
        spdlog::error("No checksum found after samples");
        throw std::runtime_error("Missing checksum");
    }

    auto sum = std::accumulate(tokens.begin(), tokens.end(), 0);
    if (sum != static_cast<int64_t>(checksum)) {
        spdlog::error("Checksum failed: {} != {}", sum, checksum);
        throw std::runtime_error("Checksum failed");
    }

    return ParsedLine{
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

    std::string_view view(line);
    std::from_chars_result result;

    size_t end = view.find(',');
    double computer_time;
    result = std::from_chars(view.data(), view.data() + end, computer_time);
    size_t start = end + 1;

    unsigned long long gps_time;
    end = view.find(',', start);
    result = std::from_chars(view.data() + start, view.data() + end, gps_time);
    start = end + 1;

    std::string flags;
    end = view.find(',', start);
    flags = std::string(view.substr(start, end - start));
    start = end + 1;

    double sample_rate;
    end = view.find(',', start);
    result = std::from_chars(view.data() + start, view.data() + end, sample_rate);
    start = end + 1;

    double latitude;
    end = view.find(',', start);
    result = std::from_chars(view.data() + start, view.data() + end, latitude);
    start = end + 1;

    double longitude;
    end = view.find(',', start);
    result = std::from_chars(view.data() + start, view.data() + end, longitude);
    start = end + 1;

    double elevation;
    end = view.find(',', start);
    result = std::from_chars(view.data() + start, view.data() + end, elevation);
    start = end + 1;

    int satellite_count;
    end = view.find(',', start);
    result = std::from_chars(view.data() + start, view.data() + end, satellite_count);
    start = end + 1;

    double speed;
    end = view.find(',', start);
    result = std::from_chars(view.data() + start, view.data() + end, speed);
    start = end + 1;

    double heading;
    end = view.find(',', start);
    result = std::from_chars(view.data() + start, view.data() + end, heading);
    start = end + 1;

    int count_samples;
    end = view.find(',', start);
    result = std::from_chars(view.data() + start, view.data() + end, count_samples);
    start = end + 1;

    bool clipping = flags.find('C') != std::string::npos;
    bool has_gps = flags.find('G') != std::string::npos;

    // Read data (fast, zero-allocation parse from the remaining string_view)
    std::vector<int16_t> tokens;
    start = parse_comma_separated_shorts(view, tokens, start, 7200);

    uint64_t checksum;
    result = std::from_chars(view.data() + start, view.data() + view.size(), checksum);

    auto sum = std::accumulate(tokens.begin(), tokens.end(), 0);


    std::ostringstream oss;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << tokens[i];
    }
    // std::cout << "parsed short values: " << oss.str() << std::endl;
    // spdlog::trace("parsed short values: {}", oss.str().sub
    if (sum != checksum) {
        spdlog::error("Checksum failed: {} != {}", sum, checksum);
        throw std::runtime_error("Checksum failed");
        // keep previous behavior: do not throw here (original code commented out throw)
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