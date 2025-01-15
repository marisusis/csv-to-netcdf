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

const CaptureSchema CaptureSchemaV3 = {
    .regular_column_count = 11,
    .types = {"double", "double", "string", "double", "double", "double", "double", "int", "double", "double", "int", "uint16_t", "int"},
    .netcdf_types = {NC_DOUBLE, NC_DOUBLE, NC_STRING, NC_DOUBLE, NC_DOUBLE, NC_DOUBLE, NC_DOUBLE, NC_INT, NC_DOUBLE, NC_DOUBLE, NC_INT, NC_USHORT, NC_INT},
    .labels = {"computer_time", "gps_time", "flags", "sample_rate", "latitude", "longitude", "elevation", "satellite_count", "speed", "heading", "count_samples", "samples", "checksum"}
};


const CaptureSchema CaptureSchemaV2 = {
    .regular_column_count = 10,
    .types = {"double", "string", "double", "double", "double", "double", "int", "double", "double", "int", "uint16_t", "int"},
    .netcdf_types = {NC_DOUBLE, NC_STRING, NC_DOUBLE, NC_DOUBLE, NC_DOUBLE, NC_DOUBLE, NC_INT, NC_DOUBLE, NC_DOUBLE, NC_INT, NC_USHORT, NC_INT},
    .labels = {"gps_time", "flags", "sample_rate", "latitude", "longitude", "elevation", "satellite_count", "speed", "heading", "count_samples", "samples", "checksum"}
};

const CaptureSchema CaptureSchemaV1 = {
    .regular_column_count =  1,
    .types = {"double", "uint16_t"},
    .netcdf_types = {NC_DOUBLE, NC_USHORT},
    .labels = {"computer_time", "samples"}
};

struct Line_v2 {
    double gps_time;
    std::string flags;
    double sample_rate;
    double latitude;
    double longitude;
    double elevation;
    int satellite_count;
    double speed;
    double heading;
    int count_samples;
    std::vector<uint16_t> samples;
};

std::expected<Line_v2, std::string> parse_line_v2(const std::string& line);
