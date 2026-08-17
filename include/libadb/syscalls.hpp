#ifndef ADB_SYSCALLS_HPP
#define ADB_SYSCALLS_HPP

#include <string_view>

namespace adb {
    std::string_view syscall_id_to_name(int id);
    int syscall_name_to_id(std::string_view name);
}

#endif
