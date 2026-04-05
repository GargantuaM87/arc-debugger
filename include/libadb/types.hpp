#ifndef ADB_TYPES_HPP
#define ADB_TYPES_HPP

#include <array>
#include <cstddef>

namespace adb {
    // using keyword allows for type aliases
    using byte64 = std::array<std::byte, 8>;
    using byte128 = std::array<std::byte, 16>;
}

#endif
