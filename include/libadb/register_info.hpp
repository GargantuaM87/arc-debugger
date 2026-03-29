#ifndef ADB_REGISTER_INFO_HPP
#define ADB_REGISTER_INFO_HPP

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <sys/user.h>

namespace adb {
    enum class register_id { // gives each register its own enumerator value

    };

    enum class register_type {
        gpr, // general purpose register
        sub_gpr, // subregister of a gpr
        fpr, // floating-point register
        dr // debug register
    };

    enum class register_format {
        uint,
        double_float,
        long_double,
        vector
    };

    struct register_info {
        register_id id;
        std::string_view name;
        std::int32_t dwarf_id;
        std::size_t size;
        std::size_t offset;
        register_type type;
        register_format format;
    };

    inline constexpr const register_info g_register_infos = {

    };
}

#endif
