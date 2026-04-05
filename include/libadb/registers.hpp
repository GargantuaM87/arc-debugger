#ifndef ADB_REGISTERS_HPP
#define ADB_REGISTERS_HPP

#include <cstdint>
#include <sys/user.h>
#include <variant>
#include "./register_info.hpp"
#include "./types.hpp"

namespace adb {
    //forward declaration of process
    // allows for its members to be hidden while we create a pointer to it
    class process;
    class registers {
    public:
        registers() = delete; // remove default construction
        registers(const registers&) = delete; // remove copy function
        registers& operator=(const registers&) = delete; // remove move function
        // using varient (a discriminated union) means the variable can be any of these types at a given time and holds information ab the data being held
        using value = std::variant<
            std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t,
            std::int8_t, std::int16_t, std::int32_t, std::int64_t,
            float, double, long double,
            byte64, byte128>;
        value read(const register_info& info) const;
        void write(const register_info& info, value val);

        // using an ID to retrieve the register value as a specific type
        template<class T>
        T read_by_id_as(register_id id) const {
            return std::get<T>(read(register_info_by_id(id))); // std::get will retrieve the type returned by std::variant
        }
        // writing to a register given its ID
        void write_by_id(register_id id, value val) {
            write(register_info_by_id(id), val);
        }

    private:
        friend process; //only adb::process should be able to construct a adb::register type
        registers(process& proc) : proc_(&proc) {}

        user data_; // store register values
        process* proc_;
    };
}

#endif
