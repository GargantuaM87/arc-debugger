#ifndef ADB_DWARF_HPP
#define ADB_DWARF_HPP

#include "./detail/dwarf.h"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace adb {
    struct attr_spec {
        std::uint64_t attr;
        std::uint64_t form;
    };

    struct abbrev {
        std::uint64_t code;
        std::uint64_t tag;
        bool has_children;
        std::vector<attr_spec> attr_specs;
    };


    class elf;
    class dwarf {
        public:
            dwarf(const elf& parent);
            const elf* elf_file() const { return elf_; }

            const std::unordered_map<std::uint64_t, abbrev>& get_abbrev_table(std::size_t offset);
        private:
            const elf* elf_;
            // offsets to another map which represents the abbreviation code (key) to abbreviation table entry (value)
            std::unordered_map<std::size_t, std::unordered_map<std::uint64_t, abbrev>> abbrev_tables_;
    };
}

#endif
