#include <cstring>
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
#include "../include/libadb/process.hpp"
#include "../include/libadb/error.hpp"

// Anonymous namespace so we can reuse the same names in different files
namespace {
    /*
     * Attatch to a process.
     */
    std::unique_ptr<adb::process> attatch(int argc, const char** argv) {
        // Passing PID
        if(argc == 3 && argv[1] == std::string_view("-p")) { // comparing string contents instead of memory contents
            pid_t pid = std::atoi(argv[2]); // conversion from string to integer
            return adb::process::attatch(pid);
        }
       // Passing program name
        else {
           const char* program_path = argv[1];
           return adb::process::launch(program_path);
       }
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
    void print_stop_reason(const adb::process& process, adb::stop_reason reason) {
        std::cout << "Process " << process.pid() << ' ';

       switch(reason.reason) {
       case adb::process_state::exited:
            std::cout << "exited with status " << static_cast<int>(reason.info);
            break;
       case adb::process_state::terminated:
            std::cout << "terminated with signal " << sigabbrev_np(reason.info); // Gives SIGNAL abbreviation for Signal code
            break;
       case adb::process_state::stopped:
            std::cout << "stopped with signal " << sigabbrev_np(reason.info);
            break;
       }
       std::cout << std::endl;
    }
    void handle_command(std::unique_ptr<adb::process>& process, std::string_view line) {
        auto args = split(line, ' ');
        auto command = args[0];

        if(is_prefix(command, "continue")) {
            process->resume();
            auto reason = process->wait_on_signal();
            print_stop_reason(*process, reason);
        }
        else {
            std::cerr << "Unknown command" << std::endl;
        }
    }
}
namespace {
    void main_loop(std::unique_ptr<adb::process>& process) {
        char* line = nullptr;
        while ((line = readline("adb> ")) != nullptr) {
            std::string line_str;

            if(line == std::string_view("")) {
                free(line);
                if(history_length > 0) {
                    line_str = history_list()[history_length - 1]->line;
                }
            }
            else {
                line_str = line;
                add_history(line);
                free(line);
            }
            if(!line_str.empty()) {
                try {
                    handle_command(process, line_str);
                }
                catch(const adb::error& err) {
                    std::cout << err.what() << std::endl;
                }
            }
        }
    }
}
int main(int argc, const char** argv) {
    if(argc == 1) {
        std::cerr << "No arguments given" << std::endl;
        return -1;
    }
    try {
        auto process = attatch(argc, argv);
        main_loop(process);
    }
    catch(const adb::error& err) {
        std::cout << err.what() << std::endl;
    }
}
