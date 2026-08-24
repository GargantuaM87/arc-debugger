#ifndef ADB_ELF_HPP
#define ADB_ELF_HPP

#include <cstddef>
#include <filesystem>
#include <elf.h>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <optional>
#include "./types.hpp"

namespace adb {
    class elf {
        public:
            elf(const std::filesystem::path& path);
            ~elf();

            elf(const elf&) = delete;
            elf& operator=(const elf&) = delete;

            std::filesystem::path path() const  {return path_;}
            const Elf64_Ehdr& header() const { return header_; }
            // Read and store the section header of an ELF file
            void parse_section_headers();
            // Return a string that represents a section's name
            std::string_view section_name(std::size_t index) const;
            // Will return a pointer to the section header for the section with the given name
            std::optional<const Elf64_Shdr*> get_section(std::string_view name) const;
            // Will return a range of bytes of data representing the section that is given by its name
            span<const std::byte> get_section_contents(std::string_view name) const;
            std::string_view get_string(std::size_t index) const;
        private:
            int fd_;
            std::filesystem::path path_;
            std::size_t file_size_;
            std::byte* data_;
            Elf64_Ehdr header_;
            std::vector<Elf64_Shdr> section_headers;

            void build_section_map();

            std::unordered_map<std::string_view, Elf64_Shdr*> section_map;
    };
}

#endif


