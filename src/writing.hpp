#include "CLI/CLI.hpp"
#include "spdlog/spdlog.h"
#include "netcdf.h"

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <map>
#include <regex>
#include "parsing.hpp"

void write_line_v2(Line_v2 line, int ncid, std::map<std::string, int> varids, size_t time_coord);
void write_line_v3(Line_v3 line, int ncid, std::map<std::string, int> varids, size_t time_coord);