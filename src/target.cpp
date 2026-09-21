#include "../include/libadb/target.hpp"
#include "../include/libadb/types.hpp"
#include <elf.h>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace {
    std::unique_ptr<adb::elf> create_loaded_elf(const adb::process& proc, const std::filesystem::path& path) {
        auto auxv = proc.get_auxv(); // obtaining the auxiliary vector
        auto obj = std::make_unique<adb::elf>(path);
        /* setting load bias of .text section by subtracting the load address of the entry point in the ELF header (file address)
        *  from the actual load address of the entry point (virtual address)
        */
        obj->notify_loaded(adb::virt_addr(auxv[AT_ENTRY] - obj->header().e_entry));
        return obj;
    }
}

std::unique_ptr<adb::target> adb::target::launch(std::filesystem::path path, std::optional<int> stdout_replacement) {
    auto proc = process::launch(path, true, stdout_replacement);
    auto obj = create_loaded_elf(*proc, path);
    return std::unique_ptr<target>(new target(std::move(proc), std::move(obj)));
}

std::unique_ptr<adb::target> adb::target::attatch(pid_t pid) {
    std::filesystem::path elf_path = std::filesystem::path("/proc") / std::to_string(pid) / "exe";
    auto proc = process::attatch(pid);
    auto obj = create_loaded_elf(*proc, elf_path);
    return std::unique_ptr<target>(new target(std::move(proc), std::move(obj)));
}
