// decoder.hpp - Thumb (ARMv6-M) decoder and disassembler. 
// decode() turns one instruction into an Insn. --disasm prints it with
// disasm(); at CP2 the executor will switch on Insn::op the same way.
#pragma once
#include <cstdint>
#include <string>

enum Op {
    UNDEF,
    // move
    MOVS_IMM, MOVS_REG, MOV, MVNS,
    // arithmetic
    ADDS_REG, ADDS_IMM3, ADDS_IMM8, ADD_HI, ADD_RD_SP, ADD_SP, SUB_SP, ADR,
    ADCS, SUBS_REG, SUBS_IMM3, SUBS_IMM8, SBCS, NEGS, MULS,
    // logic, shifts, compare
    ANDS, ORRS, EORS, BICS, TST, CMP_IMM, CMP, CMN,
    LSLS_IMM, LSRS_IMM, ASRS_IMM, LSLS_REG, LSRS_REG, ASRS_REG, RORS,
    // extend and reverse
    SXTB, SXTH, UXTB, UXTH, REV,
    // loads and stores
    LDR_IMM, LDRB_IMM, LDRH_IMM, STR_IMM, STRB_IMM, STRH_IMM,
    LDR_REG, LDRB_REG, LDRH_REG, LDRSB_REG, LDRSH_REG, STR_REG, STRB_REG, STRH_REG,
    LDR_SP, STR_SP, LDR_LIT,
    LDM, STM, PUSH, POP,
    // control and system
    B_COND, B, BL, BX, BLX, SVC, NOP
};

struct Insn {
    Op       op = UNDEF;
    int      size = 2;              // bytes: 2, or 4 for BL
    int      rd = 0, rn = 0, rm = 0;
    uint32_t imm = 0;               // immediate, already scaled (e.g. word offset * 4)
    int      cond = 0;              // condition for B_COND
    uint32_t list = 0;              // register list (bit n = rn) for LDM/STM/PUSH/POP
    uint32_t target = 0;            // branch target, literal address or ADR result
};

// Decode the instruction at address pc. hw2 is only used for 32-bit BL.
Insn decode(uint32_t pc, uint16_t hw, uint16_t hw2);

// objdump-style text, e.g. "push {r4, lr}".
std::string disasm(const Insn &in);

// Print the --disasm listing for addresses [start, end).
void disassemble(uint32_t start, uint32_t end);

// Supplied by iss.cpp: the halfword at addr, 0 if unmapped. Never faults.
uint16_t read16(uint32_t addr);
