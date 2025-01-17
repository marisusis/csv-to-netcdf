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
};

const CaptureSchema CaptureSchemaV1 = {
    .regular_column_count =  1,
    .types = {"double", "uint16_t"},
    .netcdf_types = {NC_DOUBLE, NC_USHORT},
    .labels = {"computer_time", "samples"}
};

const CaptureSchema CaptureSchemaV2 = {
    .regular_column_count = 10,
    .types = {"int", "int", "int", "double", "double", "double", "double", "int", "double", "double", "int", "uint16_t", "int"},
    .netcdf_types = {NC_INT, NC_BYTE, NC_BYTE, NC_DOUBLE, NC_DOUBLE, NC_DOUBLE, NC_DOUBLE, NC_INT, NC_DOUBLE, NC_DOUBLE, NC_INT, NC_USHORT, NC_INT},
    .labels = {"gps_time", "has_gps", "clipping", "sample_rate", "latitude", "longitude", "elevation", "satellite_count", "speed", "heading", "count_samples", "samples", "checksum"}
};

const CaptureSchema CaptureSchemaV3 = {
    .regular_column_count = 11,
    .types = {"double", "double", "int", "int", "double", "double", "double", "double", "int", "double", "double", "int", "uint16_t", "int"},
    .netcdf_types = {NC_DOUBLE, NC_INT, NC_BYTE, NC_BYTE, NC_DOUBLE, NC_DOUBLE, NC_DOUBLE, NC_DOUBLE, NC_INT, NC_DOUBLE, NC_DOUBLE, NC_INT, NC_USHORT, NC_INT},
    .labels = {"computer_time", "gps_time", "has_gps", "clipping", "sample_rate", "latitude", "longitude", "elevation", "satellite_count", "speed", "heading", "count_samples", "samples", "checksum"}
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

std::map<std::string, std::string> parse_metadata(std::istream& stream);

Line_v2 parse_line_v2(const std::string& line);
Line_v3 parse_line_v3(const std::string& line);

#endif