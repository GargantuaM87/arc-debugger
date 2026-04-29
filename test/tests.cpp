#include <catch2/catch_test_macros.hpp>
#include "../include/libadb/process.hpp"
#include "../include/libadb/error.hpp"
#include "../include/libadb/pipe.hpp"
#include "../include/libadb/bit.hpp"
#include <cerrno>
#include <cstdint>
#include <signal.h>
#include <string>
#include <fstream>
#include <string_view>

using namespace adb;

namespace {
    bool process_exists(pid_t pid) {
        // the kernel always makes sure a process exists before sending a signal to it, so we send a signal with 0
        auto ret = kill(pid, 0);
        return ret != -1 and errno != ESRCH; // when kill fails, it returns -1 and sets errno to ESRCH (Error No Such Process)
    }
}
namespace {
    char get_process_status(pid_t pid) {
        std::ifstream stat("/proc/" + std::to_string(pid) + "/stat"); // input file stream
        std::string data;
        std::getline(stat, data); // read a line from stream 'stat' into string 'data'
        auto idx_of_last_paren = data.rfind(')');
        auto idx_of_stat_indicator = idx_of_last_paren + 2; // displace after finding idx of parenthesis
        return data[idx_of_stat_indicator]; // return the char representing process state
    }
}

TEST_CASE("process::launch success", "[process]") {
    auto proc = process::launch("yes");
    REQUIRE(process_exists(proc->pid()));
}

TEST_CASE("process::launch no such program", "[process]") {
    REQUIRE_THROWS_AS(process::launch("random_ahh_process"), error);
}

TEST_CASE("process::attatch success", "[process]") {
    auto target = process::launch("./test/targets/run_endlessly", false);
    auto proc = process::attatch(target->pid());
    REQUIRE(get_process_status(target->pid()) == 't');
}

TEST_CASE("process::attatch invalid PID", "[process]") {
    REQUIRE_THROWS_AS(process::attatch(0), error);
}

TEST_CASE("process::resume success", "[process]") {
    // using different scopes to avoid name collisions within the same test case
    {
        auto proc = process::launch("./test/targets/run_endlessly");
        proc->resume();
        auto status = get_process_status(proc->pid());
        auto success = status == 'R' or status =='S'; // should be running (R) or sleeping (S) when the proc is waiting to be scheduled by the OS
        REQUIRE(success);
    }
    {
        auto target = process::launch("./test/targets/run_endlessly", false);
        auto proc = process::attatch(target->pid());
        proc->resume();
        auto status = get_process_status(proc->pid());
        auto success = status == 'R' or status == 'S';
        REQUIRE(success);
    }
}

TEST_CASE("process::resume already terminated", "[process]") {
    auto proc = process::launch("./test/targets/end_immediately");
    proc->resume();
    proc->wait_on_signal(); // waits for the process to terminate
    REQUIRE_THROWS_AS(proc->resume(), error);
}

TEST_CASE("Write register works", "[register]") {
    bool close_on_exec = false;
    adb::pipe channel(close_on_exec);

    auto proc = process::launch("./test/targets/reg_write", true, channel.get_write());
    channel.close_write();
    // Writing to GPR
    proc->resume();
    proc->wait_on_signal();

    auto& regs = proc->get_registers();
    regs.write_by_id(register_id::rsi, 0xcafecafe);

    proc->resume();
    proc->wait_on_signal();

    auto output = channel.read();
    REQUIRE(to_string_view(output) == "0xcafecafe");
    // Writing to MMX
    regs.write_by_id(register_id::mm0, 0xba5eba11);

    proc->resume();
    proc->wait_on_signal();

    output = channel.read();
    REQUIRE(to_string_view(output) == "0xba5eba11");
    // Writing to SSE
    regs.write_by_id(register_id::xmm0, 42.24);

    proc->resume();
    proc->wait_on_signal();

    output = channel.read();
    REQUIRE(to_string_view(output) == "42.24");\
    // Writing to x87
    regs.write_by_id(register_id::st0, 42.24l);
    regs.write_by_id(register_id::fsw,
        std::uint16_t{0b0011100000000000});
    regs.write_by_id(register_id::ftw,
        std::uint16_t{0b0011111111111111});
    proc->resume();
    proc->wait_on_signal();

    output = channel.read();
    REQUIRE(to_string_view(output) == "42.24");
}

TEST_CASE("Read register works", "[register]") {
    auto proc = process::launch("./test/targets/reg_read");
    auto& regs = proc->get_registers();

    proc->resume();
    proc->wait_on_signal();

    REQUIRE(regs.read_by_id_as<std::uint64_t>(register_id::r13) == 0xcafecafe);

    proc->resume();
    proc->wait_on_signal();

    REQUIRE(regs.read_by_id_as<std::uint8_t>(register_id::r13b) == 42);

    proc->resume();
    proc->wait_on_signal();

    REQUIRE(regs.read_by_id_as<byte64>(register_id::mm0) == to_byte64(0xba5eba11ull));

    proc->resume();
    proc->wait_on_signal();

    REQUIRE(regs.read_by_id_as<byte128>(register_id::xmm0) == to_byte128(64.125));

    proc->resume();
    proc->wait_on_signal();

    REQUIRE(regs.read_by_id_as<long double>(register_id::st0) == 64.125L);

}

TEST_CASE("Can create breakpoint site", "[breakpoint]") {
    auto proc = process::launch("./test/targets/run_endlessly");
    auto& site = proc->create_breakpoint_site(virt_addr{42});
    REQUIRE(site.address().addr() == 42);
}
