#ifndef ADB_ERROR_HPP
#define ADB_ERROR_HPP

#include <stdexcept>
#include <cstring>

namespace adb {
    class error : public std::runtime_error {
        public:
            [[noreturn]] // Function is supposed to terminate through different means than returning a value and it doesn't return control flow when exiting
            // Throw an exception with a given message.
            static void send(const std::string& desc) { throw error(desc); }
            [[noreturn]]
            // Add a prefix to a message describing the meaning of the 'errno' code.
            static void send_errno(const std::string& prefix) {
                throw error(prefix + ": " + std::strerror(errno));
            }
        private:
            error(const std::string& desc) : std::runtime_error(desc) {}
    };
}
// Throw an exception with a given message.
#endif
