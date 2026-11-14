// machine.hpp - the "machine" around the CPU (Member C: Farah Habib).
//
// The machine owns: the memory map, the dump loader, reset, the fetch loop and
// instruction limit, faults, every SVC service, the trace and the cycle counter.
// The instructions themselves are in execute.cpp (members A and B).
//
// ============================== HOW TO USE THIS FROM execute.cpp ===========================
//
// The machine calls execute() once per instruction. When it is called, r[15] holds the
// address of the instruction. execute() must:
//   1. get the instruction with fetch16() (twice for BL: the second halfword at pc + 2),
//   2. decode it with decode() from decoder.hpp and switch on Insn::op,
//   3. do what the instruction does, using only the functions below,
//   4. set r[15] to the address of the next instruction (pc + 2, pc + 4 or a branch target).
//
// Registers and flags
//   r[0..15]          read registers directly: r[13] = sp, r[14] = lr, r[15] = pc.
//   set_reg(n, value) write a register. Use this instead of r[n] = value: writing sp
//                     (n = 13) clears bits 1:0, like the real M0.
//   N, Z, C, V        the APSR flags, each 0 or 1. Set them yourself in your instructions.
//
// Memory (data loads and stores)
//   mem_read(addr, size)          size is 1, 2 or 4. Returns the value (zero-extended).
//   mem_write(addr, value, size)  stores the low `size` bytes of value.
//   Both check the access (misaligned -> UNALIGNED, unmapped or a store to ROM -> BUS),
//   count one extra cycle, and mem_write records the store for the trace line.
//   LDM/STM/PUSH/POP: call them once per register, so each register costs one cycle.
//
// Instruction fetch
//   fetch16(addr)     the halfword at addr. It is added to the trace encoding.
//                     A failed fetch reports the fetch address in the FAULT line.
//
// Timing
//   taken_branch()    call once for every TAKEN branch: B<cond> when taken, B, BL, BX, BLX,
//                     POP {..., pc}, and ADD/MOV with pc as the destination.
//                     Adds 2 cycles (1 with --cpu m0plus).
//   bl_extra()        call once for BL, on top of taken_branch(): one more cycle.
//
// SVC and faults
//   svc(n)            the SVC instruction calls this with its 8-bit number.
//   fault(kind)       "UNDEFINED", "UNALIGNED", "BUS" or "INVSTATE". It throws a Fault,
//                     so it never returns. The FAULT line shows the instruction's address.
//
// THE RULE: don't change any register or flag before you know the instruction won't
// fault. Do every mem_read / check first, then write the results. A faulting
// instruction must not "half happen": the machine gives it no trace line, no count and
// no cycles, and it should leave no changed registers behind either.
//   Example (POP): read every word with mem_read first, and only then set the registers.
// ===========================================================================================
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

// ---- memory map
const uint32_t ROM_BASE = 0x00000000, ROM_SIZE = 1u << 20;     // 1 MB, read-only
const uint32_t RAM_BASE = 0x20000000, RAM_SIZE = 256u << 10;   // 256 KB, read/write

// ---- CPU state (shared with execute.cpp)
extern uint32_t r[16];              // r[13] = sp, r[14] = lr, r[15] = pc
extern int      N, Z, C, V;         // APSR flags, 0 or 1
extern uint64_t instret;            // instructions retired
extern uint64_t cycles;             // course cycle model

// ---- thrown by fault(): the fetch loop catches it and prints the FAULT line
struct Fault {
    const char *kind;               // "UNDEFINED", "UNALIGNED", "BUS" or "INVSTATE"
    uint32_t    pc;                 // address printed in the FAULT line
};

// ---- for execute.cpp (members A and B)
void     set_reg(int n, uint32_t value);
uint32_t mem_read(uint32_t addr, int size);
void     mem_write(uint32_t addr, uint32_t value, int size);
uint16_t fetch16(uint32_t addr);
void     taken_branch();
void     bl_extra();
void     svc(int n);
[[noreturn]] void fault(const char *kind);

// ---- written in execute.cpp: run the one instruction at r[15]
void execute();

// ---- for iss.cpp (main)
extern std::vector<std::pair<uint32_t, uint32_t>> text_ranges;  // [start, end) of .text
void load_program(const char *path);    // exits 253 on any load error
void set_options(uint32_t seed, bool m0plus);
bool open_trace(const char *path);      // false if the file can't be opened
void close_trace();
int  run(uint64_t max_instr);           // reset + fetch loop; returns the exit status
