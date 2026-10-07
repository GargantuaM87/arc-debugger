#ifndef ADB_DWARF_HPP
#define ADB_DWARF_HPP

#include "./detail/dwarf.h"
#include "./types.hpp"
#include <cstdint>
#include <memory>
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

 class dwarf;
 class DIE;
    class compile_unit {
        public:
            compile_unit(dwarf& parent, span<const std::byte> data, std::size_t abbrev_offset) : parent_(&parent), data_(data), offset_(abbrev_offset) {}

            const dwarf* dwarf_info() const { return parent_; }
            span<const std::byte> data() const { return data_; }

            const std::unordered_map<std::uint64_t, abbrev>& abbrev_table() const;

            DIE root() const;
        private:
           dwarf* parent_;
           span<const std::byte> data_;
           std::size_t offset_;
    };
// Attributes:
    // pointer to compile unit to which DIE belongs to
    // A pointer to its abbreviation table entry
    // A pointer to the DIE immediately after this one (whether it be a child or a sibling)
class DIE {
    public:
        explicit DIE(const std::byte* next) : next_(next) {}
        DIE(const std::byte* pos, const compile_unit* cu, const abbrev* abbrev, std::vector<const std::byte*> attr_locs, const std::byte* next)
            : pos_(pos), comp_unit_(cu), abbrev_(abbrev), attr_locs_(attr_locs), next_(next) {}

    private:
        const std::byte* pos_ = nullptr;
        const compile_unit* comp_unit_ = nullptr;
        const abbrev* abbrev_ = nullptr;
        const std::byte* next_ = nullptr;
        std::vector<const std::byte*> attr_locs_; // locations of attributes
};

    class elf;
    class dwarf {
        public:
            dwarf(const elf& parent);
            const elf* elf_file() const { return elf_; }

            const std::unordered_map<std::uint64_t, abbrev>& get_abbrev_table(std::size_t offset);

            const std::vector<std::unique_ptr<compile_unit>>& compile_units() const { return compile_units_; }
        private:
            const elf* elf_;
            // offsets to another map which represents the abbreviation code (key) to abbreviation table entry (value)
            std::unordered_map<std::size_t, std::unordered_map<std::uint64_t, abbrev>> abbrev_tables_;
            std::vector<std::unique_ptr<compile_unit>> compile_units_;
    };

}

#endif
