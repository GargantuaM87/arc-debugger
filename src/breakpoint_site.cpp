#include "../include/libadb/breakpoint_site.hpp"

namespace {
    auto get_next_id() {
        static adb::breakpoint_site::id_type id = 0; // subsequent calls will use this same object
        return ++id;
    }
}

adb::breakpoint_site::breakpoint_site(process& proc, virt_addr address) : process_{&proc}, address_{address}, is_enabled_{false}, saved_data_{} {
    id_ = get_next_id();
}
