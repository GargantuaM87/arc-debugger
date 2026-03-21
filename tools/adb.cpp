#include <iostream>
#include <sched.h>
#include <string_view>
#include <unistd.h>
#include <sys/ptrace.h>

// Anonymous namespace so we can reuse the same names in different files
namespace {
    pid_t attatch(int argc, const char** argv) {
        pid_t pid = 0;
        // Passing PID
        if(argc == 3 && argv[1] == std::string_view("-p")) { // comparing string contents instead of memory contents
            pid = std::atoi(argv[2]); // conversion from string to integer
            if(pid <= 0) {
                std::cerr << "Invalid pid" << std::endl;
                return -1;
            }
            if(ptrace(PTRACE_ATTACH, pid, nullptr, nullptr) < 0) {
                std::perror("Could not attatch!");
                return -1;
            }
        }
        // Passing program name
        else {
            const char* program_path = argv[1];
            if((pid = fork()) < 0) {
                std::perror("Fork failed");
                return -1;
            }
            if(pid == 0) {
                // In child process, execute debugee
                if(ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) < 0) {
                    std::perror("Tracing failed");
                    return 01;
                }
                if(execlp(program_path, program_path, nullptr) < 0) {
                    std::perror("Exec failed");
                    return -1;
                }
            }
        }
        return pid;
    }
}

int main(int argc, const char** argv) {
    if(argc == 1) {
        std::cerr << "No arguments given" << std::endl;
        return -1;
    }
    pid_t pid = attatch(argc, argv);
}
