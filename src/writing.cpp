#include "CLI/CLI.hpp"
#include "spdlog/spdlog.h"
#include "netcdf.h"

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <map>
#include <regex>
#include "writing.hpp"

void write_line_v2(Line_v2 line, int ncid, std::map<std::string, int> varids, size_t time_coord) {
    nc_put_var1_int(ncid, varids["gps_time"], &time_coord, &line.gps_time);
    int has_gps = static_cast<int>(line.has_gps);
    nc_put_var1_int(ncid, varids["has_gps"], &time_coord, &has_gps);
    int clipping = static_cast<int>(line.clipping);
    nc_put_var1_int(ncid, varids["clipping"], &time_coord, &clipping);
    nc_put_var1_double(ncid, varids["sample_rate"], &time_coord, &line.sample_rate);
    nc_put_var1_double(ncid, varids["latitude"], &time_coord, &line.latitude);
    nc_put_var1_double(ncid, varids["longitude"], &time_coord, &line.longitude);
    nc_put_var1_double(ncid, varids["elevation"], &time_coord, &line.elevation);
    nc_put_var1_int(ncid, varids["satellite_count"], &time_coord, &line.satellite_count);
    nc_put_var1_double(ncid, varids["speed"], &time_coord, &line.speed);
    nc_put_var1_double(ncid, varids["heading"], &time_coord, &line.heading);
    nc_put_var1_int(ncid, varids["count_samples"], &time_coord, &line.count_samples);

    size_t startp[2] = {time_coord, 0};
    size_t countp[2] = {1, static_cast<size_t>(line.count_samples)};
    nc_put_vara_int(ncid, varids["samples"], startp, countp, line.samples.data());
}

void write_line_v3(Line_v3 line, int ncid, std::map<std::string, int> varids, size_t time_coord) {
    nc_put_var1_int(ncid, varids["gps_time"], &time_coord, &line.gps_time);
    nc_put_var1_double(ncid, varids["computer_time"], &time_coord, &line.computer_time);
    int has_gps = static_cast<int>(line.has_gps);
    nc_put_var1_int(ncid, varids["has_gps"], &time_coord, &has_gps);
    int clipping = static_cast<int>(line.clipping);
    nc_put_var1_int(ncid, varids["clipping"], &time_coord, &clipping);
    nc_put_var1_double(ncid, varids["sample_rate"], &time_coord, &line.sample_rate);
    nc_put_var1_double(ncid, varids["latitude"], &time_coord, &line.latitude);
    nc_put_var1_double(ncid, varids["longitude"], &time_coord, &line.longitude);
    nc_put_var1_double(ncid, varids["elevation"], &time_coord, &line.elevation);
    nc_put_var1_int(ncid, varids["satellite_count"], &time_coord, &line.satellite_count);
    nc_put_var1_double(ncid, varids["speed"], &time_coord, &line.speed);
    nc_put_var1_double(ncid, varids["heading"], &time_coord, &line.heading);
    nc_put_var1_int(ncid, varids["count_samples"], &time_coord, &line.count_samples);

    size_t startp[2] = {time_coord, 0};
    size_t countp[2] = {1, static_cast<size_t>(line.count_samples)};

    std::vector<short> samples_short(line.samples.begin(), line.samples.end());
    nc_put_vara_short(ncid, varids["samples"], startp, countp, samples_short.data());
}