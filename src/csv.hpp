#ifndef C2N_CSV_HPP
#define C2N_CSV_HPP

#include <string>
#include <memory>
#include <fstream>
#include <iterator>
#include <filesystem>
#include "parsing.hpp"

class CSVFile {

    public:
        explicit CSVFile(const std::filesystem::path& file_path);

        const std::filesystem::path file_path() const;
        const size_t size_bytes() const;
        const SchemaVersion get_schema_version() const;

        // Line iterator that yields one line at a time from the CSV file.
        struct LineIterator {
            using iterator_category = std::input_iterator_tag;
            using value_type = std::string;
            using difference_type = std::ptrdiff_t;
            using pointer = const std::string*;
            using reference = const std::string&;

            LineIterator(); // end iterator
            explicit LineIterator(const std::shared_ptr<std::ifstream>& ifs);

            reference operator*() const;
            pointer operator->() const;

            LineIterator& operator++();    // pre-increment
            LineIterator operator++(int);  // post-increment

            bool operator==(const LineIterator& other) const;
            bool operator!=(const LineIterator& other) const;

        private:
            std::shared_ptr<std::ifstream> ifs_;
            std::string line_;
            bool at_end_;
            void read_next();
        };

        using line_iterator = LineIterator;

        // Opens the file and returns an iterator to the first line.
        // If the file cannot be opened the returned iterator == end().
        line_iterator begin() const;
        line_iterator end() const;

    private:
        std::filesystem::path file_path_;

};

#endif