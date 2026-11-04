# arcDebugger

arcDebugger is a lightweight Linux process debugging tool built in C++ for debugging native executables via `ptrace`. It supports launching or attaching to a process, inspecting registers, reading and writing memory, enabling breakpoints/watchpoints, stepping instructions, and disassembling code.

This project is still under active development, but the core CLI is usable for basic debugging workflows.

## Features

- Launch a target process or attach to an existing PID
- Read and write general-purpose and floating-point registers
- Set, enable, disable, and delete software breakpoints
- Set, enable, disable, and delete watchpoints
- Catch syscall entry/exit events
- Disassemble instructions around the current program counter or an arbitrary address
- Read and write memory
- Parse ELF metadata and symbol information
- Resume execution, stop on signals, and step single instructions

## Requirements

- Linux (the implementation uses `ptrace`, `waitpid`, and Linux-specific process APIs)
- CMake 3.19 or newer
- A C++17 compiler
- vcpkg or equivalent package installation for the following dependencies:
  - `libedit`
  - `fmt`
  - `zydis`
  - `catch2` (only required when building tests)

## Build instructions

1. Install vcpkg and make sure it is available on your system.
2. Configure the project with the vcpkg toolchain:

```bash
cd arcDebugger
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
```

3. Build the project:

```bash
cmake --build build
```

4. Optionally run the tests:

```bash
ctest --test-dir build
```

If you already have the required libraries installed system-wide, you may omit the vcpkg toolchain and configure with a normal CMake toolchain.

## Using the debugger

The CLI binary is built under the `build/tools` directory as `adb`.

### Launch a program

```bash
./build/tools/adb /path/to/program
```

This starts the target process under the debugger.

### Attach to an existing process

```bash
./build/tools/adb -p 1234
```

### Common commands inside the debugger

```text
help
continue
step
register read
register read rax
register write rax 0x1234
memory read 0x401000
memory read 0x401000 64
memory write 0x401000 41 42 43 44
breakpoint set 0x401000
breakpoint list
breakpoint disable 1
breakpoint enable 1
watchpoint set 0x402000 rw 8
catchpoint syscall
catchpoint syscall read write
catchpoint syscall none
disassemble -a 0x401000 -c 12
```

The command loop is interactive, and commands are entered at the prompt after the target process is launched or attached.

## Example workflow

```bash
./build/tools/adb /bin/ls
help
breakpoint set 0x401000
continue
register read all
memory read 0x401000 32
disassemble -a 0x401000 -c 10
step
```

## Future work

- Expanded DWARF-aware symbol resolution
- More robust command parsing and UX improvements
