#ifndef ADB_REGISTER_INFO_HPP
#define ADB_REGISTER_INFO_HPP

#include <algorithm>
#include "./error.hpp"
#include <cstdint>
#include <cstddef>
#include <string_view>
#include <sys/user.h>

namespace adb {
    enum class register_id { // gives each register its own enumerator value
        #define DEFINE_REGISTER(name,dwarf_id,size,offset,type,format) name
        #include "./detail/registers.inc"
        #undef DEFINE_REGISTER
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

    inline constexpr const register_info g_register_infos[] = {
        #define DEFINE_REGISTER(name,dwarf_id,size,offset,type,format) \
            { register_id::name, #name, dwarf_id, size, offset, type, format }
        #include "./detail/registers.inc"
        #undef DEFINE_REGISTER
    };
}
// parsing register info
namespace adb {
    // takes a comparator function and uses it to find a specific register info entry
    template <class F>
    const register_info& register_info_by(F f) {
        auto it = std::find_if(
            std::begin(g_register_infos),
            std::end(g_register_infos), f);

        if(it == std::end(g_register_infos))
            error::send("Can't find register info");

        return *it;
    }
    // functions to find register by ID, name, or DWARF ID
    // inline keyword allows multiple definitons of one function to exist
    // so we can define the definition in this header file instead of an implementation file
    inline const register_info& register_info_by_id(register_id id) {
        return register_info_by([id](auto& i) {return i.id == id; });
    }
    inline const register_info& register_info_by_name(std::string_view name) {
        return register_info_by([name](auto& i) { return i.name == name; });
    }
    inline const register_info& register_info_by_dwarf(std::int32_t dwarf_id) {
        return register_info_by([dwarf_id](auto& i) { return i.dwarf_id == dwarf_id; });
    }
}

#endif
