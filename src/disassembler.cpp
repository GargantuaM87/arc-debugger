#include <Zycore/Types.h>
#include <Zydis/Disassembler.h>
#include <Zydis/SharedTypes.h>
#include <Zydis/Zydis.h>
#include <cstddef>
#include "../include/libadb/disassembler.hpp"

std::vector<adb::disassembler::instruction> adb::disassembler::disassemble(std::size_t n_instructions, std::optional<virt_addr> address)
{
    std::vector<instruction> ret;
    ret.reserve(n_instructions); // reserve enough memory for the amount of instructions

    if(!address) {
        address.emplace(process_->get_pc()); // constructs a new object within std::optional (in this case, the program counter)
    }
    // largest x64 instruction is 15 bytes
    auto code = process_->read_memory_without_traps(*address, n_instructions * 15);

    ZyanUSize offset = 0;
    ZydisDisassembledInstruction instr;
    // loop while populating instr with disassembled instruction information
    while(ZYAN_SUCCESS(ZydisDisassembleATT (
        ZYDIS_MACHINE_MODE_LONG_64, address->addr(), code.data() + offset, code.size() - offset, &instr))
    and n_instructions > 0) {
        ret.push_back(instruction { *address, std::string(instr.text)}); //push back instruction
        offset += instr.info.length;
        *address += instr.info.length;
        --n_instructions;
    }
    return ret;
}
