#include <cstdio>
#include <numeric>
#include <unistd.h>
#include <signal.h>

void innocent_function() {
    std::puts("Replacing devs with AI...");
}

void innocent_function_end() {}

// sums up all the bytes of innocent_function into a single int
int checksum() {
    auto start = reinterpret_cast<volatile const char*>(&innocent_function); // these values can always change unexpectedly outside the range of the code
    auto end = reinterpret_cast<volatile const char*>(&innocent_function_end);
    return std::accumulate(start, end, 0);
}
int main() {
    // safe checksum value. When no breakpoints are set
    auto safe = checksum();
    // write the address of innocent_function()
    auto ptr = reinterpret_cast<void*>(&innocent_function);
    write(STDOUT_FILENO, &ptr, sizeof(void*));
    fflush(stdout);

    raise(SIGTRAP);

    while(true) {
        if(checksum() == safe) { // if nothing has changed, continue
            innocent_function();
        }
        else { // means that something has changed. Notably, a breakpoint has been set.
            puts("Replacing AI with real developers...");
        }
        fflush(stdout);
        raise(SIGTRAP); // raise a signal after each message
    }
}
