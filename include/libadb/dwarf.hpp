#ifndef ADB_DWARF_HPP
#define ADB_DWARF_HPP

#include "./detail/dwarf.h"
#include "./types.hpp"
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>
#include <optional>

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
            : pos_(pos), comp_unit_(cu), abbrev_(abbrev), attr_locs_(std::move(attr_locs)), next_(next) {}

        const compile_unit* comp_unit() { return comp_unit_; }
        const abbrev* abbrev_entry() { return abbrev_; }
        const std::byte* position() { return pos_; }
        const std::byte* next() const { return next_; }

        class children_range;
        children_range children() const;

    private:
        const std::byte* pos_ = nullptr;
        const compile_unit* comp_unit_ = nullptr;
        const abbrev* abbrev_ = nullptr;
        const std::byte* next_ = nullptr;
        std::vector<const std::byte*> attr_locs_; // locations of attributes
};

class DIE::children_range {
    public:
       children_range(DIE die) : die_(std::move(die)) {}
       class iterator {
           // Basic iterator operations
           public:
               // Types we need to expose so we can use algorithms from <algorith> and <numeric>
               using value_type = DIE;
               using reference = const DIE&;
               using pointer = const DIE*;
               using difference_type = std::ptrdiff_t;
               using iterator_category = std::forward_iterator_tag;

               iterator() = default;
               iterator(const iterator&) = default;
               iterator& operator=(const iterator&) = default;

               explicit iterator(const DIE& die);

               const DIE& operator*() const { return *die_; }
               const DIE* operator->() const { return &die_.value(); }

               iterator& operator++();
               iterator& operator++(int);

               bool operator==(const iterator& rhs) const;
               bool operator!=(const iterator& rhs) const {
                   return !(*this == rhs);
               }

            private:
               std::optional<DIE> die_;

       };

       iterator begin() const {
           if(die_.abbrev_->has_children) {
               return iterator{ die_ };
           }
           return end();
       }
       iterator end() const { return iterator{}; }

    private:
       DIE die_;
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
