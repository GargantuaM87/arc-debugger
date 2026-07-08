#ifndef ADB_BREAKPOINT_SITE_HPP
#define ADB_BREAKPOINT_SITE_HPP

#include <cstdint>
#include <cstddef>
#include "./types.hpp"

namespace adb {
    class process;

    class breakpoint_site {
        public:
            breakpoint_site() = delete;
            breakpoint_site(const breakpoint_site&) = delete;
            breakpoint_site& operator=(const breakpoint_site&) = delete;

            using id_type = std::int32_t;
            // Return the ID used for this breakpoint site.
            id_type id() const { return id_; }
            // Enable a breakpoint site while saving the data at that site.
            void enable();
            // Disable a breakpoint site while restoring the saved data at
            // that site.
            void disable();
            // Return if this current site is enabled.
            bool is_enabled() const { return is_enabled_; }
            // Return the address at this site.
            virt_addr address() const {return address_; }
            // Check if this breakpoint site is at the given virtual address.
            bool at_address(virt_addr addr) const {
                return address_ == addr;
            }
            /*
             * Check if this breakpoint site is within the specified range of
             * virtual addresses.
             */
            bool in_range(virt_addr low, virt_addr high) const {
                return low <= address_ and high > address_;
            }
        private:
            breakpoint_site(process&, virt_addr address);
            friend process;

            id_type id_;
            process* process_;
            virt_addr address_;
            bool is_enabled_;
            std::byte saved_data_;
    };
}

#endif
