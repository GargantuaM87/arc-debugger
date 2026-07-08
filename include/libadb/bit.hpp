#ifndef ADB_BIT_HPP
#define ADB_BIT_HPP

#include <cstring>
#include <cstddef>
#include <string_view>
#include <vector>
#include "./types.hpp"

namespace adb {
    // Takes a pointer of bytes and constructs an object of a given type with those bytes.
    template <class To>
    To from_bytes(const std::byte* bytes) {
        To ret;
        std::memcpy(&ret, bytes, sizeof(To));
        return ret;
    }
    // Cast an object of a type to a pointer to bytes.
    template <class From>
    std::byte* as_bytes(From& from) {
        return reinterpret_cast<std::byte*>(&from);
    }
    // Cast an object of a type to a pointer to bytes.
    template <class From>
    const std::byte* as_bytes(const From& from) {
        return reinterpret_cast<const std::byte*>(&from);
    }
    // Convert an object of a type to a 16-byte array of 128 bits.
    template <class From>
    byte128 to_byte128(From src) {
        byte128 ret{}; // initialize all elements to 0
        //only copy up to the size of From so we can cast types smaller than 128/64 to the array and leave the rest of the bytes as 0
        std::memcpy(&ret, &src, sizeof(From));
        return ret;
    }
    // Convert an object of a type to an 8-byte array of 64 bits.
    template <class From>
    byte64 to_byte64(From src) {
        byte64 ret{};
        std::memcpy(&ret, &src, sizeof(From));
        return ret;
    }
    // Cast a pointer to bytes to a pointer to characters (a string).
    inline std::string_view to_string_view(const std::byte* data, std::size_t size) {
        return {reinterpret_cast<const char*>(data), size };
    }
    // Cast a pointer to bytes to a pointer to characters (a string).
    inline std::string_view to_string_view(const std::vector<std::byte>& data) {
        return to_string_view(data.data(), data.size());
    }
}



#endif
