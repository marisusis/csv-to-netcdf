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

#define FILE_NAME "output.nc"

enum class ReadState {
    Scanning,
    Metadata,
    Data
};

void handle_error(int status) {
    if (status != NC_NOERR) {
        spdlog::error("NetCDF error (code {}): {}", status, nc_strerror(status));
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char **argv) {
    CLI::App app;

    bool verbose = false;
    app.add_flag("--verbose,-v", verbose, "Print verbose output");

    bool file_list;
    app.add_flag("--file-list", file_list, "Treat input file as a list of files");

    std::string input_file_path;
    app.add_option("--input,-i", input_file_path, "CSV input file")
        ->check(CLI::ExistingFile)
        ->required();

    std::string output_file_path;
    app.add_option("--output-file,-o", output_file_path, "Output NetCDF file name")
        ->required();

    uint8_t schema_version;
    app.add_option("--schema-version,-V", schema_version, "Schema version for the input data")
        ->required();

    bool strict = false;
    app.add_flag("--strict,-s", strict, "Strict mode");
    

    CLI11_PARSE(app, argc, argv);

    // spdlog::set_pattern("[%^%L%$] [%H:%M:%S %z] [%n] [thread %t] %v");
    spdlog::set_pattern("[%^%l%$] %v");
    spdlog::info("hello.");

    if (verbose) {
        spdlog::set_level(spdlog::level::debug);
    }

    CaptureSchema schema;

    switch (schema_version) {
        case 1:
            schema = CaptureSchemaV1;
            break;
        case 2:
            schema = CaptureSchemaV2;
            break;
        case 3:
            schema = CaptureSchemaV3;
            break;
        default:
            spdlog::error("Invalid schema version: {}", schema_version);
            exit(EXIT_FAILURE);
    }

    if (schema_version != 2) {
        spdlog::error("Only schema version 2 is supported");
        exit(EXIT_FAILURE);
    }

    if (file_list) {
        spdlog::error("not supported yet");
        exit(EXIT_FAILURE);
    }


    std::ifstream file;
    file.rdbuf()->pubsetbuf(0, 0);
    file.open(input_file_path);

    std::string line;
    std::map <std::string, std::string> metadata;

    ReadState readState = ReadState::Scanning;
    while (true) {

        // each line should be small enough to not be a memory issue since it's less than a second of data
        if (!std::getline(file, line).good()) {
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

    int ncid, time_dimid, sample_dimid;
    int samples_varid;

    // Create the file
    handle_error(nc_create(FILE_NAME, NC_CLOBBER, &ncid));

    nc_put_att(ncid, NC_GLOBAL, "original_schema_version", NC_BYTE, 1, &schema_version);
    for (auto const& [key, val] : metadata) {
        nc_put_att(ncid, NC_GLOBAL, key.c_str(), NC_CHAR, val.length(), val.c_str());
    }

    // Define the dimensions
    handle_error(nc_def_dim(ncid, "time", NC_UNLIMITED, &time_dimid));
    handle_error(nc_def_dim(ncid, "sample", 7200, &sample_dimid));
    int dimids[2] = {time_dimid, sample_dimid};

    // Create variables
    std::map <std::string, int> varids;
    for (auto const& label : schema.labels) {
        if (label == "samples") {
            break;
        }

        int varid;
        handle_error(nc_def_var(ncid, label.c_str(), NC_INT, 1, &time_dimid, &varid));
        varids[label] = varid;
        spdlog::debug("Created variable: {}", label);
    }

    // Count samples in first few lines
    int look_at = 1000;
    int sample_count = 0;
    for (int i = 0; i < look_at; i++) {
        if (!std::getline(file, line).good()) {
            spdlog::error("Failed to read the first line of the input file");
            exit(EXIT_FAILURE);
        }

        auto result = parse_line_v2(line);
        if (result.has_value()) {
            
        } else {
            spdlog::error("Error parsing line: {}", result.error());
        }
    }

    // Define the variable
    handle_error(nc_def_var(ncid, "samples", NC_INT, 2, dimids, &samples_varid));

    // End define mode
    handle_error(nc_enddef(ncid));



    // Close the file
    handle_error(nc_close(ncid));

    spdlog::info("Successfully created NetCDF file: {}\n", FILE_NAME);
    return 0;
}