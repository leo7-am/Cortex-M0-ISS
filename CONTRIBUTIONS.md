# Contributions

## CP1

### Member A: Marwan Aboelnour
- Wrote the decoder (`decoder.hpp`, `decoder.cpp`). It reads all 43 instructions.
- Wrote `--disasm`, which prints the program's instructions like objdump does.
- Added to `iss.cpp`: finding the code section and reading the command line.
- Wrote the Thumb program `programs/divide.s` (HW3 problem 3: division) and its dump.

### Member C: Farah Habib
- Wrote the Thumb program `programs/fib.s` (HW3 problem 5(a): recursive Fibonacci) and its
  dump. It keeps the RARS version's prompts and output, and `fib` is recursive with a
  12-byte stack frame (`push {r4, r5, lr}` / `pop {r4, r5, pc}`).

## CP2

### Member A: Marwan Aboelnour
- Wrote the load and store instructions in `execute.cpp`: `LDR`, `STR`, `LDRB`, `STRB`,
  `LDRH`, `STRH`, `LDRSB`, `LDRSH`, in all their forms (`[rn, #imm]`, `[rn, rm]`,
  `[sp, #imm]` and `[pc, #imm]`).
- Wrote `LDM`, `STM`, `PUSH` and `POP`. `POP {pc}` returns from a function and checks bit 0.
- Wrote the test program `tests/memory_ops`, which checks all of these against refiss.

### Member C: Farah Habib
- Split the simulator into files: `machine.hpp/.cpp` (my part), `execute.cpp` (where
  Members A and B add instructions by switching on `decode().op`), and `iss.cpp` (`main()`).
- Wrote `machine.hpp`: a documented interface for A and B (registers, flags, `set_reg`,
  `mem_read`, `mem_write`, `fetch16`, `taken_branch`, `bl_extra`, `svc`, `fault`).
- Wrote `machine.cpp`:
  - memory map (1 MB ROM, 256 KB RAM) with alignment checked before BUS
  - faults as a C++ exception, so a faulting instruction doesn't retire
  - reset (sp, pc and INVSTATE from the vector table, lr = 0xFFFFFFFF)
  - fetch loop with the `--max-instr` LIMIT
  - all SVC services (EXIT, PUTCHAR, PUTS, PUTINT, PUTHEX, GETCHAR, GETINT, READ, WRITE,
    INSTRET, CYCLES, RAND, DUMP_REGS, DUMP_MEM, TRACE_ON, TRACE_OFF)
  - the `--trace` file and the cycle counter (including `--cpu m0plus`)
- Made Marwan's dump loader stricter: a bad line exits 253, and `.text` can span several
  ranges.
- Command line: `--cpu` must be `m0` or `m0plus`, and numbers are checked.
- Makefile: builds every `.cpp` file, adds `test-cp2` and `test-ours`. Added `.gitignore`
  and stopped tracking `iss.exe` and `iss.pdb`.
- Test programs in `tests/`: `getint_rules`, `io_edge_cases`, `fault_bus_read_into_rom`,
  `fault_unaligned_rom_store`.
