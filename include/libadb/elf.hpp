#ifndef ADB_ELF_HPP
#define ADB_ELF_HPP

#include <cstddef>
#include <filesystem>
#include <elf.h>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <map>
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
            // Read and store the section header of an ELF file.
            void parse_section_headers();
            // Return a string that represents a section's name.
            std::string_view section_name(std::size_t index) const;
            // Will return a pointer to the section header for the section with the given name.
            std::optional<const Elf64_Shdr*> get_section(std::string_view name) const;
            // Will return a range of bytes of data representing the section that is given by its name.
            span<const std::byte> get_section_contents(std::string_view name) const;
            // Retrieve a string value corresponding to the index that is indexed from the (section headers) general string table or dynamic string table.
            std::string_view get_string(std::size_t index) const;
            virt_addr load_bias() const { return load_bias_; }
            void notify_loaded(virt_addr addr) { load_bias_ = addr; }
            // Retrieve the section that corresponds to the given file address.
            const Elf64_Shdr* get_section_with_addr(file_addr addr) const;
            // Retrieve the section that corresponds to the given virtual address.
            const Elf64_Shdr* get_section_with_addr(virt_addr addr) const;
            // Return the starting file address of a section.
            std::optional<file_addr> get_section_start_addr(std::string_view name) const;
            // Read and store symbol tables (complete, abbreviated or none).
            void parse_symbol_table();
            // Returns symbol by its given name.
            std::vector<const Elf64_Sym*> get_symbols_by_name(std::string_view name) const;
            // Returns symbol at the given file adress.
            std::optional<const Elf64_Sym*> get_symbol_at_addr(file_addr addr) const;
            // Returns symbol at the given virtual address.
            std::optional<const Elf64_Sym*> get_symbol_at_addr(virt_addr addr) const;
            // Returns symbol that contains the given file address.
            std::optional<const Elf64_Sym*> get_symbol_with_addr(file_addr addr) const;
            // Returns symbol at the given virtual address.
            std::optional<const Elf64_Sym*> get_symbol_with_addr(virt_addr addr) const;
        private:
            int fd_;
            std::filesystem::path path_;
            std::size_t file_size_;
            std::byte* data_;
            Elf64_Ehdr header_;
            std::vector<Elf64_Shdr> section_headers;
            virt_addr load_bias_;
            std::vector<Elf64_Sym> symbol_table;

            void build_section_map();
            void build_symbol_maps();

            std::unordered_map<std::string_view, Elf64_Shdr*> section_map;
            std::unordered_multimap<std::string_view, Elf64_Sym*> symbol_name_map;

            struct range_comparator {
                bool operator() (std::pair<file_addr, file_addr> lhs, std::pair<file_addr, file_addr> rhs) const {
                    return lhs.first < rhs.first;
                }
            };
            std::map<std::pair<file_addr, file_addr>, Elf64_Sym*, range_comparator> symbol_addr_map;
    };
}

#endif


