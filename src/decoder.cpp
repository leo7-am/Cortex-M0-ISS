// decoder.cpp - Thumb (ARMv6-M) decoder and objdump-style disassembler.
// Bit layouts come from the ARMv6-M Architecture Reference
// Manual (DDI 0419), chapter A5.
#include "decoder.hpp"
#include <cstdarg>
#include <cstdio>
#include <vector>

static const char *REG[16] = { "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7","r8", "r9", "r10", "r11", "r12", "sp", "lr", "pc" };
static const char *COND[16] = { "eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc", "hi", "ls", "ge", "lt", "gt", "le", "al", "nv" };

// Bits hi..lo of v, like the manual's v<hi:lo>.
static uint32_t bits(uint32_t v, int hi, int lo)
{
    return (v >> lo) & ((1u << (hi - lo + 1)) - 1);
}

// Sign-extend the low n bits of v.
static int32_t sext(uint32_t v, int n)
{
    return (int32_t)(v << (32 - n)) >> (32 - n);
}

Insn decode(uint32_t pc, uint16_t hw, uint16_t hw2)
{
    Insn in;
    int lo3 = bits(hw, 2, 0), mid3 = bits(hw, 5, 3), hi3 = bits(hw, 8, 6);
    int r8 = bits(hw, 10, 8);                 // register in bits 10..8
    uint32_t imm8 = bits(hw, 7, 0), imm5 = bits(hw, 10, 6);
    bool load = bits(hw, 11, 11);             // L bit of load/store forms
    in.rd = lo3;                              // most forms put rd in bits 2..0

    // 32-bit (first 5 bits 11101, 11110 or 11111): only BL is in our subset.
    if ((hw >> 11) >= 0x1D) {
        in.size = 4;
        if ((hw >> 11) == 0x1E && (hw2 & 0xD000) == 0xD000) {
            uint32_t S  = bits(hw, 10, 10);
            uint32_t I1 = !(bits(hw2, 13, 13) ^ S);    // I1 = NOT(J1 XOR S)
            uint32_t I2 = !(bits(hw2, 11, 11) ^ S);    // I2 = NOT(J2 XOR S)
            uint32_t off = S << 24 | I1 << 23 | I2 << 22 | bits(hw, 9, 0) << 12 | bits(hw2, 10, 0) << 1;
            in.op = BL;
            in.target = pc + 4 + sext(off, 25);
        }
        return in;
    }

    if ((hw >> 11) == 0x03) {                         // 00011: add/sub, register or imm3
        bool sub = bits(hw, 9, 9);
        in.rn = mid3;
        if (bits(hw, 10, 10)) { in.op = sub ? SUBS_IMM3 : ADDS_IMM3; in.imm = hi3; }
        else                  { in.op = sub ? SUBS_REG  : ADDS_REG;  in.rm  = hi3; }
    }
    else if ((hw >> 13) == 0) {                       // 000: shift by immediate
        in.rm = mid3;
        int kind = bits(hw, 12, 11);
        if (kind == 0) {                              // LSL #0 is really MOVS rd, rm
            in.op = imm5 ? LSLS_IMM : MOVS_REG;
            in.imm = imm5;
        } else {                                      // LSR/ASR #0 means #32
            in.op = kind == 1 ? LSRS_IMM : ASRS_IMM;
            in.imm = imm5 ? imm5 : 32;
        }
    }
    else if ((hw >> 13) == 1) {                       // 001: movs/cmp/adds/subs #imm8
        static const Op ops[4] = { MOVS_IMM, CMP_IMM, ADDS_IMM8, SUBS_IMM8 };
        in.op = ops[bits(hw, 12, 11)];
        in.rd = in.rn = r8;
        in.imm = imm8;
    }
    else if ((hw >> 10) == 0x10) {                    // 010000: ALU on low registers
        static const Op ops[16] = { ANDS, EORS, LSLS_REG, LSRS_REG, ASRS_REG, ADCS, SBCS, RORS,
                                    TST,  NEGS, CMP,      CMN,      ORRS,     MULS, BICS, MVNS };
        in.op = ops[bits(hw, 9, 6)];
        in.rd = in.rn = lo3;                          // rd = rd <op> rm
        in.rm = mid3;
    }
    else if ((hw >> 10) == 0x11) {                    // 010001: high registers, BX, BLX
        int rdn = bits(hw, 7, 7) << 3 | lo3;
        in.rm = bits(hw, 6, 3);
        switch (bits(hw, 9, 8)) {
        case 0: in.op = ADD_HI; in.rd = in.rn = rdn; break;
        case 1: in.op = CMP;    in.rd = in.rn = rdn; break;
        case 2: in.op = MOV;    in.rd = rdn;         break;
        case 3: in.op = bits(hw, 7, 7) ? BLX : BX;   break;
        }
    }
    else if ((hw >> 11) == 0x09) {                    // 01001: ldr rd, [pc, #imm8*4]
        in.op = LDR_LIT;
        in.rd = r8;
        in.imm = imm8 * 4;
        in.target = ((pc + 4) & ~3u) + in.imm;        // pc+4 rounded down to a word
    }
    else if ((hw >> 12) == 0x5) {                     // 0101: [rn, rm] addressing
        static const Op ops[8] = { STR_REG, STRH_REG, STRB_REG, LDRSB_REG,
                                   LDR_REG, LDRH_REG, LDRB_REG, LDRSH_REG };
        in.op = ops[bits(hw, 11, 9)];
        in.rn = mid3;
        in.rm = hi3;
    }
    else if ((hw >> 12) == 0x6) {                     // 0110: word [rn, #imm5*4]
        in.op = load ? LDR_IMM : STR_IMM;  in.rn = mid3; in.imm = imm5 * 4;
    }
    else if ((hw >> 12) == 0x7) {                     // 0111: byte [rn, #imm5]
        in.op = load ? LDRB_IMM : STRB_IMM; in.rn = mid3; in.imm = imm5;
    }
    else if ((hw >> 12) == 0x8) {                     // 1000: halfword [rn, #imm5*2]
        in.op = load ? LDRH_IMM : STRH_IMM; in.rn = mid3; in.imm = imm5 * 2;
    }
    else if ((hw >> 12) == 0x9) {                     // 1001: word [sp, #imm8*4]
        in.op = load ? LDR_SP : STR_SP;     in.rd = r8; in.rn = 13; in.imm = imm8 * 4;
    }
    else if ((hw >> 12) == 0xA) {                     // 1010: adr / add rd, sp, #imm8*4
        in.rd = r8;
        in.imm = imm8 * 4;
        if (bits(hw, 11, 11)) { in.op = ADD_RD_SP; in.rn = 13; }
        else                  { in.op = ADR; in.target = ((pc + 4) & ~3u) + in.imm; }
    }
    else if ((hw & 0xFF00) == 0xB000) {               // add/sub sp, #imm7*4
        in.op = bits(hw, 7, 7) ? SUB_SP : ADD_SP;
        in.imm = bits(hw, 6, 0) * 4;
    }
    else if ((hw & 0xFF00) == 0xB200) {               // sxth/sxtb/uxth/uxtb
        static const Op ops[4] = { SXTH, SXTB, UXTH, UXTB };
        in.op = ops[bits(hw, 7, 6)];
        in.rm = mid3;
    }
    else if ((hw & 0xFFC0) == 0xBA00) {               // rev (rev16/revsh are excluded)
        in.op = REV;
        in.rm = mid3;
    }
    else if ((hw & 0xFE00) == 0xB400) {               // push {list, lr?}
        in.op = PUSH;
        in.list = imm8 | bits(hw, 8, 8) << 14;
    }
    else if ((hw & 0xFE00) == 0xBC00) {               // pop {list, pc?}
        in.op = POP;
        in.list = imm8 | bits(hw, 8, 8) << 15;
    }
    else if (hw == 0xBF00) {                          // nop (other hints are excluded)
        in.op = NOP;
    }
    else if ((hw >> 12) == 0xC) {                     // 1100: stmia / ldmia rn!, {list}
        in.op = load ? LDM : STM;
        in.rn = r8;
        in.list = imm8;
    }
    else if ((hw >> 8) == 0xDF) {                     // svc #imm8 (condition 1111)
        in.op = SVC;
        in.imm = imm8;
    }
    else if ((hw >> 12) == 0xD && bits(hw, 11, 8) != 14) {   // b<cond> (1110 is undefined)
        in.op = B_COND;
        in.cond = bits(hw, 11, 8);
        in.target = pc + 4 + sext(imm8, 8) * 2;
    }
    else if ((hw >> 11) == 0x1C) {                    // 11100: b label
        in.op = B;
        in.target = pc + 4 + sext(bits(hw, 10, 0), 11) * 2;
    }
    // Anything else stays UNDEF.
    return in;
}

// printf into a std::string.
static std::string fmt(const char *f, ...)
{
    char buf[100];
    va_list args;
    va_start(args, f);
    vsnprintf(buf, sizeof buf, f, args);
    va_end(args);
    return buf;
}

// "{r4, r5, lr}" - registers written out one by one, like objdump.
static std::string reglist(uint32_t list)
{
    std::string s = "{";
    for (int r = 0; r < 16; r++)
        if (list >> r & 1) s += (s.size() > 1 ? ", " : "") + std::string(REG[r]);
    return s + "}";
}

std::string disasm(const Insn &in)
{
    const char *d = REG[in.rd], *n = REG[in.rn], *m = REG[in.rm];
    switch (in.op) {
    case MOVS_IMM:  return fmt("movs %s, #%u", d, in.imm);
    case MOVS_REG:  return fmt("movs %s, %s", d, m);
    case MOV:       return (in.rd == 8 && in.rm == 8) ? "nop" : fmt("mov %s, %s", d, m);
    case MVNS:      return fmt("mvns %s, %s", d, m);

    case ADDS_REG:  return fmt("adds %s, %s, %s", d, n, m);
    case SUBS_REG:  return fmt("subs %s, %s, %s", d, n, m);
    case ADDS_IMM3: return fmt("adds %s, %s, #%u", d, n, in.imm);
    case SUBS_IMM3: return fmt("subs %s, %s, #%u", d, n, in.imm);
    case ADDS_IMM8: return fmt("adds %s, #%u", d, in.imm);
    case SUBS_IMM8: return fmt("subs %s, #%u", d, in.imm);
    case ADD_HI:    return fmt("add %s, %s", d, m);
    case ADD_RD_SP: return fmt("add %s, sp, #%u", d, in.imm);
    case ADD_SP:    return fmt("add sp, #%u", in.imm);
    case SUB_SP:    return fmt("sub sp, #%u", in.imm);
    case ADR:       return fmt("add %s, pc, #%u", d, in.imm);   // objdump's spelling of adr
    case ADCS:      return fmt("adcs %s, %s", d, m);
    case SBCS:      return fmt("sbcs %s, %s", d, m);
    case NEGS:      return fmt("negs %s, %s", d, m);
    case MULS:      return fmt("muls %s, %s", d, m);

    case ANDS:      return fmt("ands %s, %s", d, m);
    case ORRS:      return fmt("orrs %s, %s", d, m);
    case EORS:      return fmt("eors %s, %s", d, m);
    case BICS:      return fmt("bics %s, %s", d, m);
    case TST:       return fmt("tst %s, %s", n, m);
    case CMP_IMM:   return fmt("cmp %s, #%u", n, in.imm);
    case CMP:       return fmt("cmp %s, %s", n, m);
    case CMN:       return fmt("cmn %s, %s", n, m);
    case LSLS_IMM:  return fmt("lsls %s, %s, #%u", d, m, in.imm);
    case LSRS_IMM:  return fmt("lsrs %s, %s, #%u", d, m, in.imm);
    case ASRS_IMM:  return fmt("asrs %s, %s, #%u", d, m, in.imm);
    case LSLS_REG:  return fmt("lsls %s, %s", d, m);
    case LSRS_REG:  return fmt("lsrs %s, %s", d, m);
    case ASRS_REG:  return fmt("asrs %s, %s", d, m);
    case RORS:      return fmt("rors %s, %s", d, m);

    case SXTB:      return fmt("sxtb %s, %s", d, m);
    case SXTH:      return fmt("sxth %s, %s", d, m);
    case UXTB:      return fmt("uxtb %s, %s", d, m);
    case UXTH:      return fmt("uxth %s, %s", d, m);
    case REV:       return fmt("rev %s, %s", d, m);

    case LDR_IMM:   return fmt("ldr %s, [%s, #%u]", d, n, in.imm);
    case LDRB_IMM:  return fmt("ldrb %s, [%s, #%u]", d, n, in.imm);
    case LDRH_IMM:  return fmt("ldrh %s, [%s, #%u]", d, n, in.imm);
    case STR_IMM:   return fmt("str %s, [%s, #%u]", d, n, in.imm);
    case STRB_IMM:  return fmt("strb %s, [%s, #%u]", d, n, in.imm);
    case STRH_IMM:  return fmt("strh %s, [%s, #%u]", d, n, in.imm);
    case LDR_REG:   return fmt("ldr %s, [%s, %s]", d, n, m);
    case LDRB_REG:  return fmt("ldrb %s, [%s, %s]", d, n, m);
    case LDRH_REG:  return fmt("ldrh %s, [%s, %s]", d, n, m);
    case LDRSB_REG: return fmt("ldrsb %s, [%s, %s]", d, n, m);
    case LDRSH_REG: return fmt("ldrsh %s, [%s, %s]", d, n, m);
    case STR_REG:   return fmt("str %s, [%s, %s]", d, n, m);
    case STRB_REG:  return fmt("strb %s, [%s, %s]", d, n, m);
    case STRH_REG:  return fmt("strh %s, [%s, %s]", d, n, m);
    case LDR_SP:    return fmt("ldr %s, [sp, #%u]", d, in.imm);
    case STR_SP:    return fmt("str %s, [sp, #%u]", d, in.imm);
    case LDR_LIT:   return fmt("ldr %s, [pc, #%u]", d, in.imm);

    // LDM writes back only when the base register is not in the list.
    case LDM:  return fmt("ldmia %s%s, ", n, (in.list >> in.rn & 1) ? "" : "!") + reglist(in.list);
    case STM:  return fmt("stmia %s!, ", n) + reglist(in.list);
    case PUSH: return "push " + reglist(in.list);
    case POP:  return "pop " + reglist(in.list);

    case B_COND: return fmt("b%s %x", COND[in.cond], in.target);
    case B:      return fmt("b %x", in.target);
    case BL:     return fmt("bl %x", in.target);
    case BX:     return fmt("bx %s", m);
    case BLX:    return fmt("blx %s", m);
    case SVC:    return fmt("svc %u", in.imm);      // decimal, like objdump
    case NOP:    return "nop";
    default:     return "undefined";
    }
}

void disassemble(uint32_t start, uint32_t end)
{
    // Pass 1: every word loaded by "ldr rX, [pc, #imm]" is literal-pool data.
    // Remembering them stops a data word from being decoded as code.
    std::vector<bool> is_data(end - start + 4, false);
    for (uint32_t a = start; a < end; ) {
        Insn in = decode(a, read16(a), read16(a + 2));
        if (in.op == LDR_LIT && in.target >= start && in.target < end)
            for (int i = 0; i < 4; i++) is_data[in.target - start + i] = true;
        a += in.size;
    }

    // Pass 2: print one line per instruction, "address: encoding  text".
    for (uint32_t a = start; a < end; ) {
        if (is_data[a - start]) {
            uint32_t word = read16(a) | (uint32_t)read16(a + 2) << 16;
            printf("%08x: %08x  .word 0x%08x\n", a, word, word);
            a += 4;
            continue;
        }
        uint16_t hw = read16(a), hw2 = read16(a + 2);
        Insn in = decode(a, hw, hw2);
        if (in.size == 4) printf("%08x: %04x %04x  %s\n", a, hw, hw2, disasm(in).c_str());
        else              printf("%08x: %04x       %s\n", a, hw, disasm(in).c_str());
        a += in.size;
    }
}
