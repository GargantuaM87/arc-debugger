#ifndef ADB_WATCHPOINT_HPP
#define ADB_WATCHPOINT_HPP

#include <cstdint>
#include <cstddef>
#include "./types.hpp"

namespace adb {
    class process;

    class watchpoint {
        public:
            watchpoint() = delete; // Default constructor
            watchpoint(const watchpoint&) = delete; // Copy operation
            watchpoint& operator=(const watchpoint&) = delete; // Move operation

            using id_type = std::int32_t;
            // Return this watchpoint's ID.
            id_type id() const { return id_; }
            // Enable watchpoint.
            void enable();
            // Disable watchpoint.
            void disable();
            // Test if this watchpoint is enabled.
            bool is_enabled() const { return is_enabled_; }
            // Return watchpoint address.
            virt_addr address() const { return address_; }
            // Return watchpoint mode.
            stopPoint_mode mode() const { return mode_; }
            // Return watchpoint size.
            std::size_t size() const { return size_; }
            // Test if this watchpoint is at the given address.
            bool at_address(virt_addr address) const {
                return address_ == address;
            }
            // Test if this watchpoint is in the given range.
            bool in_range(virt_addr low, virt_addr high) const {
                return low <= address_ and high > address_;
            }
        private:
            friend process;
            watchpoint(process& proc, virt_addr address, stopPoint_mode mode, std::size_t size);

            id_type id_;
            process* process_;
            virt_addr address_;
            stopPoint_mode mode_;
            std::size_t size_;
            bool is_enabled_;
            int hardware_register_index_ = -1;
    };
}

#endif
