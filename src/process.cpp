#include "../include/libadb/process.hpp"
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

std::unique_ptr<adb::process> adb::process::launch(std::filesystem::path path) {
    pid_t pid;
    if((pid = fork()) < 0) {
        // Error: Fork failed
    }

    if(pid == 0) {
        if(ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) < 0) {
            // Error: Tracing failed
        }
        if(execlp(path.c_str(), path.c_str(), nullptr) < 0) {
            // Error: exec failed
        }
    }
    std::unique_ptr<process> proc (new process(pid, true)); // private constructor used within the own class
    proc->wait_on_signal();

    return proc;
}

std::unique_ptr<adb::process> adb::process::attatch(pid_t pid) {
    if(pid == 0) {
        // Error: Invalid PID
    }
    if(ptrace(PTRACE_ATTACH, pid, nullptr, nullptr) < 0) {
        // Error: Could not attatch
    }
    std::unique_ptr<process> proc (new process(pid, false)); // terminate_on_end is false since we're launching a process
    proc->wait_on_signal();

    return proc;
}
