#ifndef ADB_TARGET_HPP
#define ADB_TARGET_HPP

#include <filesystem>
#include <memory.h>
#include <memory>
#include <optional>
#include <sys/types.h>
#include "elf.hpp"
#include "process.hpp"

namespace adb {
    class target {
        public:
            target() = delete;
            target(const target&) = delete;
            target& operator=(const target&) = delete;
            // Launch a process with the given path while creating an ELF file object. The standard output will be replaced by the given stdout_replacement.
            static std::unique_ptr<target> launch(std::filesystem::path path, std::optional<int> stdout_replacement = std::nullopt);
            // Attatch to an already running process with the given PID while creating an ELF file object associated with that process.
            static std::unique_ptr<target> attatch(pid_t pid);
            // Return a reference of this target's process.
            process& get_process() { return *process_; }
            // Return a reference of this target's process.
            const process& get_process() const { return *process_; }
            // Return a reference of this target's ELF file.
            elf& get_elf() { return *elf_; }
            // Return a reference of this target's ELF file.
            const elf& get_elf() const { return *elf_; }

        private:
            target(std::unique_ptr<process> proc, std::unique_ptr<elf> obj) : process_(std::move(proc)), elf_(std::move(obj))
        {}

            std::unique_ptr<process> process_;
            std::unique_ptr<elf> elf_;
    };
}

#endif
