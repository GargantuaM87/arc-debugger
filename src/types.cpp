#include "../include/libadb/types.hpp"
#include "../include/libadb/elf.hpp"
#include <cassert>

adb::virt_addr adb::file_addr::to_virt_addr() const {
    assert(elf_ && "to_virt_addr called on null address");
    auto section = elf_->get_section_with_addr(*this);
    if(!section) return virt_addr{};

    return virt_addr { addr_ + elf_->load_bias().addr() };
}

adb::file_addr adb::virt_addr::to_file_addr(const elf& obj) const {
    auto section = obj.get_section_with_addr(*this);
    if(!section) return file_addr {};

    return file_addr {obj, addr_ - obj.load_bias().addr() };
}
