#ifndef ADB_PROCESS_HPP
#define ADB_PROCESS_HPP

#include <filesystem>
#include <memory.h>
#include <sys/types.h>

namespace adb {
    enum class process_state {
        stopped,
        running,
        exited,
        terminated
    };

    class process {
        public:
            ~process();
            static std::unique_ptr<process> launch(std::filesystem::path path); // Launches process based on path
            static std::unique_ptr<process> attatch(pid_t pid); // Attatches to process based on PID

            void resume();
            void wait_on_signal();
            // Getters
            pid_t pid() const { return pid_; }
            process_state state() const { return state_; }
            // Deleting constructor and copy operations
            process() = delete;
            process(const process&) = delete;
            process& operator=(const process&) = delete;

        private:
            pid_t pid_ = 0;
            process_state state_ = process_state::stopped;
            bool terminate_on_end_ = true;

            process(pid_t pid, bool terminate_on_end) : pid_(pid), terminate_on_end_(terminate_on_end) { }
    };
}


#endif
