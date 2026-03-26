#include <catch2/catch_test_macros.hpp>
#include "../include/libadb/process.hpp"
#include "../include/libadb/error.hpp"
#include <cerrno>
#include <signal.h>

using namespace adb;

namespace {
    bool process_exists(pid_t pid) {
        // the kernel always makes sure a process exists before sending a signal to it, so we send a signal with 0
        auto ret = kill(pid, 0);
        return ret != -1 and errno != ESRCH; // when kill fails, it returns -1 and sets errno to ESRCH (Error No Such Process)
    }
}

TEST_CASE("process::launch success", "[process]") {
    auto proc = process::launch("yes");
    REQUIRE(process_exists(proc->pid()));
}

TEST_CASE("process::launch no such program", "[process]") {
    REQUIRE_THROWS_AS(process::launch("random_ahh_process"), error);
}
