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


 class compile_unit;
 class range_list {
     public:
         range_list(const compile_unit* cu, span<const std::byte> data, file_addr base_address) : comp_unit_(cu), data_(data), base_addr_(base_address) {}

         struct entry {
             file_addr low;
             file_addr high;

             bool contains(file_addr addr) const {
                 return low <= addr and addr < high;
             }
         };

         class iterator;
         iterator begin() const;
         iterator end() const;

         bool contains(file_addr addr) const;

     private:
             const compile_unit* comp_unit_;
             span<const std::byte> data_;
             file_addr base_addr_;

};

 class range_list::iterator {
     public:
         using value_type = entry;
         using reference = const entry&;
         using pointer = const entry*;
         using difference_type = std::ptrdiff_t;
         using iterator_category = std::forward_iterator_tag;

         iterator(const compile_unit* cu, span<const std::byte> data, file_addr base_addr);

         iterator() = default;
         iterator(const iterator&) = default;
         iterator& operator=(const iterator&) = default;

         const entry& operator*() const { return current_; }
         const entry* operator->() const { return &current_; }

         bool operator==(iterator rhs) const { return pos_ == rhs.pos_; }
         bool operator !=(iterator rhs) const { return pos_ != rhs.pos_; }

         iterator& operator++();
         iterator operator++(int);

    private:
         const compile_unit* comp_unit_ = nullptr;
         span<const std::byte> data_{ nullptr, nullptr };
         file_addr base_addr_;
         const std::byte* pos_ = nullptr;
         entry current_;

 };

 class DIE;
 class attr {
     public:
         attr(const compile_unit* comp_unit, std::uint64_t type, std::uint64_t form, const std::byte* location) : comp_unit_(comp_unit), type_(type), form_(form), location_(location) {}

         std::uint64_t name() const { return type_; }
         std::uint64_t form() const { return form_; }

         file_addr as_address() const;
         std::uint32_t as_section_offset() const;
         span<const std::byte> as_block() const;
         std::uint64_t as_int() const;
         std::string_view as_string() const;
         DIE as_reference() const;


     private:
         const compile_unit* comp_unit_;
         std::uint64_t type_;
         std::uint64_t form_;
         const std::byte* location_;
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

        const compile_unit* comp_unit() const { return comp_unit_; }
        const abbrev* abbrev_entry() const { return abbrev_; }
        const std::byte* position() const { return pos_; }
        const std::byte* next() const { return next_; }

        class children_range;
        children_range children() const;
        // Whether a DIE contains an attribute of the specified type.
        bool contains(std::uint64_t attribute) const;
        // Retrieve value of an attribute.
        attr operator[](std::uint64_t attribute) const;

        file_addr low_pc() const;
        file_addr high_pc() const;

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
                // Pre-Increment
               iterator& operator++();
               // Post-Increment
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
