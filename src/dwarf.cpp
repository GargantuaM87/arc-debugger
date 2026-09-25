#include "../include/libadb/dwarf.hpp"
#include "../include/libadb/types.hpp"
#include "../include/libadb/bit.hpp"
#include "../include/libadb/elf.hpp"
#include <cstdint>
#include <iterator>
#include <string_view>
#include <unordered_map>
#include <algorithm>

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


