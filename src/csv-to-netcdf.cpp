#include "CLI/CLI.hpp"
#include "spdlog/spdlog.h"
#include "netcdf.h"
#include <indicators/cursor_control.hpp>
#include <indicators/progress_bar.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <map>
#include <regex>
#include <ranges>

#include "parsing.hpp"
#include "writing.hpp"
#include "utils.hpp"

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

    int deflate = 0;
    app.add_option("--deflate,-z", deflate, "Deflate level for NetCDF variables")
        ->default_val(0)
        ->check(CLI::Range(1, 9));
    

    CLI11_PARSE(app, argc, argv);

    // spdlog::set_pattern("[%^%L%$] [%H:%M:%S %z] [%n] [thread %t] %v");
    spdlog::set_pattern("[%^%l%$] %v");
    spdlog::info("hello.");

    if (verbose) {
        spdlog::set_level(spdlog::level::debug);
    }

    if (deflate) {
        spdlog::info("compression enabled at level {}.", deflate);
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
            spdlog::error("invalid schema version: {}", schema_version);
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

    int ncid, time_dimid, sample_dimid;
    int samples_varid;

    // Create the file
    handle_error(nc_create(output_file_path.c_str(), NC_NETCDF4, &ncid));

    int format;
    nc_inq_format(ncid, &format);
    spdlog::debug("NetCDF format: {}", format);

    if (schema_version > 1) {
        std::map<std::string, std::string> metadata = parse_metadata(file);

        nc_put_att(ncid, NC_GLOBAL, "original_schema_version", NC_BYTE, 1, &schema_version);
        
        for (auto const& [key, val] : metadata) {
            std::string lower_key = key;
            std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);
            nc_put_att(ncid, NC_GLOBAL, lower_key.c_str(), NC_CHAR, val.length(), val.c_str());
            spdlog::debug("Added metadata: {} = {}", lower_key, val);
        }
    }


    // Define the dimensions
    handle_error(nc_def_dim(ncid, "time", NC_UNLIMITED, &time_dimid));
    handle_error(nc_def_dim(ncid, "sample", 7200, &sample_dimid));
    int dimids[2] = {time_dimid, sample_dimid};

    // Create variables
    std::map <std::string, int> varids;
    auto labels_and_types = std::views::zip(schema.labels, schema.netcdf_types);
    for (const auto& [label, type] : labels_and_types) {
        if (label == "samples") {
            break;
        }

        int varid;
        handle_error(nc_def_var(ncid, label.c_str(), type, 1, &time_dimid, &varid));

        if (deflate) {
            handle_error(nc_def_var_deflate(ncid, varid, 0, 1, deflate));
        }

        varids[label] = varid;
        spdlog::debug("Created variable: {} with type: {}", label, type);
    }

        // Define the variable
    handle_error(nc_def_var(ncid, "samples", NC_SHORT, 2, dimids, &samples_varid));
    if (deflate) {
        handle_error(nc_def_var_deflate(ncid, samples_varid, 0, 1, deflate));
    }
    varids["samples"] = samples_varid;

    // End define mode
    handle_error(nc_enddef(ncid));

    const size_t total_lines = count_data_lines(file);
    spdlog::debug("Total lines: {}", total_lines);

    // Read data lines
    using namespace indicators;
    ProgressBar bar{
        option::BarWidth{50},
        option::Start{"["},
        option::Fill{"="},
        option::Lead{">"},
        option::Remainder{" "},
        option::End{"]"},
        option::PostfixText{"Extracting Archive"},
        option::ForegroundColor{Color::green},
        option::ShowElapsedTime{true},
        option::ShowRemainingTime{true},
    };

    bar.set_option(option::PostfixText{std::format("Processing {}/{}", 0, total_lines)});

    file.clear();
    file.seekg(0, std::ios::beg);
    size_t lines = 0;
    size_t time_coord = 0;
    size_t errors = 0;
    for (std::string line; std::getline(file, line);) {
        lines++;

        if (line.empty() || line.at(0) == '#') {
            continue;
        }

        try {
            if (schema_version == 1) {
                spdlog::error("Schema version 1 not supported");
                exit(EXIT_FAILURE);
            } else if (schema_version == 2) {
                auto result = parse_line_v2(line);
                write_line_v2(result, ncid, varids, time_coord);
            } else if (schema_version == 3) {
                auto result = parse_line_v3(line);
                write_line_v3(result, ncid, varids, time_coord);
            }
        } catch (const std::exception& e) {
            spdlog::debug("Error parsing line {}: {}\nLINE: {}", lines, e.what(), line.substr(0, 20));
            errors++;
            continue;
        }

        bar.set_progress(lines * 100 / total_lines);
        bar.set_option(option::PostfixText{std::format("Processing {}/{}", lines, total_lines)});

        time_coord++;
    }

    bar.mark_as_completed();

    if (errors > 0) {
        spdlog::warn("Encountered {} errors while parsing the input file", errors);
    }

    // Close the file
    handle_error(nc_close(ncid));

    spdlog::info("Successfully created NetCDF file: {}\n", output_file_path);
    spdlog::debug("Lines: {}", lines);
    return 0;
}