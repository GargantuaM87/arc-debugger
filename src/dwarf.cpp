#include "../include/libadb/dwarf.hpp"
#include "../include/libadb/types.hpp"
#include "../include/libadb/bit.hpp"
#include "../include/libadb/elf.hpp"
#include "../include/libadb/error.hpp"
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <algorithm>
#include <vector>

namespace {
    class cursor {
        public:
            explicit cursor(adb::span<const std::byte> data) : data_(data), pos_(data.begin()) {}

        cursor& operator++() { ++pos_; return *this; }
        cursor& operator+=(std::size_t size) { pos_ += size; return *this; }

        // Parse any fixed-width integer
        template <class T>
            T fixed_int() {
                auto t = adb::from_bytes<T>(pos_);
                pos_ += sizeof(T);
                return t;
            }

        // Bunch of conversion functions for specific types
        std::uint8_t u8() { return fixed_int<std::int8_t>(); }
        std::uint16_t u16() { return fixed_int<std::int16_t>(); }
        std::uint32_t u32() { return fixed_int<std::int32_t>(); }
        std::uint64_t u64() { return fixed_int<std::int64_t>(); }
        std::int8_t s8() { return fixed_int<std::int8_t>(); }
        std::int16_t s16() { return fixed_int<std::int16_t>(); }
        std::int32_t s32() { return fixed_int<std::int32_t>(); }
        std::int64_t s64() { return fixed_int<std::int64_t>(); }

        // For parsing strings
        std::string_view string() {
            auto null_terminator = std::find(pos_, data_.end(), std::byte{0});
            std::string_view output(reinterpret_cast<const char*>(pos_), null_terminator - pos_);
            pos_ = null_terminator + 1;
            return output;
        }

        // For parsing ULEB128 format
        std::uint64_t uleb128() {
            std::uint64_t result = 0;
            int shift = 0; // how much to shift the next byte to the left

            std::uint8_t byte = 0;

            do {
                byte = u8();
                auto masked = static_cast<uint64_t>(byte & 0x7f); // mask off first bit
                result |= masked << shift;
                shift += 7;
            } while ((byte & 0x80) != 0);
            return result;
        }

        std::uint64_t sleb128() {
            std::int64_t result = 0;
            int shift = 0;

            std::uint8_t byte = 0;

            do {
                byte = u8();
                auto masked = static_cast<uint64_t>(byte & 0x7f);
                result |= masked << shift;
                shift += 7;
            } while ((byte & 0x80) != 0);
            // checking if we filled the result integer
            // also checking whether the number should be negative by checking whether the last byte read has a 1 in its second-highest position
            if ((shift < sizeof(result) * 8) and (byte & 0x40)) {
                result |= (~static_cast<std::uint64_t>(0) << shift);
            }

            return result;
        }

        void skip_form(std::uint64_t form) {
            switch(form) {
                // presence of an attribute
                case DW_FORM_flag_present:
                    break;
                // Fixed-width forms
                case DW_FORM_data1:
                case DW_FORM_ref1:
                case DW_FORM_flag:
                    pos_ += 1; break;

                case DW_FORM_data2:
                case DW_FORM_ref2:
                    pos_ += 2; break;

                case DW_FORM_data4:
                case DW_FORM_ref4:
                case DW_FORM_ref_addr:
                case DW_FORM_sec_offset:
                case DW_FORM_strp:
                    pos_ += 4; break;

                case DW_FORM_data8:
                case DW_FORM_addr:
                    pos_ += 8; break;

                // LEB128 forms
                case DW_FORM_sdata:
                    sleb128(); break;
                case DW_FORM_udata:
                case DW_FORM_ref_udata:
                    uleb128(); break;

                // Data block forms whose size is encoded as a ULEB128 or fixed-sized integer
                case DW_FORM_block1:
                    pos_ += u8(); break;
                case DW_FORM_block2:
                    pos_ += u16(); break;
                case DW_FORM_block4:
                    pos_ += u32(); break;
                case DW_FORM_block:
                case DW_FORM_exprloc:
                    pos_ += uleb128();
                    break;


                // Strings
                case DW_FORM_string:
                    while(!finished() && *pos_ != std::byte(0)) {
                        ++pos_;
                    }
                    ++pos_;
                    break;

                // Indirect form
                case DW_FORM_indirect:
                    skip_form(uleb128());
                    break;

                default: adb::error::send("Unrecognized DWARF form");
            }
        }


        const std::byte* position() const { return pos_; }

        bool finished() const {
            return pos_ >= data_.end();
        }

        private:
            adb::span<const std::byte> data_;
            const std::byte* pos_;
    };
}

namespace {
    std::unordered_map<std::uint64_t, adb::abbrev> parse_abbrev_table(const adb::elf& obj, std::size_t offset) {
        cursor cur(obj.get_section_contents(".debug_abbrev")); // get pre-defined section that cross references with DWARF file's content
        cur += offset;

        std::unordered_map<uint64_t, adb::abbrev> table;
        std::uint64_t code = 0;

        // extract ULEB128 code and tag
        // 1-byte uint for children flag
        // list of ULEB128 pairs of attribute types and forms, terminated by a pair of 0s
        do {
            // Parse one entry
            code = cur.uleb128();
            auto tag = cur.uleb128();
            auto has_children = static_cast<bool>(cur.u8());

            std::vector<adb::attr_spec> attr_specs;
            std::uint64_t attr = 0;

            do {
                attr = cur.uleb128();
                auto form = cur.uleb128();

                if (attr != 0) {
                    attr_specs.push_back(adb::attr_spec{ attr, form });
                }
            } while(attr != 0);

            if(code != 0) {
                table.emplace(code, adb::abbrev {code, tag, has_children, std::move(attr_specs)});
            }

        } while (code != 0);

        return table;
    }

    std::unique_ptr<adb::compile_unit> parse_compile_unit(adb::dwarf& dwarf, const adb::elf& obj, cursor cur) {
        auto start = cur.position();
        auto size = cur.u32(); // byte size of the info for compile unit
        auto version = cur.u16(); // version of DWARF
        auto abbrev = cur.u32(); // offset into abbreviation table
        auto address_size = cur.u8(); // size of an address (should be 8)

        if(size == 0xffffffff) {
            adb::error::send("Only DWARF32 is supported");
        }
        if(version != 4) {
            adb::error::send("Only DWARF version 4 is supported");
        }
        if(address_size != 8) {
            adb::error::send("Invalid address size for DWARF");
        }

        size += sizeof(std::uint32_t);

        adb::span<const std::byte> data = {start, size};
        return std::make_unique<adb::compile_unit>(dwarf, data, abbrev);

    }

    std::vector<std::unique_ptr<adb::compile_unit>> parse_compile_units(adb::dwarf& dwarf, const adb::elf& obj) {
        auto debug_info = obj.get_section_contents(".debug_info");
        cursor cur(debug_info);

        std::vector<std::unique_ptr<adb::compile_unit>> units;
        while(!cur.finished()) {
            auto unit = parse_compile_unit(dwarf, obj, cur);
            cur += unit->data().size();
            units.push_back(std::move(unit));
        }
        return units;
    }

    adb::DIE parse_die(const adb::compile_unit& cu, cursor cur) {
        auto pos = cur.position();
        auto abbrev_code = cur.uleb128();

        if(abbrev_code == 0) { // Means this DIE is null
            auto next = cur.position(); // store the position of the cursor and send it into the DIE
            return adb::DIE { next };
        }
        // If the DIE isn't null...
        auto& abbrev_table = cu.abbrev_table();
        auto& abbrev = abbrev_table.at(abbrev_code); // Find entry for DIE

        std::vector<const std::byte*> attr_locs;
        attr_locs.reserve(abbrev.attr_specs.size()); // remove unnecessary dynamic memory allocations
        for(auto& attr : abbrev.attr_specs) {
            attr_locs.push_back(cur.position());
            cur.skip_form(attr.form); // skip over in bytes
        }
        auto next = cur.position();
        return adb::DIE(pos, &cu, &abbrev, std::move(attr_locs), next);
    }
}

const std::unordered_map<std::uint64_t, adb::abbrev>& adb::dwarf::get_abbrev_table(std::size_t offset) {
    if(!abbrev_tables_.count(offset)) {
        abbrev_tables_.emplace(offset, parse_abbrev_table(*elf_, offset));
    }
    return abbrev_tables_.at(offset);
}

const std::unordered_map<uint64_t, adb::abbrev>& adb::compile_unit::abbrev_table() const {
    return parent_->get_abbrev_table(offset_);
}

adb::dwarf::dwarf(const adb::elf& parent) : elf_(&parent) {
    compile_units_ = parse_compile_units(*this, parent);
}

adb::DIE adb::compile_unit::root() const {
    std::size_t header_size = 11; // the size of a compile unit header is 11 bytes, so the DIE for it is 11 bytes past the beginning of its data_ member.
    cursor cur( {data_.begin() + header_size, data_.end()});
    return parse_die(*this, cur);
}

adb::DIE::children_range::iterator::iterator(const adb::DIE& die) {
   cursor next_cur( {die.next_, die.comp_unit_->data().end() });
   die_ = parse_die(*die.comp_unit_, next_cur);
}

bool adb::DIE::children_range::iterator::operator==(const iterator& rhs) const {
    // an iterator is null if it doesn't have a DIE or the stored DIE has an abbrev code of 0
    auto lhs_null = !die_.has_value() or !die_->abbrev_entry();
    auto rhs_null = !rhs.die_.has_value() or !rhs.die_->abbrev_entry();
    if(lhs_null and rhs_null) return true; // if both iterators are null, then naturally, they're equal
    if (lhs_null or rhs_null) return false; // if only one is null

    // match abbrev codes and next pointers
    return die_->abbrev_ == rhs->abbrev_ and die_->next() == rhs->next();
}

adb::DIE::children_range::iterator& adb::DIE::children_range::iterator::operator++() {
    if(!die_.has_value() or !die_->abbrev_) return *this;
    // parse the siblings of the DIE
    if(!die_->abbrev_->has_children) {
        cursor next_cur( {die_->next_, die_->comp_unit_->data().end()});
        die_ = parse_die(*die_->comp_unit_, next_cur);
    }
    else if(die_->contains(DW_AT_sibling)) {
        die_ = die_.value()[DW_AT_sibling].as_reference();
    }
    // parse children of the DIE
    else {
        iterator sub_children(*die_); // advance over children until we hit null terminator
        while (sub_children->abbrev_) ++sub_children;

        cursor next_cur( {sub_children->next_, die_->comp_unit_->data().end()} );
        die_ = parse_die(*die_->comp_unit_, next_cur);
    }
    return *this;
}

adb::DIE::children_range::iterator& adb::DIE::children_range::iterator::operator++(int) {
    auto temp = *this;
    ++(*this);
    return temp;
}

adb::DIE::children_range adb::DIE::children() const {
    return children_range(*this);
}

bool adb::DIE::contains(std::uint64_t attribute) const {
    auto& specs = abbrev_->attr_specs;
    return std::find_if(begin(specs), end(specs),
            [=](auto spec) { return spec.attr == attribute; }) != end(specs);
}

adb::attr adb::DIE::operator[](std::uint64_t attribute) const {
    auto& specs = abbrev_->attr_specs;
    for( std::size_t i = 0; i < specs.size(); ++i) {
        if(specs[i].attr == attribute) {
            // pointer to compilation unit, attribute's type, attribute's form, and where the bytes for the attribute's value begin
            return { comp_unit_, specs[i].attr, specs[i].form, attr_locs_[i] };
        }
    }
    error::send("Attribute not found");
}

adb::file_addr adb::DIE::low_pc() const {
    return (*this)[DW_AT_low_pc].as_address(); // always a file address. Points to the ELF file that contains this DWARF informaton.
}

adb::file_addr adb::DIE::high_pc() const {
    auto attr = (*this)[DW_AT_high_pc];
    std::uint64_t addr;
    // directly extract the address
    if(attr.form() == DW_FORM_addr) {
        addr = attr.as_address();
    }
    // form is an offset from the low program counter
    else {
        addr = low_pc() + attr.as_int();
    }

    return file_addr {
        *comp_unit_->dwarf_info()->elf_file(), addr
    };
}

adb::file_addr adb::attr::as_address() const {
    cursor cur( {location_, comp_unit_->data().end()} );

    if(form_ != DW_FORM_addr) error::send("Invalid address type");
    auto elf = comp_unit_->dwarf_info()->elf_file();
    return file_addr{*elf, cur.u64()};
}

std::uint32_t adb::attr::as_section_offset() const {
    cursor cur( {location_, comp_unit_->data().end() } );

    if(form_ != DW_FORM_sec_offset) error::send("Invalid section offset type");
    return cur.u32();
}

std::uint64_t adb::attr::as_int() const {
    cursor cur( {location_, comp_unit_->data().end() });
    switch (form_) {
        case DW_FORM_data1:
            return cur.u8();
        case DW_FORM_data2:
            return cur.u16();
        case DW_FORM_data4:
            return cur.u32();
        case DW_FORM_data8:
            return cur.u64();
        case DW_FORM_udata:
            return cur.uleb128();
        default:
            error::send("Invalid integer type");
    }
}

adb::span<const std::byte> adb::attr::as_block() const {
    std::size_t size;

    cursor cur( {location_, comp_unit_->data().end() });

    switch(form_) {
        case DW_FORM_block1:
            size = cur.u8();
            break;
        case DW_FORM_block2:
            size = cur.u16();
            break;
        case DW_FORM_block4:
            size = cur.u32();
            break;
        case DW_FORM_block:
            size = cur.uleb128();
            break;
        default:
            error::send("Invalid block type");
    }
    return { cur.position(), size};
}

adb::DIE adb::attr::as_reference() const {
    cursor cur( {location_, comp_unit_->data().end()} );
    std::size_t offset; // based on the start of the current DIE's compile unit

    switch (form_) {
        case DW_FORM_ref1:
            offset = cur.u8();
            break;
        case DW_FORM_ref2:
            offset = cur.u16();
            break;
        case DW_FORM_ref4:
            offset = cur.u32();
            break;
        case DW_FORM_ref8:
            offset = cur.u64();
            break;
        case DW_FORM_udata:
            offset = cur.uleb128();
            break;
        // reference data in other compile units, so its offset is relative to .debug_info
        case DW_FORM_ref_addr:
            {
                offset = cur.u32();
                auto section = comp_unit_->dwarf_info()->elf_file()->get_section_contents(".debug_info");
                // get the position in bytes?
                auto die_pos = section.begin() + offset; // calculate position of referenced DIE
                auto& comp_units = comp_unit_->dwarf_info()->compile_units();
                // look for the compile unit whose data range includes the DIE we're looking for
                auto cu_finder = [=](auto& cu) {
                    return cu->data().begin() <= die_pos and cu->data().end() > die_pos;
                };

                auto cu_for_offet = std::find_if(begin(comp_units), end(comp_units), cu_finder);
                cursor ref_cur( {die_pos, cu_for_offet->get()->data().end()} );
                return parse_die(**cu_for_offet, ref_cur);
            }
        default:
            error::send("Invalid reference type");
    }

    cursor ref_cur( {comp_unit_->data().begin() + offset, comp_unit_->data().end()} );
    return parse_die(*comp_unit_, ref_cur); // parse DIE at position given with offset
}

std::string_view adb::attr::as_string() const {
    cursor cur({location_, comp_unit_->data().end()});
    switch (form_) {
        case DW_FORM_string:
            return cur.string();
        case DW_FORM_strp: {
                               auto offset = cur.u32();
                               auto strtab = comp_unit_->dwarf_info()->elf_file()->get_section_contents(".debug_str");
                               cursor strtab_cur( {strtab.begin() + offset, strtab.end()} );
                               return strtab_cur.string();
                           }
        default:
            error::send("Invalid string type");
    }
}

