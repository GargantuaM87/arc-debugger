#include <iostream>
#include <sched.h>
#include <string_view>
#include <string>
#include <editline/readline.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <vector>
#include <sstream>

// Anonymous namespace so we can reuse the same names in different files
namespace {
    /*
     * Attatch to a process.
     */
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
                    return -1;
                }
                // -l means arguments should be supplied individually instead of as an array
                // -p means that the facility will search the PATH environment variable for the program name if the supplied path doesn't contain a /
                if(execlp(program_path, program_path, nullptr) < 0) {
                    std::perror("Exec failed");
                    return -1;
                }
            }
        }
        return pid;
    }
}
namespace {
    std::vector<std::string> split(std::string_view str, char delimiter) {
        std::vector<std::string> out{};
        std::stringstream ss {std::string{str}}; // like a string builder in Java
        std::string item;

        while(std::getline(ss, item, delimiter)) {
            out.push_back(item);
        }
        return out;
    }
    bool is_prefix(std::string_view str, std::string_view of) {
        if(str.size() > of.size()) return false; // as a prefix of a word is supposed to be smaller
        return std::equal(str.begin(), str.end(), of.begin()); // if the string has any occurence of the prefix word
    }
    // Causes the OS to resume the execution of a process
    void resume(pid_t pid) {
        if(ptrace(PTRACE_CONT, pid, nullptr, nullptr) < 0) {
            std::cerr << "Couldn't continue" << std::endl;
            std::exit(-1);
        }
    }
    void wait_on_signal(pid_t pid) {
        int wait_status = 0;
        int options = 0;
        if(waitpid(pid, &wait_status, options) < 0) {
            std::perror("waitpid failed");
            std::exit(-1);
        }
    }

    void handle_command(pid_t pid, std::string_view line) {
        auto args = split(line, ' ');
        auto command = args[0];

        if(is_prefix(command, "continue")) {
            resume(pid);
            wait_on_signal(pid);
        }
        else {
            std::cerr << "Unknown command" << std::endl;
        }
    }
}
int main(int argc, const char** argv) {
    if(argc == 1) {
        std::cerr << "No arguments given" << std::endl;
        return -1;
    }
    pid_t pid = attatch(argc, argv);
    int wait_status;
    int options = 0;
    if(waitpid(pid, &wait_status, options) < 0) {
        std::perror("waitpid failed");
        return -1;
    }

    char* line = nullptr;
    while((line = readline("adb> ")) != nullptr) {
        std::string line_str; // hold the command to be executed. Can come from the user or the recent history

        if(line == std::string_view("")) {
            free(line);
            if(history_length > 0) {
                line_str = history_list()[history_length - 1]->line; // most recent line input from the user
            }
        }

        else {
            line_str = line;
            add_history(line);
            free(line);
        }

        if(!line_str.empty()) {
            handle_command(pid, line_str);
        }
    }
}
