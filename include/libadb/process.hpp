#ifndef ADB_PROCESS_HPP
#define ADB_PROCESS_HPP

#include <cstdint>
#include <filesystem>
#include <memory.h>
#include <sys/types.h>
#include <sys/user.h>
#include "./registers.hpp"

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
            static std::unique_ptr<process> launch(std::filesystem::path path, bool debug = true); // Launches process based on path
            static std::unique_ptr<process> attatch(pid_t pid); // Attatches to process based on PID

            void resume();
            stop_reason wait_on_signal();
            // Getters
            pid_t pid() const { return pid_; }
            process_state state() const { return state_; }
            // Deleting constructor and copy operations
            process() = delete;
            process(const process&) = delete;
            process& operator=(const process&) = delete;

            // Register functions
            registers& get_registers() { return *registers_; }
            const registers& get_registers() const { return *registers_; }

            void write_user_area(std::size_t offset, std::uint64_t data);

            void write_fprs(const user_fpregs_struct& fprs);
            void write_gprs(const user_regs_struct& gprs);

        private:
            pid_t pid_ = 0;
            process_state state_ = process_state::stopped;
            bool terminate_on_end_ = true;
            bool is_attatched = true;
            std::unique_ptr<registers> registers_;

            process(pid_t pid, bool terminate_on_end, bool is_attatched) : pid_(pid), terminate_on_end_(terminate_on_end),
                                                                           is_attatched(is_attatched), registers_(new registers(*this)) { }

            void read_all_registers(); // populate registers when the process halts
    };
}


#endif
