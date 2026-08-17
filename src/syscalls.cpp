#include "../include/libadb/syscalls.hpp"
#include "../include/libadb/error.hpp"
#include <string_view>
#include <unordered_map>

namespace {
    const std::unordered_map<std::string_view, int> syscall_name_map = {
        #define DEFINE_SYSCALL(name, id) {#name, id},
        #include "./include/syscalls.inc"
        #undef DEFINE_SYSCALL
    };
}

std::string_view adb::syscall_id_to_name(int id) {
    switch(id) {
        #define DEFINE_SYSCALL(name, id) case id: return #name;
        #include "./include/syscalls.inc"
        #undef DEFINE_SYSCALL
    default: adb::error::send("No such syscall");
    }
}

int adb::syscall_name_to_id(std::string_view name) {
    if(syscall_name_map.count(name) != 1)
        adb::error::send("No such syscall");
    return syscall_name_map.at(name);
}
