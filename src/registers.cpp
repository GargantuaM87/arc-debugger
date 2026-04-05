#include "../include/libadb/registers.hpp"
#include "../include/libadb/bit.hpp"
#include "../include/libadb/process.hpp"
#include <cstdint>
#include <exception>
#include <iostream>
#include <type_traits>
#include <algorithm>

namespace {
    template <class T>
    adb::byte128 widen(const adb::register_info& info, T t) {
        using namespace adb;
        // if a floating point value, cast it to the widest relevant floating point type before casting to a byte128
        if constexpr (std::is_floating_point_v<T>) { // this expression runs at compile tme
            if(info.format == register_format::double_float)
                return to_byte128(static_cast<double>(t));
            if(info.format == register_format::long_double)
                return to_byte128(static_cast<long double>(t));
        }
        else if constexpr (std::is_signed_v<T>) {
            if(info.format == register_format::uint) {
                switch(info.size) {
                    case 2: return to_byte128(static_cast<std::int16_t>(t));
                    case 4: return to_byte128(static_cast<std::int32_t>(t));
                    case 8: return to_byte128(static_cast<std::int64_t>(t));
                }
            }
        }
        return to_byte128(t);
    }
}

/**
 * 1. Get pointer to raw bytes of data
 * 2. Figure out where the bytes of the register data is inside the user struct with info.offset
 * 3. Convert bytes into type specified based on info.size and info.format fields
 */
adb::registers::value adb::registers::read(const register_info& info) const {
    auto bytes = as_bytes(data_);

    if(info.format == register_format::uint) {
        switch (info.size) {
            // offset helps find where the bytes of the register data lives
            case 1: return from_bytes<std::uint8_t>(bytes + info.offset);
            case 2: return from_bytes<std::uint16_t>(bytes + info.offset);
            case 4: return from_bytes<std::uint32_t>(bytes + info.offset);
            case 8: return from_bytes<std::uint64_t>(bytes + info.offset);
            default: adb::error::send("Unexpected register size");
        }
    }
    else if(info.format == register_format::double_float) {
        return from_bytes<double>(bytes + info.offset);
    }
    else if(info.format == register_format::long_double) {
        return from_bytes<long double>(bytes + info.offset);
    }
    else if(info.format == register_format::vector and info.size == 8) {
        return from_bytes<byte64>(bytes + info.offset);
    }
    else {
        return from_bytes<byte128>(bytes + info.offset);
    }
}
/**
 * 1. Get raw bytes of data as a pointer
 */
void adb::registers::write(const register_info& info, value val) {
    auto bytes = as_bytes(data_);
    // takes a function and a variant
    // calls the given function with the value stored in the variant
    // the given function is a generic lambda
    std::visit([&](auto& v) {
        if(sizeof(v) <= info.size) {
            auto wide = widen(info, v); // can write smaller-sized values into registers safely
            auto val_bytes = as_bytes(wide);
            std::copy(val_bytes, val_bytes + info.size, // bytes of the stored value
                bytes + info.offset);                   // then copy them into the correct place in the struct of registers
        }
        else {
            std::cerr << "adb::register::write called with "
                "mismatched register and value sizes";
            std::terminate();
        }
    }, val);


    if(info.type == register_type::fpr) {
        proc_->write_fprs(data_.i387);
    }
    else {
        // bitwise AND operation to align high 8-bit register addresses
        // Will set the lowest 3 bits in the adress to 0
        auto aligned_offset = info.offset &~0b111;
        proc_->write_user_area(aligned_offset, from_bytes<std::uint64_t>(bytes + aligned_offset));
    }
}
