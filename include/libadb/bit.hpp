#ifndef ADB_BIT_HPP
#define ADB_BIT_HPP

#include <cstring>
#include <cstddef>
#include "./types.hpp"

namespace adb {
    // Takes bytes and constructs an object from those bytes
    template <class To>
    To from_bytes(const std::byte* bytes) {
        To ret;
        std::memcpy(&ret, bytes, sizeof(To));
        return ret;
    }
    // Cast object as a pointer to bytes
    template <class From>
    std::byte* as_bytes(From& from) {
        return reinterpret_cast<std::byte*>(&from);
    }
    // Overloaded function for const arguments
    template <class From>
    const std::byte* as_bytes(const From& from) {
        return reinterpret_cast<const std::byte*>(&from);
    }

    template <class From>
    byte128 to_byte128(From src) {
        byte128 ret{}; // initialize all elements to 0
        //only copy up to the size of From so we can cast types smaller than 128/64 to the array and leave the rest of the bytes as 0
        std::memcpy(&ret, &src, sizeof(From));
        return ret;
    }
    template <class From>
    byte64 to_byte64(From src) {
        byte64 ret{};
        std::memcpy(&ret, &src, sizeof(From));
        return ret;
    }
}



#endif
