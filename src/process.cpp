#include "../include/libadb/process.hpp"
#include "../include/libadb/error.hpp"
#include "../include/libadb/pipe.hpp"
#include "../include/libadb/bit.hpp"
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/personality.h>
#include <sys/uio.h>

namespace {
    void exit_with_perror(adb::pipe& channel, std::string const& prefix) {
        auto message = prefix + ": " + std::strerror(errno);
        channel.write(reinterpret_cast<std::byte*>(message.data()), message.size());
        exit(-1);
    }
}
// We can launch a process with options to debug it or not
std::unique_ptr<adb::process> adb::process::launch(std::filesystem::path path, bool debug, std::optional<int> stdout_replacement) {
    pipe channel(true); // close_on_exec = true
    pid_t pid;
    if((pid = fork()) < 0) {
        // Error: Fork failed
        error::send_errno("fork failed");
    }

    if(pid == 0) {
        personality(ADDR_NO_RANDOMIZE);
    }

    if(pid == 0) {
        // won't be needing this after we're inside the child process
        channel.close_read(); // parent will close its write end. On the parent side, we read the other end and throw an exception if the child wrote anything to it
        if(stdout_replacement) {
            if(dup2(*stdout_replacement, STDOUT_FILENO) < 0) {
                exit_with_perror(channel, "stdout replacement failed");
            }
        }
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
    auto pc = get_pc();
    if(breakpoint_sites_.enabled_stopPoint_at_address(pc)) { // check if we're at an enabled breakpoint before resuming
        auto& bp = breakpoint_sites_.get_by_address(pc);
        bp.disable();

        if(ptrace(PTRACE_SINGLESTEP, pid_, nullptr, nullptr) < 0) { // disable breakpoint then execute a single instruction
            error::send_errno("Failed to single step");
        }
        int wait_status;
        if(waitpid(pid_, &wait_status, 0) < 0) { // wait until inferior has executed instrction and halted
            error::send_errno("waitpid failed");
        }
        bp.enable(); // enable breakpoint again
    }
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

    if(is_attatched and state_ == process_state::stopped) {
        read_all_registers();
        // after stopping at a SIGTRAP, if the address 1 byte below the PC is an enabled breakpoint, then point at that breakpoint
        auto instr_begin = get_pc() - 1;
        if(reason.info == SIGTRAP and breakpoint_sites_.enabled_stopPoint_at_address(instr_begin))
            set_pc(instr_begin);
    }

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

void adb::process::read_all_registers() {
    // read all GPRs
    if(ptrace(PTRACE_GETREGS, pid_, nullptr, &get_registers().data_.regs) < 0) {
        error::send_errno("Could not read GPR registers");
    }
    // read all FPRs
    if(ptrace(PTRACE_GETFPREGS, pid_, nullptr, &get_registers().data_.i387) < 0) {
        error::send_errno("Could not read FPR registers");
    }
    // read debug registers
    for(int i = 0; i < 8; ++i) {
        auto id = static_cast<int>(register_id::dr0) + i;
        auto info = register_info_by_id(static_cast<register_id>(id));

        errno = 0; // set to 0 to signal for errors. If it is set to anything other than 0 after, then we have an error
        std::int64_t data = ptrace(PTRACE_PEEKUSER, pid_, info.offset, nullptr); // read data from correct offset
        if(errno != 0) error::send_errno("Could not read debug register");

        get_registers().data_.u_debugreg[i] = data; // write retrieved data into registers_ member
    }
}

void adb::process::write_user_area(std::size_t offset, std::uint64_t data) {
    // writing to debug registers or a single general purpose register at the given offset
    if(ptrace(PTRACE_POKEUSER, pid_, offset, data) < 0) {
        error::send_errno("Could not write to user area");
    }
}

void adb::process::write_fprs(const user_fpregs_struct& fprs) {
    if(ptrace(PTRACE_SETFPREGS, pid_, nullptr, &fprs) < 0) {
        error::send_errno("Could not write floating point registers");
    }
}

void adb::process::write_gprs(const user_regs_struct& gprs) {
    if(ptrace(PTRACE_SETREGS, pid_, nullptr, &gprs) < 0) {
        error::send_errno("Could not write general purpose registers");
    }
}

adb::breakpoint_site& adb::process::create_breakpoint_site(virt_addr address) {
    if(breakpoint_sites_.contains_address(address)) {
        error::send("Breakpoint site already created at address " + std::to_string(address.addr()));
    }
    return breakpoint_sites_.push(std::unique_ptr<breakpoint_site>(new breakpoint_site(*this, address)));
}

adb::stop_reason adb::process::step_instruction() {
    std::optional<breakpoint_site*> to_reEnable; //track the breakpoint at which the process is currently stopped (if there is one)
    auto pc = get_pc();
    if(breakpoint_sites_.enabled_stopPoint_at_address(pc))
    {
        auto& bp = breakpoint_sites_.get_by_address(pc); // declare as a ref so we can store a pointer to it in a variable declared in the enclosing scope
        bp.disable();
        to_reEnable = &bp;
    }

    if(ptrace(PTRACE_SINGLESTEP, pid_, nullptr, nullptr) < 0)
    {
        error::send_errno("Could not single step");
    }
    auto reason = wait_on_signal(); // after stepping over instruction, wait until the next instruction is completed then retrieve stop reason
    // enable the breakpoint again
    if(to_reEnable) {
        to_reEnable.value()->enable();
    }
    return reason;
}

std::vector<std::byte> adb::process::read_memory(virt_addr address, std::size_t amount) const {
    std::vector<std::byte> ret(amount); // initialize vector to fit the data we'll need

    iovec local_desc{ ret.data(), ret.size() }; // a type that wraps a pointer to a buffer and the size of a buffer (data and size of the ret vector)
    std::vector<iovec> remote_descs; // vector of descriptors to split range of data to be copied on memory page boundaries
    while(amount > 0)
    {
        auto next_page = 0x1000 - (address.addr() & 0xfff);
        auto chunk_size = std::min(amount, next_page);
        remote_descs.push_back({ reinterpret_cast<void*>(address.addr()), chunk_size});
        amount -= chunk_size;
        address += chunk_size;
    }

    if(process_vm_readv(pid_, &local_desc, /*__liovcnt=*/1, remote_descs.data(), /*__riovcnt=*/remote_descs.size(), 0) < 0) {
        error::send_errno("Could not read process memory space");
    }
    return ret;
}
// read 8 bytes from desired address, copy data over the start of those bytes, then write them back
void adb::process::write_memory(virt_addr address, span<const std::byte> data)
{
    std::size_t written = 0;
    while(written < data.size()) { // loop until we write all the data that the caller gives
        auto remaining = data.size() - written;
        std::uint64_t word;

        if(remaining >= 8) { // if at least 8 bytes remain, write the next 8 bytes from the start of the given buffer
            word = from_bytes<std::uint64_t>(data.begin() + written);
        }
        else {  // special case of doing a partial memory write
            auto read = read_memory(address + written, 8);
            auto word_data = reinterpret_cast<char*>(&word);
            std::memcpy(word_data, data.begin() + written, remaining);
            std::memcpy(word_data + remaining, read.data() + remaining, 8 - remaining);
        }
        if(ptrace(PTRACE_POKEDATA, pid_, address + written, word) < 0) {
            error::send_errno("Failed to write memory");
        }
        written += 8;
    }
}

std::vector<std::byte> adb::process::read_memory_without_traps(adb::virt_addr address, std::size_t amount) const {
    auto memory = read_memory(address, amount);
    auto sites = breakpoint_sites_.get_in_region(address, address + amount);
    // for each enabled breakpoint
    // replace int3 instruction with the memory addresses's original data
    for (auto site : sites) {
        if(!site->is_enabled()) continue;
        auto offset = site->address() - address.addr();
        memory[offset.addr()] = site->saved_data_;
    }
    return memory;
}
