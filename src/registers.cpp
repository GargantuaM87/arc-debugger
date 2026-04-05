#include "../include/libadb/registers.hpp"
#include "../include/libadb/bit.hpp"
#include "../include/libadb/process.hpp"
#include <exception>
#include <iostream>

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
    // the given function is a lambda
    std::visit([&](auto& v) {
        if(sizeof(v) == info.size) {
            auto val_bytes = as_bytes(v);
            std::copy(val_bytes, val_bytes + sizeof(v),
                bytes + info.offset);
        }
        else {
            std::cerr << "adb::register::write called with "
                "mismatched register and value sizes";
            std::terminate();
        }
    }, val);
}
