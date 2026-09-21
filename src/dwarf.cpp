#include "../include/libadb/dwarf.hpp"
#include "../include/libadb/types.hpp"
#include "../include/libadb/bit.hpp"
#include <cstdint>
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
            int shift = 0;

            std::uint8_t byte = 0;

            do {
                byte = u8();
                auto masked = static_cast<uint64_t>(byte & 0x7f);
                result |= masked << shift;
                shift += 7;
            } while ((byte & 0x80) != 0);
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

const std::unordered_map<std::uint64_t, adb::abbrev>& adb::dwarf::get_abbrev_table(std::size_t offset) {
    if(!abbrev_tables_.count(offset)) {
        abbrev_tables_.emplace(offset, parse_abbrev_table(*elf_, offset));
    }
    return abbrev_tables_.at(offset);
}


