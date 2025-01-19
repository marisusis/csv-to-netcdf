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
#include <filesystem>
#include <ranges>

#include "parsing.hpp"
#include "utils.hpp"

void handle_error(int status) {
    if (status != NC_NOERR) {
        spdlog::error("NetCDF error (code {}): {}", status, nc_strerror(status));
        exit(EXIT_FAILURE);
    }
}

namespace fs = std::filesystem;
using namespace indicators;

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
    app.add_option("--output,-o", output_file_path, "Output NetCDF file name");

    uint8_t schema_version;
    app.add_option("--schema-version,-V", schema_version, "Schema version for the input data")
        ->default_val(0);

    int deflate = 0;
    app.add_option("--deflate,-z", deflate, "Deflate level for NetCDF variables")
        ->default_val(0)
        ->check(CLI::Range(1, 9));

    bool scaffold = false;
    app.add_flag("--scaffold,-q", scaffold, "Create a scaffold NetCDF file without writing data");

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

    std::vector<fs::path> files;

    if (file_list) {
        fs::path input_path = input_file_path;
        fs::path input_directory = input_path.remove_filename();

        std::ifstream file_stream;
        file_stream.rdbuf()->pubsetbuf(0, 0);
        file_stream.open(input_file_path);
        for (std::string line; std::getline(file_stream, line);) {
            files.push_back(input_directory / line);
        }

        spdlog::warn("not supported yet");
    } else {
        files.push_back(input_file_path);
    }

    spdlog::info("validating input files...");
    for (const auto& file : files) {
        spdlog::debug("file: {}", file.c_str());
        if (!fs::exists(file)) {
            spdlog::error("file does not exist: {}", file.c_str());
            exit(EXIT_FAILURE);
        }

        // Verify csv extension
        if (file.extension() != ".csv") {
            spdlog::error("invalid file extension: {}", file.extension().c_str());
            exit(EXIT_FAILURE);
        }
    }

    if (output_file_path.empty()) {
        output_file_path = input_file_path + ".nc";
        spdlog::warn("using default output file path: {}", output_file_path);
    }

    // Read metadata from the first file
    std::ifstream file;
    file.rdbuf()->pubsetbuf(0, 0);
    file.open(files.front());

    if (schema_version == 0) {
        spdlog::warn("no schema version provided, detecting schema version from the first file...");
        schema_version = get_schema_version(file);
        spdlog::debug("detected schema version {} from {}", schema_version, files.front().string());
    }

    const CaptureSchema2* schema2;

    switch (schema_version) {
        case 1:
            schema2 = &v1_schema;
            break;
        case 2:
            schema2 = &v2_schema;
            break;
        case 3:
            schema2 = &v3_schema;
            break;
        default:
            spdlog::error("invalid schema version: {}", schema_version);
            exit(EXIT_FAILURE);
    }

    ProgressBar bar{
        option::BarWidth{30},
        option::Start{"["},
        option::Fill{"="},
        option::Lead{">"},
        option::Remainder{" "},
        option::End{"]"},
        option::PostfixText{"preprocessing files"},
        option::ForegroundColor{Color::cyan},
        option::ShowElapsedTime{true},
        option::ShowRemainingTime{true},
        option::FontStyles{std::vector<FontStyle>{FontStyle::bold}},
    };

    // Preprocess files
    size_t total_lines = 0;
    for (size_t i = 0; i < files.size(); i++) {
        const auto& file_path = files[i];
        bar.set_option(option::PostfixText{std::format("preprocessing {}/{} files", i, files.size())});

        file.open(file_path);

        const int file_schema_version = get_schema_version(file);

        if (file_schema_version != schema_version) {
            spdlog::error("schema version mismatch: {} != {}", file_schema_version, schema_version);
            exit(EXIT_FAILURE);
        }

        total_lines += count_data_lines_fast(file_path);
        bar.set_progress((i + 1) * 100 / files.size());
    }

    file.close();

    bar.mark_as_completed();

    spdlog::debug("total lines: {}", total_lines);

    std::string line;

    int ncid, time_dimid, sample_dimid;
    std::map <std::string, int> varids;
    std::map <std::string, int> dimids;
    int varid, dimid;
    int samples_varid;

    spdlog::info("preparing netcdf file...");

    // Create the file
    handle_error(nc_create(output_file_path.c_str(), NC_NETCDF4, &ncid));

    int format;
    nc_inq_format(ncid, &format);
    spdlog::debug("NetCDF format: {}", format);

    if (schema_version > 1) {
        auto file_path = files.front();
        file.open(file_path);
        std::map<std::string, std::string> metadata = parse_metadata(file);

        nc_put_att(ncid, NC_GLOBAL, "original_schema_version", NC_BYTE, 1, &schema_version);
        
        for (auto const& [key, val] : metadata) {
            std::string lower_key = key;
            std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);
            nc_put_att(ncid, NC_GLOBAL, lower_key.c_str(), NC_CHAR, val.length(), val.c_str());

            spdlog::debug("Added metadata: {} = {}", lower_key, val);
        }
    }

    // Define dimensions
    handle_error(nc_def_dim(ncid, "time", NC_UNLIMITED, &time_dimid));
    handle_error(nc_def_dim(ncid, "sample", 7200, &sample_dimid));
    dimids["time"] = time_dimid;
    dimids["sample"] = sample_dimid;

    for (const ColumnSchema& column : schema2->columns) {
        if (column.label == "samples") {
            break;
        }

        int varid;
        handle_error(nc_def_var(ncid, column.label.c_str(), column.netcdf_type, 1, &dimids["time"], &varid));

        if (deflate) {
            handle_error(nc_def_var_deflate(ncid, varid, 0, 1, deflate));
        }

        if (!column.unit.empty()) {
            nc_put_att(ncid, varid, "units", NC_CHAR, column.unit.length(), column.unit.c_str());
        }

        varids[column.label] = varid;
        spdlog::debug("created variable \"{}\" with type \"{}\"", column.label, column.netcdf_type);
    }

    int dims[2] = {dimids["time"], dimids["sample"]};
    handle_error(nc_def_var(ncid, "parsing_errors", NC_INT, 2, dims, &varid));
    varids["parsing_errors"] = varid;

    // Define the samples variable
    handle_error(nc_def_var(ncid, "samples", NC_SHORT, 2, dims, &varid));
    short valid_range[2] = {0, 1023};
    handle_error(nc_put_att(ncid, varid, "valid_min", NC_SHORT, 1, &valid_range[0]));
    handle_error(nc_put_att(ncid, varid, "valid_max", NC_SHORT, 1, &valid_range[1]));
    varids["samples"] = varid;
    if (deflate) {
        handle_error(nc_def_var_deflate(ncid, varids["samples"], 0, 1, deflate));
    }

    // End define mode
    handle_error(nc_enddef(ncid));

    if (scaffold) {
        spdlog::warn("Scaffold mode enabled, skipping data processing");
        handle_error(nc_close(ncid));
        spdlog::info("Successfully created NetCDF file: {}\n", output_file_path);
        return 0;
    }

    // Read data lines
    ProgressBar bar2{
        option::BarWidth{30},
        option::Start{"["},
        option::Fill{"="},
        option::Lead{">"},
        option::Remainder{" "},
        option::End{"]"},
        option::PostfixText{"Processing data lines"},
        option::ForegroundColor{Color::yellow},
        option::ShowElapsedTime{true},
        option::ShowRemainingTime{true},
        option::FontStyles{std::vector<FontStyle>{FontStyle::bold}},
    };

    bar2.set_option(option::PostfixText{"processing"});

    size_t errors = 0;
    size_t lines = 0;
    size_t time_coord = 0;
    spdlog::info("processing data lines...");
    std::ios::sync_with_stdio(false);
    for (size_t i = 0; i < files.size(); i++) {
        const auto& file_path = files[i];
        std::ifstream file;
        file.open(file_path);

        file.clear();
        file.seekg(0, std::ios::beg);

        for (std::string line; std::getline(file, line);) {
            lines++;

            if (line.empty() || line.at(0) == '#') {
                continue;
            }

            try {

                std::map<std::string, std::any> parsed;

                if (schema_version == 1) {
                    spdlog::error("Schema version 1 not supported");
                    exit(EXIT_FAILURE);
                } else if (schema_version == 2) {
                    parsed = parse_line_v2(line);
                } else if (schema_version == 3) {
                    parsed = parse_line_v3(line);
                }

                for (const auto& column : schema2->columns) {

                    if (parsed.find(column.label) == parsed.end()) {
                        spdlog::error("missing column: {}", column.label);
                        continue;
                    }

                    if (column.label == "samples") {
                        size_t startp[2] = {time_coord, 0};
                        size_t countp[2] = {1, static_cast<size_t>(7200)};

                        std::vector<int> samples = std::any_cast<std::vector<int>>(parsed["samples"]);
                        std::vector<short> samples_short(samples.begin(), samples.end());
                        nc_put_vara_short(ncid, varids["samples"], startp, countp, samples_short.data());
                        continue;
                    }

                    if (column.netcdf_type == NC_INT) {
                        int int_value = std::any_cast<int>(parsed[column.label]);
                        nc_put_var1_int(ncid, varids[column.label], &time_coord, &int_value);
                    } else if (column.netcdf_type == NC_DOUBLE) {
                        double value = std::any_cast<double>(parsed[column.label]);
                        nc_put_var1_double(ncid, varids[column.label], &time_coord, &value);
                    } else if (column.netcdf_type == NC_BYTE) {
                        int byte_value = std::any_cast<char>(parsed[column.label]);
                        nc_put_var1_int(ncid, varids[column.label], &time_coord, &byte_value);
                    } else if (column.netcdf_type == NC_SHORT) {
                        short short_value = std::any_cast<short>(parsed[column.label]);
                        nc_put_var1_short(ncid, varids[column.label], &time_coord, &short_value);
                    } else if (column.netcdf_type == NC_STRING) {
                        std::string str_value = std::any_cast<std::string>(parsed[column.label]);
                        nc_put_var1_text(ncid, varids[column.label], &time_coord, str_value.c_str());
                    }
                }
            } catch (const std::exception& e) {
                spdlog::debug("Error parsing line {}: {}\nLINE: {}", lines, e.what(), line.substr(0, 20));
                errors++;
                continue;
            }

            bar2.set_progress(lines * 100 / total_lines);
            bar2.set_option(option::PostfixText{std::format("{}/{} lines, {}/{} files, {} errors", lines, total_lines, i, files.size(), errors)});

            time_coord++;
        }
    }
    std::ios::sync_with_stdio(true);

    bar2.mark_as_completed();

    if (errors > 0) {
        spdlog::warn("Encountered {} errors while parsing the input file", errors);
    }

    // Close the file
    handle_error(nc_close(ncid));

    spdlog::info("Successfully created NetCDF file: {}\n", output_file_path);
    return 0;
}