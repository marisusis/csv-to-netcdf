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
#include <future>
#include <deque>
#include <optional>
#include <thread>
#include <chrono>

#include "csv.hpp"
#include "parsing.hpp"
#include "utils.hpp"

const size_t BYTES_PER_LINE_ESTIMATE = 28800;

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
    
    CLI::Option* verbose_option = app.add_flag("--verbose,-v", "Print verbose output");

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
    app.add_flag("--scaffold", scaffold, "Create a scaffold NetCDF file without writing data");

    bool dont_write = false;
    app.add_flag("--dont-write", dont_write, "Don't write data to the NetCDF file");

    CLI11_PARSE(app, argc, argv);

    // spdlog::set_pattern("[%^%L%$] [%H:%M:%S %z] [%n] [thread %t] %v");
    spdlog::set_pattern("[%^%l%$] %v");
    spdlog::info("hello.");

    if (verbose_option->count() == 1) {
        spdlog::set_level(spdlog::level::debug);
    } else if (verbose_option->count() > 1) {
        spdlog::set_level(spdlog::level::trace);
    }

    if (deflate) {
        spdlog::info("compression enabled at level {}.", deflate);
    }

    std::vector<fs::path> files;
    std::vector<CSVFile> csv_files;

    if (file_list) {
        fs::path input_path = input_file_path;
        fs::path input_directory = input_path.remove_filename();

        std::ifstream file_stream;
        file_stream.rdbuf()->pubsetbuf(0, 0);
        file_stream.open(input_file_path);
        for (std::string line; std::getline(file_stream, line);) {
            files.push_back(input_directory / line);
            csv_files.emplace_back(input_directory / line);
        }

        spdlog::warn("not supported yet");
    } else {
        files.push_back(input_file_path);
        csv_files.emplace_back(input_file_path);
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
    for (size_t i = 0; i < csv_files.size(); i++) {
        const auto& csv_file = csv_files[i];
        bar.set_option(option::PostfixText{std::format("preprocessing {}/{} files", i, files.size())});

        spdlog::debug("file size: {} bytes", csv_file.size_bytes());
        total_lines += csv_file.size_bytes() / BYTES_PER_LINE_ESTIMATE;

        // total_lines += count_data_lines_fast(file_path);
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
    std::string output_file_temp = output_file_path + ".tmp";
    handle_error(nc_create(output_file_temp.c_str(), NC_NETCDF4, &ncid));

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

    std::vector<char*> text;
    text.reserve(files.size());

    std::transform(files.begin(), files.end(), std::back_inserter(text), [](const fs::path& p) {
        return strdup(p.filename().string().c_str());
    });

    handle_error(nc_put_att(ncid, NC_GLOBAL, "source_files", NC_STRING, text.size(), text.data()));

    // Define dimensions
    handle_error(nc_def_dim(ncid, "time", NC_UNLIMITED, &time_dimid));
    handle_error(nc_def_dim(ncid, "sample", 7200, &sample_dimid));
    dimids["time"] = time_dimid;
    dimids["sample"] = sample_dimid;

    for (const ColumnSchema& column : columns) {
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

    handle_error(nc_def_var(ncid, "test111", NC_INT, 1, &dimids["time"], &varid));
    handle_error(nc_put_att(ncid, varid, "units", NC_CHAR, 3, "ms"));

    int dims[2] = {dimids["time"], dimids["sample"]};

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
        spdlog::info("moving temporary file to final location... {}->{}", output_file_temp, output_file_path);    
        fs::rename(output_file_temp, output_file_path);
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
        option::ShowPercentage{true},
        option::ShowRemainingTime{true},
        option::FontStyles{std::vector<FontStyle>{FontStyle::bold}},
    };

    bar2.set_option(option::PostfixText{"processing"});

    uint64_t errors = 0;
    size_t line_counter = 0;
    size_t file_counter = 0;
    size_t time_coord = 0;
    spdlog::info("processing data lines...");
    std::ios::sync_with_stdio(false);

    // Asynchronous parsing + ordered writing:
    size_t max_workers = std::thread::hardware_concurrency();
    spdlog::info("Detected {} hardware threads", max_workers);
    if (max_workers == 0) max_workers = 4;
    size_t max_queue = std::max<size_t>(4, max_workers * 4);

    struct ParseOutcome {
        std::optional<ParsedLine> parsed;
        std::string error;
    };

    struct Pending {
        std::future<ParseOutcome> fut;
        std::string line;
        size_t local_line;
        fs::path file_path;
    };

    std::deque<Pending> pending;

    auto flush_ready = [&](bool force = false) {
        using namespace std::chrono_literals;
        while (!pending.empty()) {
            auto &p = pending.front();
            if (!force) {
                if (p.fut.wait_for(0ms) != std::future_status::ready) break;
            } else {
                p.fut.wait();
            }

            ParseOutcome outcome = p.fut.get();
            // Process outcome in-order
            if (!outcome.parsed.has_value()) {
                // parsing failed
                errors++;
                spdlog::error("Error parsing line {} in file {}: {}\nLINE: {}", p.local_line, p.file_path.string(), outcome.error, p.line.substr(0, 100));
            } else {
                ParsedLine &parsed = *outcome.parsed;
                if (!dont_write) {
                    if (auto cpu_time = parsed.cpu_time) {
                        nc_put_value<double>(ncid, varids["cpu_time"], &time_coord, *cpu_time);
                    }
                    if (auto gps_time = parsed.gps_time) {
                        nc_put_value<unsigned long long>(ncid, varids["gps_time"], &time_coord, *gps_time);
                    }
                    nc_put_value<char>(ncid, varids["has_gps"], &time_coord, static_cast<char>(parsed.has_gps));
                    nc_put_value<char>(ncid, varids["clipping"], &time_coord, static_cast<char>(parsed.clipping));
                    nc_put_value<double>(ncid, varids["sample_rate"], &time_coord, parsed.sample_rate);
                    nc_put_value<double>(ncid, varids["latitude"], &time_coord, parsed.latitude);
                    nc_put_value<double>(ncid, varids["longitude"], &time_coord, parsed.longitude);
                    nc_put_value<double>(ncid, varids["elevation"], &time_coord, parsed.elevation);
                    nc_put_value<int>(ncid, varids["satellite_count"], &time_coord, parsed.satellite_count);
                    nc_put_value<double>(ncid, varids["speed"], &time_coord, parsed.speed);
                    nc_put_value<double>(ncid, varids["heading"], &time_coord, parsed.heading);

                    size_t startp[2] = {time_coord, 0};
                    size_t countp[2] = {1, static_cast<size_t>(7200)};
                    nc_put_vara_short(ncid, varids["samples"], startp, countp, parsed.samples.data());
                    time_coord++;
                }
            }

            pending.pop_front();
        }
    };

    for (CSVFile& csv_file : csv_files) {
        file_counter++;
        size_t local_line_counter = 0;

        std::string file_name = csv_file.file_path().filename().string();
        SchemaVersion schema_version = csv_file.get_schema_version();
        bar2.set_option(option::PostfixText{std::format("{} v{} {}/{} files, {} errors", file_name, static_cast<int>(schema_version), file_counter, files.size(), errors)});


        for (auto line : csv_file) {
            bar2.set_progress(line_counter * 100 / (total_lines == 0 ? 1 : total_lines));
            line_counter++;
            local_line_counter++;

            if (line.empty() || line.at(0) == '#') {
                continue;
            }

            if (line.at(0) == '$') {
                spdlog::warn("bad line detected at file {}, line {}", csv_file.file_path().string(), local_line_counter);
                continue;
            }

            // Launch async parse task
            auto task_line = line; // copy for async
            auto file_path_copy = csv_file.file_path();
            size_t local_copy = local_line_counter;

            auto fut = std::async(std::launch::async, [task_line, schema_version]() -> ParseOutcome {
                try {
                    ParsedLine parsed;
                    if (schema_version == Schema_V1) {
                        // maintain original behavior
                        throw std::runtime_error("Schema version 1 not supported");
                    } else if (schema_version == Schema_V2) {
                        parsed = parse_line_v2(task_line);
                    } else if (schema_version == Schema_V3) {
                        parsed = parse_line_v3(task_line);
                    } else {
                        return ParseOutcome{std::nullopt, "unknown schema version"};
                    }
                    return ParseOutcome{std::make_optional(parsed), std::string()};
                } catch (const std::exception &e) {
                    return ParseOutcome{std::nullopt, e.what()};
                } catch (...) {
                    return ParseOutcome{std::nullopt, "unknown exception"};
                }
            });

            pending.push_back(Pending{std::move(fut), std::move(task_line), local_copy, file_path_copy});

            // throttle number of outstanding tasks and flush ready ones
            if (pending.size() > max_queue) {
                flush_ready(false);
                // if still too large (no ready ones), block on the front to free space
                if (pending.size() > max_queue) {
                    pending.front().fut.wait();
                    flush_ready(false);
                }
            }
        }
        // After finishing a file, try to flush ready tasks to keep memory low
        flush_ready(false);
    }

    // Wait for all remaining tasks (force-complete)
    flush_ready(true);
    std::ios::sync_with_stdio(true);

    if (errors > 0) {
        spdlog::warn("Encountered {} errors while parsing the input file", errors);
    }

    handle_error(nc_put_att(ncid, NC_GLOBAL, "parsing_errors", NC_INT64, 1, &errors));
    handle_error(nc_put_att(ncid, NC_GLOBAL, "complete", NC_CHAR, 4, "yes"));

    // Close the file
    handle_error(nc_close(ncid));

    // Move the temporary file to the final location
    spdlog::info("moving temporary file to final location... {}->{}", output_file_temp, output_file_path);
    fs::rename(output_file_temp, output_file_path);

    spdlog::info("successfully created NetCDF file: {}", output_file_path);
    return 0;
}