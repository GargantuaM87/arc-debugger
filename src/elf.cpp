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
#include <cxxabi.h>
#include <algorithm>
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
    build_symbol_maps();
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

const Elf64_Shdr* adb::elf::get_section_with_addr(adb::file_addr addr) const {
    if(addr.elf_file() != this) return nullptr;

    for(auto& section : section_headers) {
        if(section.sh_addr <= addr.addr() and section.sh_addr + section.sh_size > addr.addr()) {
            return &section;
        }
    }
    return nullptr;
}

const Elf64_Shdr* adb::elf::get_section_with_addr(adb::virt_addr addr) const {
    for(auto& section : section_headers) {
        if(load_bias_ + section.sh_addr <= addr and load_bias_ + section.sh_addr + section.sh_size > addr) {
            return &section;
        }
    }
    return nullptr;
}

std::optional<adb::file_addr> adb::elf::get_section_start_addr(std::string_view name) const {
    if(auto sect = get_section(name); sect) {
        return file_addr { *this, sect.value()->sh_addr };
    }
    return std::nullopt;
}

void adb::elf::parse_symbol_table() {
    auto opt_symtab = get_section(".symtab"); // complete symbol table

    if(!opt_symtab) {
        opt_symtab = get_section("dynsym"); // abbreviated symbol table
        if(!opt_symtab)
            return;
    }

    auto symtab = *opt_symtab;
    symbol_table.resize(symtab->sh_size / symtab->sh_entsize);
    std::copy(data_ + symtab->sh_offset, data_ + symtab->sh_offset + symtab->sh_size, reinterpret_cast<std::byte*>(symbol_table.data()));
}

void adb::elf::build_symbol_maps() {
    for(auto& symbol : symbol_table) {
        auto mangled_name = get_string(symbol.st_name);
        int demangle_status;
        auto demanged_name = abi::__cxa_demangle(mangled_name.data(), nullptr, nullptr, &demangle_status);

        if(demangle_status == 0) { // testing for potential mangled symbol then attempting to demangle the name
            symbol_name_map.insert({ demanged_name, &symbol });
            free(demanged_name);
        }
        symbol_name_map.insert( {mangled_name, &symbol} ); // add an entry for the symbol's (potentially) mangled name anyway
        // if the symbol has an address and a name, and doesn't point to thread-local storage
        if(symbol.st_value != 0 and symbol.st_name != 0 and ELF64_ST_TYPE(symbol.st_info) != STT_TLS) {
            auto addr_range = std::pair(file_addr{*this, symbol.st_value}, file_addr{*this, symbol.st_value + symbol.st_size});
            symbol_addr_map.insert({addr_range, &symbol});
        }
    }
}

std::vector<const Elf64_Sym*> adb::elf::get_symbols_by_name(std::string_view name) const {
    // structured binding
    auto [begin, end] = symbol_name_map.equal_range(name);

    std::vector<const Elf64_Sym*> ret;
    std::transform(begin, end, std::back_inserter(ret), [](auto& pair) { return pair.second; });
    return ret;
}

std::optional<const Elf64_Sym*> adb::elf::get_symbol_at_addr(adb::file_addr addr) const {
    if(addr.elf_file() != this)
        return std::nullopt;

    adb::file_addr null_addr;
    // finding symbol based on start address
    auto it = symbol_addr_map.find( {addr, null_addr} );
    if(it == end(symbol_addr_map))
        return std::nullopt;
    return it->second;
}

std::optional<const Elf64_Sym*> adb::elf::get_symbol_at_addr(adb::virt_addr addr) const {
    // defer to previous method above
    return get_symbol_at_addr(addr.to_file_addr(*this));
}

std::optional<const Elf64_Sym*> adb::elf::get_symbol_with_addr(adb::file_addr addr) const {
    if(addr.elf_file() != this or symbol_addr_map.empty())
        return std::nullopt;

    file_addr null_addr;
    // find element that is equal to or greater than the given key
    auto it = symbol_addr_map.lower_bound( {addr, null_addr} );
    // if the address is the start address of the symbol
    if(it != end(symbol_addr_map)) {
        if(auto [key, value] = *it; key.first == addr) {
            return value;
        }
    }
    // there is no entry preceding the given address
    if(it == begin(symbol_addr_map))
        return std::nullopt;

    it--;
    // if the symbol is earlier than the given address, but spans past it
    if(auto [key, value] = *it; key.first < addr and key.second > addr) {
        return value;
    }

    return std::nullopt;
}

std::optional<const Elf64_Sym*> adb::elf::get_symbol_with_addr(adb::virt_addr addr) const {
    return get_symbol_with_addr(addr.to_file_addr(*this));
}


