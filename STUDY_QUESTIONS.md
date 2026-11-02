# arcDebugger Study Questions

Use these as prompts, not a quiz to rush through. For each one, first make a prediction, then trace the implementation or run a small experiment, and finally explain what evidence changed or confirmed your thinking.

## 1. Process and operating-system foundations

1. Follow `process::launch` from `fork()` through `exec`. Which operations happen in the child, which happen in the parent, and how does the parent learn that child setup failed?
2. Why does the child call `PTRACE_TRACEME` before `exec`? What does the initial stop let the debugger do?
3. Compare `launch` with `attatch` (the project’s spelling). What state and ownership assumptions differ, especially when a `process` is destroyed?
4. What information does `waitpid` return, and how does `stop_reason` distinguish a normal exit, signal termination, and a stopped process?
5. In `resume` and `wait_on_signal`, when does the debugger consider the process running or stopped? What could become inconsistent if an error occurs between a `ptrace` call and updating `state_`?

## 2. Traps, breakpoints, and stepping

6. A software breakpoint replaces a byte with `0xCC`. What x86 instruction is that, why is it useful to a debugger, and what does the CPU do when it executes it?
7. Why does `wait_on_signal` move the program counter back by one after a software breakpoint? What would happen if it did not?
8. `PTRACE_PEEKDATA` reads a machine word, even though a software breakpoint changes one byte. How does `breakpoint_site::enable` preserve the surrounding bytes, and what assumptions are involved?
9. When execution is stopped at an enabled breakpoint, why must `resume` temporarily disable it, single-step one instruction, then enable it again? Trace this path and identify what should happen if single-stepping fails.
10. Compare software and hardware breakpoints. What are their tradeoffs, and where does this implementation enforce the x86 limit on hardware breakpoint registers?
11. `SIGTRAP` can mean more than “breakpoint hit.” How does this project distinguish single-step, software-breakpoint, hardware-breakpoint, and syscall stops? What role does `PTRACE_O_TRACESYSGOOD` play?

## 3. C++ design and correctness

12. Why are `process`, `elf`, and `target` non-copyable? Which resources or relationships would a default copy accidentally duplicate?
13. In `target`, why are `process` and `elf` owned by `std::unique_ptr`, while `get_process()` and `get_elf()` return references? What lifetime guarantee must callers respect?
14. Find uses of `std::optional` and `std::variant` in the debugger. What invalid or alternative states do they represent, and how does the caller have to handle each possibility?
15. `syscall_information` stores either syscall arguments or a return code in a union. How can a caller know which member is valid, and what C++ rules make this representation worth inspecting carefully?
16. Trace a register read or write from the public `registers` API down to the operating-system interface. How does the code map a logical register ID to its storage or kernel offset?
17. Pick one use of `std::byte`, `span`, or `from_bytes`. What does the type make safer or clearer, and what bounds, alignment, or lifetime risks remain?

## 4. Executables, addresses, and symbols

18. What is the difference between a file address and a virtual address? Trace how `elf::load_bias()` or `notify_loaded()` helps relate addresses in the executable to addresses in a running process.
19. What is the difference between an ELF section and a loadable segment? Which one is useful for parsing symbols, and which one determines what the operating system maps into memory?
20. Why might disabling address-space layout randomization make debugger tests easier? What changes when the target is run with ASLR enabled?
21. How does the ELF symbol table help associate an address with a function name? What information is missing when a binary is stripped?
22. The DWARF API models abbreviation tables, compile units, DIEs, and attributes. How do these pieces describe source-level structure, and what extra information would be needed to show a local variable’s current value?

## 5. Try it in the repository

23. Read the breakpoint tests in `test/tests.cpp`. Before running them, predict how the program counter and original instruction byte should look before a hit, at the hit, and after resuming.
24. Choose a test target and trace one debugger operation end-to-end: API call, `ptrace` request, kernel stop or memory change, and test assertion. Draw the sequence and label which process is executing at each step.
25. Build a tiny target with a loop and one variable, then compare stepping with and without compiler optimization. Which source-level expectations stop matching machine instructions, and why?
26. Use tools such as `readelf`, `objdump`, or `/proc/<pid>/maps` to check an address the debugger reports. Can you explain any difference among the file offset, ELF virtual address, and runtime address?
27. Choose an edge case in launch, attach, memory access, or breakpoint handling. Write down the expected behavior, add a focused test, and explain which failure path or invariant the test covers.

## Suggested starting points

- Process lifecycle and stop handling: `src/process.cpp` and `include/libadb/process.hpp`
- Software breakpoints: `src/breakpoint_site.cpp` and `include/libadb/breakpoint_site.hpp`
- ELF address and symbol model: `include/libadb/elf.hpp`
- Existing experiments and assertions: `test/tests.cpp` and `test/targets/`