#ifndef ADB_DISASSEMBLER_HPP
#define ADB_DISASSEMBLER_HPP

#include "./process.hpp"
#include "types.hpp"
#include <optional>

namespace adb {
    class disassembler {
        // holds string representation of the instruction
        // and memory address of the binary instruction
        struct instruction {
            virt_addr address;
            std::string text;
        };
    public:
        disassembler(process& proc) : process_(&proc) {}

        std::vector<instruction> disassemble(std::size_t n_instructions, std::optional<virt_addr> address = std::nullopt);
    private:
        process* process_;
    };
}

#endif
