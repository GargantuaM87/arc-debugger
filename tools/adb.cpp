#include <cstdint>
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
#include <fmt/format.h>
#include <fmt/ranges.h>
#include "../include/libadb/process.hpp"
#include "../include/libadb/error.hpp"
#include "../include/libadb/parse.hpp"

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
        std::string message;

       switch(reason.reason) {
       case adb::process_state::exited:
            message = fmt::format("exited with status {}", static_cast<int>(reason.info));
            break;
       case adb::process_state::terminated:
            message = fmt::format("terminated with signal {}", sigabbrev_np(reason.info));
            break;
       case adb::process_state::stopped:
            message = fmt::format("stopped with signal {} at {:#}", sigabbrev_np(reason.info), process.get_pc().addr());
            break;
       }
       fmt::print("Process {} {}\n", process.pid(), message);
    }

    adb::registers::value parse_register_value(adb::register_info info, std::string_view text) {
        try {
            if(info.format == adb::register_format::uint) {
                switch(info.size) {
                    case 1: return adb::to_integral<std::uint8_t>(text, 16).value();
                    case 2: return adb::to_integral<std::uint16_t>(text, 16).value();
                    case 4: return adb::to_integral<std::uint32_t>(text, 16).value();
                    case 8: return adb::to_integral<std::uint64_t>(text, 16).value();
                }
            }
            else if(info.format == adb::register_format::double_float) {
                return adb::to_float<double>(text).value();
            }
            else if(info.format == adb::register_format::long_double) {
                return adb::to_float<long double>(text).value();
            }
            else if(info.format == adb::register_format::vector) {
                if(info.size == 8) {
                    return adb::parse_vector<8>(text);
                }
                else if(info.size == 16) {
                    return adb::parse_vector<16>(text);
                }
            }
        }
        catch(...) {}
        adb::error::send("Invalid format");
    }

    void print_help(const std::vector<std::string>& args) {
        if(args.size() == 1) {
            std::cerr << R"(Available commands:
    continue    - Resume the process
    register    - Commands for operating on registers
            )";
        }
        else if(is_prefix(args[1], "register")) {
            std::cerr << R"(Available commands:
    read
    read <register>
    read all
    write <register> <value>
            )";
        }
        else {
            std::cerr << "No help available on that\n";
        }
    }

    void handle_register_read(adb::process& process, const std::vector<std::string>& args) {
        auto format = [](auto t) {
            if constexpr (std::is_floating_point_v<decltype(t)>) {
                return fmt::format("{}", t);
            }
            else if constexpr (std::is_integral_v<decltype(t)>) {
                return fmt::format("{:#0{}x}", t, sizeof(t) * 2 + 2); // registers will have the correct size wehn we print them
            }
            else {
                return fmt::format("[{:#04x}]", fmt::join(t, ","));
            }
        };
        // reading all registers or just GPRs
        if(args.size() == 2 or (args.size() == 3 and args[2] == "all")) {
            for(auto& info : adb::g_register_infos) {
                bool should_print = (args.size() == 3 or info.type == adb::register_type::gpr) and info.name != "orig_rax"; //orig_rax is not considered a real register
                if(!should_print) continue;
                auto value = process.get_registers().read(info);
                fmt::print("{}:\t{}\n", info.name, std::visit(format, value)); // format register by name and its value
            }
        }
        // reading a specific register
        else if(args.size() == 3) {
            try {
                auto info = adb::register_info_by_name(args[2]);
                auto value = process.get_registers().read(info);
                fmt::print("{}:\t{}\n", info.name, std::visit(format, value));
            }
            catch(adb::error& err) {
                std::cerr << "No such register\n";
                return;
            }
        }
        // help message
        else {
            print_help({"help", "register"});
        }
    }

    void handle_register_write(adb::process& process, const std::vector<std::string>& args) {
        if(args.size() != 4) {
            print_help({"help", "register"});
            return;
        }
        try {
            auto info = adb::register_info_by_name(args[2]);
            auto value = parse_register_value(info, args[3]);
            process.get_registers().write(info, value);
        }
        catch(adb::error& err) {
            std::cerr << err.what() << '\n';
            return;
        }
    }

    void handle_register_command(adb::process& process, std::vector<std::string>& args) {
        if(args.size() < 2) {
            print_help({"help", "register"});
            return;
        }

        if(is_prefix(args[1], "read")) {
            handle_register_read(process, args);
        }
        else if(is_prefix(args[1], "write")) {
            handle_register_write(process, args);
        }
        else {
            print_help({"help", "register"});
        }
    }

    void handle_command(std::unique_ptr<adb::process>& process, std::string_view line) {
        auto args = split(line, ' ');
        auto command = args[0];

        if(is_prefix(command, "continue")) {
            process->resume();
            auto reason = process->wait_on_signal();
            print_stop_reason(*process, reason);
        }
        else if(is_prefix(command, "help")) {
            print_help(args);
        }
        else if(is_prefix(command, "register")) {
            handle_register_command(*process, args);
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
