#include "csv.hpp"
#include "spdlog/spdlog.h"
#include "parsing.hpp"
#include <string>
#include <fstream>
#include <memory>

CSVFile::CSVFile(const std::filesystem::path& file_path) : file_path_(file_path) {
    // spdlog::trace("CSVFile initialized with file path: {}", file_path_.string());
}

const std::filesystem::path CSVFile::file_path() const {
    return file_path_;
}

const size_t CSVFile::size_bytes() const {
    try {
        return std::filesystem::file_size(file_path_);
    } catch (const std::filesystem::filesystem_error& e) {
        spdlog::error("Error getting file size for {}: {}", file_path_.string(), e.what());
        return 0;
    }
}

const SchemaVersion CSVFile::get_schema_version() const {
    std::ifstream file(file_path_);
    if (!file.is_open()) {
        // spdlog::error("Failed to open CSV file to get schema version: {}", file_path_);
        std::string filename = file_path_.string();
        throw std::runtime_error(std::format("Failed to open CSV file: {}", filename));
    }

    file.clear();
    file.seekg(0, std::ios::beg);

    std::map<std::string, std::string> metadata = parse_metadata(file);

    for (const auto& [key, value] : metadata) {
        spdlog::debug("Metadata: {} = {}", key, value);
    }

    if (metadata.empty()) {
        return Schema_V1;
    } else if (!metadata.empty() && metadata.find("version") == metadata.end()) {
        return Schema_V2;
    } else if (metadata.find("version") != metadata.end()) {
        if (metadata["version"] == "3") {
            return Schema_V3;
        }
    }

    for (const auto& [key, value] : metadata) {
        spdlog::debug("Metadata: {} = {}", key, value);
    }
    
    throw std::runtime_error("Unknown schema version");
}


// LineIterator implementations
CSVFile::LineIterator::LineIterator() : ifs_(nullptr), line_(), at_end_(true) {}

CSVFile::LineIterator::LineIterator(const std::shared_ptr<std::ifstream>& ifs)
    : ifs_(ifs), line_(), at_end_(false) {
    read_next();
}

void CSVFile::LineIterator::read_next() {
    if (!ifs_ || !ifs_->is_open()) {
        at_end_ = true;
        return;
    }
    if (std::getline(*ifs_, line_)) {
        at_end_ = false;
    } else {
        at_end_ = true;
        // optionally release stream when finished
        ifs_.reset();
    }
}

CSVFile::LineIterator::reference CSVFile::LineIterator::operator*() const {
    return line_;
}

CSVFile::LineIterator::pointer CSVFile::LineIterator::operator->() const {
    return &line_;
}

CSVFile::LineIterator& CSVFile::LineIterator::operator++() {
    read_next();
    return *this;
}

CSVFile::LineIterator CSVFile::LineIterator::operator++(int) {
    LineIterator tmp = *this;
    ++(*this);
    return tmp;
}

bool CSVFile::LineIterator::operator==(const LineIterator& other) const {
    if (at_end_ && other.at_end_) return true;
    // consider equal if they refer to same stream and same line (or both non-end and same stream)
    return ifs_ == other.ifs_ && line_ == other.line_ && at_end_ == other.at_end_;
}

bool CSVFile::LineIterator::operator!=(const LineIterator& other) const {
    return !(*this == other);
}

// CSVFile begin/end
CSVFile::line_iterator CSVFile::begin() const {
    auto ifs = std::make_shared<std::ifstream>(file_path_);
    if (!ifs->is_open()) {
        spdlog::error("Failed to open CSV file: {}", file_path_.string());
        return CSVFile::line_iterator(); // end
    }
    return CSVFile::line_iterator(ifs);
}

CSVFile::line_iterator CSVFile::end() const {
    return CSVFile::line_iterator(); // default-constructed end iterator
}