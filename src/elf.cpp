#include <elf.h>
#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "../include/libadb/elf.hpp"
#include "../include/libadb/error.hpp"
#include "../include/libadb/bit.hpp"

adb::elf::elf(const std::filesystem::path& path) {
    path_ = path;

    if((fd_ = open(path.c_str(), O_RDONLY)) < 0) {
        error::send_errno("Could not open ELF file");
    }
    // fill in metadata for ELF file
    struct stat stats; // 'struct' keyword differs 'stat' from its global function counterpart
    if(fstat(fd_, &stats) < 0) {
        error::send_errno("Could not retrieve ELF file stats");
    }
    file_size_ = stats.st_size;

    void* ret;
    // map file into virtual memory
    if((ret = mmap(0, file_size_, PROT_READ, MAP_SHARED, fd_, 0)) == MAP_FAILED) {
        close(fd_);
        error::send_errno("Could not mmap ELF file");
    }
    data_ = reinterpret_cast<std::byte*>(ret);

    std::copy(data_, data_ + sizeof(header_), as_bytes(header_));
    parse_section_headers();
    build_section_map();
}

adb::elf::~elf() {
    munmap(data_, file_size_);
    close(fd_);
}

void adb::elf::parse_section_headers() {
    // special edge case for when the ELF file contains many sections
    auto n_headers = header_.e_shnum; // num of section entries
    if(n_headers == 0 and header_.e_shentsize != 0) { // if a file has 0xff00 sections or more, then set e__shnum to 0 and store number of sections in sh_size
        n_headers = from_bytes<Elf64_Shdr>(data_ + header_.e_shoff).sh_size;
    }
    section_headers.resize(n_headers);
    std::copy(data_ + header_.e_shoff, data_ + header_.e_shoff + sizeof(Elf64_Shdr) * n_headers, reinterpret_cast<std::byte*>(section_headers.data()));
}

std::string_view adb::elf::section_name(std::size_t index) const {
    auto& section = section_headers[header_.e_shstrndx]; // storing the section name string table
    return { reinterpret_cast<char*>(data_) + section.sh_size + index }; // return the string that starts at the given index into section
}

void adb::elf::build_section_map() {
    // section name to reference to the section's header
    for(auto& section : section_headers) {
        section_map[section_name(section.sh_name)] = &section;
    }
}

std::optional<const Elf64_Shdr*> adb::elf::get_section(std::string_view name) const {
    if(section_map.count(name) == 0) {
        return std::nullopt;
    }

    return section_map.at(name);
}

adb::span<const std::byte> adb::elf::get_section_contents(std::string_view name) const {
    if(auto sect = get_section(name); sect) {
        return { data_ + sect.value()->sh_offset, sect.value()->sh_size };
    }

    return { nullptr, std::size_t(0) };
}

std::string_view adb::elf::get_string(std::size_t index) const {
    // grab section header corresponding to either .strtab or .dynstr (general string table and dynamic string table respectively)
    auto opt_strtab = get_section(".strtab");
    if(!opt_strtab) {
        opt_strtab = get_section(".dynstr");
        if(!opt_strtab)
            return "";
    }
    return {
        reinterpret_cast<char*>(data_) + opt_strtab.value()->sh_offset + index
    };
}
