#ifndef ADB_ELF_HPP
#define ADB_ELF_HPP

#include <filesystem>
#include <elf.h>
#include <vector>

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
            std::string_view section_name(std::size_t index) const;
        private:
            int fd_;
            std::filesystem::path path_;
            std::size_t file_size_;
            std::byte* data_;
            Elf64_Ehdr header_;
            std::vector<Elf64_Shdr> section_headers;
    };
}

#endif


