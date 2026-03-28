#include <catch2/catch_test_macros.hpp>
#include "../include/libadb/process.hpp"
#include "../include/libadb/error.hpp"
#include <cerrno>
#include <signal.h>
#include <string>
#include <fstream>

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
