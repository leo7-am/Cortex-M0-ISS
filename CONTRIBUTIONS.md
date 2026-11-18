# Contributions

## CP1

### Member A: Marwan Aboelnour
- Wrote the decoder (`decoder.hpp`, `decoder.cpp`). It reads all 43 instructions.
- Wrote `--disasm`, which prints the program's instructions like objdump does.
- Added to `iss.cpp`: finding the code section and reading the command line.
- Wrote the Thumb program `programs/divide/divide.s` (HW3 problem 3: division) and its dump.

### Member B: Adham Abotarboush
- Wrote the Thumb program `programs/range/range.s` (HW3 problem 1: count and sum of a range
  with a step) and its dump.

### Member C: Farah Habib
- Wrote the Thumb program `programs/fib/fib.s` (HW3 problem 5(a): recursive Fibonacci) and its
  dump. It keeps the RARS version's prompts and output, and `fib` is recursive with a
  12-byte stack frame (`push {r4, r5, lr}` / `pop {r4, r5, pc}`).

## CP2

### Member A: Marwan Aboelnour
- Wrote the load and store instructions in `execute.cpp`: `LDR`, `STR`, `LDRB`, `STRB`,
  `LDRH`, `STRH`, `LDRSB`, `LDRSH`, in all their forms (`[rn, #imm]`, `[rn, rm]`,
  `[sp, #imm]` and `[pc, #imm]`).
- Wrote `LDM`, `STM`, `PUSH` and `POP`. `POP {pc}` returns from a function and checks bit 0.
- Wrote the test program `tests/memory_ops`, which checks all of these against refiss.

### Member B: Adham Abotarboush
- Wrote the arithmetic, logic, shift and branch instructions in `src/alu.cpp` and
  `src/alu.hpp`; `execute.cpp` passes every instruction it doesn't handle to `execute_alu()`:
  - moves: `MOVS`, `MOV` (any registers), `MVNS`, `NOP`
  - arithmetic: `ADDS`, `SUBS`, `ADCS`, `SBCS`, `NEGS`, `MULS`, `ADD` (high registers),
    `ADD`/`SUB` with sp, `ADR`
  - logic and compare: `ANDS`, `ORRS`, `EORS`, `BICS`, `TST`, `CMP`, `CMN`
  - shifts: `LSLS`, `LSRS`, `ASRS` by immediate and by register, `RORS`, with the exact
    carry rules for amounts of 0 and 32 and above
  - extend and reverse: `SXTB`, `SXTH`, `UXTB`, `UXTH`, `REV`
  - branches: all `B<cond>`, `BL`, `BX`, `BLX`, and `MOV`/`ADD` with pc as a plain branch
- `add_with_carry()` sets N, Z, C and V for every add, subtract and compare, so C is
  "no borrow" on subtraction.
- `BX`/`BLX` check bit 0 before changing lr, so an INVSTATE fault leaves no change.
- Organized the repository: sources moved to `src/`, kept only `public-tests/` from the
  course kit, put each program in its own folder under `programs/`, removed `out.txt`
  and `.DS_Store`, and updated the Makefile paths.

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
  - SVC services: EXIT and the console services (PUTCHAR, PUTS, PUTINT, PUTHEX, GETCHAR,
    GETINT), plus CYCLES, RAND, DUMP_REGS, DUMP_MEM, TRACE_ON and TRACE_OFF
  - the `--trace` file and the cycle counter (including `--cpu m0plus`)
- Made Marwan's dump loader stricter: a bad line exits 253, and `.text` can span several
  ranges.
- Command line: `--cpu` must be `m0` or `m0plus`, and numbers are checked.
- Makefile: builds every `.cpp` file, adds `test-cp2` and `test-ours`. Added `.gitignore`
  and stopped tracking `iss.exe` and `iss.pdb`.
- Test programs in `tests/`: `getint_rules`, `fault_unaligned_rom_store`.
