#ifndef ADB_PROCESS_HPP
#define ADB_PROCESS_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory.h>
#include <optional>
#include <vector>
#include <sys/types.h>
#include <sys/user.h>
#include "./registers.hpp"
#include "./types.hpp"
#include "./breakpoint_site.hpp"
#include "./stopPoint_collection.hpp"
#include "./bit.hpp"

namespace adb {
    enum class process_state {
        stopped,
        running,
        exited,
        terminated
    };

    struct stop_reason {
        stop_reason(int wait_status);

        process_state reason;
        std::uint8_t info;
    };

    class process {
        public:
            ~process();
            /**
             * Launches a process through the given file path. By default, the process launched it to be debugged and it will
             * not have its stdout replaced.
             */
            static std::unique_ptr<process> launch(std::filesystem::path path, bool debug = true, std::optional<int> stdout_replacement = std::nullopt);
            /**
             * Attatch this debugger to a current active process through its PID.
             */
            static std::unique_ptr<process> attatch(pid_t pid); // Attatches to process based on PID
            /*
             * Change a process's state to running.
             */
            void resume();
            /*
             * Make a process stop until it recieves a signal.
             */
            stop_reason wait_on_signal();
            // Return process ID.
            pid_t pid() const { return pid_; }
            // Return process activity state.
            process_state state() const { return state_; }
            // Deleting constructor and copy operations
            process() = delete;
            process(const process&) = delete;
            process& operator=(const process&) = delete;

            // Return process registers.
            registers& get_registers() { return *registers_; }
            // Return process registers.
            const registers& get_registers() const { return *registers_; }
            // Write to debug registers or a general purpose register.
            void write_user_area(std::size_t offset, std::uint64_t data);
            // Write to all floating point registers.
            void write_fprs(const user_fpregs_struct& fprs);
            // Write to all general purpose registers.
            void write_gprs(const user_regs_struct& gprs);
            // Return the program counter from the %rip register.
            virt_addr get_pc() const {
                return virt_addr{
                    get_registers().read_by_id_as<std::uint64_t>(register_id::rip)
                };
            }
            // Set the program counter by writing to the %rip register.
            void set_pc(virt_addr address)
            {
                get_registers().write_by_id(register_id::rip, address.addr());
            }
            /*
             * Create a breakpoint site at the given address. Only one breakpoint site may exist at the same address. By default, the
             * breakpoint created will be a software breakpoint and will not be internally used by the debugger.
             */
            breakpoint_site& create_breakpoint_site(virt_addr address, bool hardware = false, bool internal = false);
            // Return collection of breakpoint sites.
            stopPoint_collection<breakpoint_site>& breakpoint_sites() { return breakpoint_sites_; }
            // Return collection of breakpoint sites. A const overload to maintain const correctness.
            const stopPoint_collection<breakpoint_site>& breakpoint_sites() const { return breakpoint_sites_; }
            /*
             * Step over a single instruction within the process. Before performing a single
             * step, a check is made to see if a breakpoint is enabled at the current program counter. If so, then the breakpoint is disabled and re-enabled again later.
             */
            adb::stop_reason step_instruction();\

            // Read memory from a virtual address and to a specified amount.
            std::vector<std::byte> read_memory(virt_addr address, std::size_t amount) const;
            /* Read memory while replacing int3 instructions that are necessary for
            *  breakpoints with the original data at each address within a range.
            */
            std::vector<std::byte> read_memory_without_traps(virt_addr address, std::size_t amount) const;
            /*
             * Loop until all the data that is given is written to the desired memory
             * address. Data is written 8 bytes at a time and in the case that there isn't at
             * least 8 bytes of data to write, then do a partial memory write.
             */
            void write_memory(virt_addr address, span<const std::byte> data);
            // Read from a given memory address as a different type.
            template <class T>
            T read_memory_as(virt_addr address) const {
                auto data = read_memory(address, sizeof(T)); // reads amount of memory equal to the size of the type
                return from_bytes<T>(data.data());
            }


        private:
            pid_t pid_ = 0;
            process_state state_ = process_state::stopped;
            bool terminate_on_end_ = true;
            bool is_attatched = true;
            std::unique_ptr<registers> registers_;
            stopPoint_collection<breakpoint_site> breakpoint_sites_;

            process(pid_t pid, bool terminate_on_end, bool is_attatched) : pid_(pid), terminate_on_end_(terminate_on_end),
                                                                           is_attatched(is_attatched), registers_(new registers(*this)) { }
            // Used to periodically update registers.
            void read_all_registers(); // populate registers when the process halts
    };
}


#endif
