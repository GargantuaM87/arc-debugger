#include "../include/libadb/process.hpp"
#include "../include/libadb/error.hpp"
#include <csignal>
#include <cstdlib>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

std::unique_ptr<adb::process> adb::process::launch(std::filesystem::path path) {
    pid_t pid;
    if((pid = fork()) < 0) {
        // Error: Fork failed
        error::send_errno("fork failed");
    }

    if(pid == 0) {
        if(ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) < 0) {
            error::send_errno("tracing failed");
        }
        if(execlp(path.c_str(), path.c_str(), nullptr) < 0) {
            error::send_errno("exec failed");
        }
    }
    std::unique_ptr<process> proc (new process(pid, true)); // private constructor used within the own class
    proc->wait_on_signal();

    return proc;
}

std::unique_ptr<adb::process> adb::process::attatch(pid_t pid) {
    if(pid == 0) {
        error::send("Invalid PID");
    }
    if(ptrace(PTRACE_ATTACH, pid, nullptr, nullptr) < 0) {
        error::send_errno("Could not attatch");
    }
    std::unique_ptr<process> proc (new process(pid, false)); // terminate_on_end is false since we're not launching a process
    proc->wait_on_signal();

    return proc;
}

adb::process::~process() {
    if(pid_ != 0) {
        int status;
        if(state_ == process_state::running) {
            kill(pid_, SIGSTOP); // Pause the process
            waitpid(pid_, &status, 0);
        }
        ptrace(PTRACE_DETACH, pid_, nullptr, nullptr); // Detatch from the process
        kill(pid_, SIGCONT); // Continue the process

        if(terminate_on_end_) {
            kill(pid_, SIGKILL);
            waitpid(pid_, &status, 0);
        }
    }
}

void adb::process::resume() {
    if (ptrace(PTRACE_CONT, pid_, nullptr, nullptr) < 0) {
        error::send_errno("Could not resume");
    }
    state_ = process_state::running;
}

adb::stop_reason adb::process::wait_on_signal() {
    int wait_status;
    int options = 0;
    if(waitpid(pid_, &wait_status, options) < 0) {
        error::send_errno("waitpid failed");
    }
    stop_reason reason(wait_status); // construct a new stop_reason object
    state_ = reason.reason; // set current process state to what made it stop
    return reason;
}

adb::stop_reason::stop_reason(int wait_status) {
    if(WIFEXITED(wait_status)) { // if the given status represents an exit event
        reason = process_state::exited;
        info = WEXITSTATUS(wait_status); //extracts exit code
    }
    else if(WIFSIGNALED(wait_status)) { // if a stop was due to termination
        reason = process_state::terminated;
        info = WTERMSIG(wait_status); // extract signal code
    }
    else if(WIFSTOPPED(wait_status)) { // if a stop was due to a pause
        reason = process_state::stopped;
        info = WSTOPSIG(wait_status); // extract signal code
    }
}
