#ifndef ADB_PIPE_HPP
#define ADB_PIPE_HPP

#include <vector>
#include <cstddef>

namespace adb {
    /**
     * A wrapper class for C pipes.
     */
    class pipe {
        public:
            explicit pipe(bool close_on_exec); //explicit keyword prevents implicit conversions from occuring (either with the constructor or pass by values)
            ~pipe();

            int get_read() const  { return fds_[read_fd]; }
            int get_write() const { return fds_[write_fd]; }
            int release_read();
            int release_write();
            void close_read();
            void close_write();

            std::vector<std::byte> read();
            void write(std::byte* from, std::size_t bytes);

        private:
            static constexpr unsigned read_fd = 0;
            static constexpr unsigned write_fd = 1;
            int fds_[2];
    };
}

#endif
