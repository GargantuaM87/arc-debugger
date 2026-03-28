#include "../include/libadb/process.hpp"
#include "../include/libadb/error.hpp"
#include "../include/libadb/pipe.hpp"
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
    void exit_with_perror(adb::pipe& channel, std::string const& prefix) {
        auto message = prefix + ": " + std::strerror(errno);
        channel.write(reinterpret_cast<std::byte*>(message.data()), message.size());
        exit(-1);
    }
}
// We can launch a process with options to debug it or not
std::unique_ptr<adb::process> adb::process::launch(std::filesystem::path path, bool debug) {
    pipe channel(true); // close_on_exec = ture
    pid_t pid;
    if((pid = fork()) < 0) {
        // Error: Fork failed
        error::send_errno("fork failed");
    }

    if(pid == 0) {
        // won't be needing this after we're inside the child process
        channel.close_read(); // parent will close its write end. On the parent side, we read the other end and throw an exception if the child wrote anything to it
        if(debug and ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) < 0) {
            exit_with_perror(channel, "tracing failed");
        }
        if(execlp(path.c_str(), path.c_str(), nullptr) < 0) {
            exit_with_perror(channel, "exec failed");
        }
    }

    channel.close_write();
    auto data = channel.read();
    channel.close_read();

    if(data.size() > 0) {
        waitpid(pid, nullptr, 0); // wait for child process to terminate
        auto chars = reinterpret_cast<char*>(data.data());
        error::send(std::string(chars, chars + data.size())); // issue error with given message read from child through pipe
    }

    std::unique_ptr<process> proc (new process(pid, true, debug)); // private constructor used within the own class

    if(debug)
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
    std::unique_ptr<process> proc (new process(pid, false, true)); // terminate_on_end is false since we're not launching a process
    proc->wait_on_signal();

    return proc;
}

adb::process::~process() {
    if(pid_ != 0) {
        int status;
        if(is_attatched) {
            if(state_ == process_state::running) {
                kill(pid_, SIGSTOP); // Pause the process
                waitpid(pid_, &status, 0);
            }
            ptrace(PTRACE_DETACH, pid_, nullptr, nullptr); // Detatch from the process
            kill(pid_, SIGCONT); // Continue the process
        }
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
