#include "alu.hpp"
#include "machine.hpp"

/* Input: x, a result.
   Output: none. Sets N and Z from x.
*/
static void set_nz(uint32_t x)
{
    N = x >> 31;
    Z = (x == 0);
}

/* Input: n, a register number.
   Output: the register's value as an instruction sees it; pc reads as its address + 4.
*/
static uint32_t reg_value(int n)
{
    return n == 15 ? r[15] + 4 : r[n];
}

/* Input: x, y and carry_in (0 or 1).
   Output: x + y + carry_in.
   Sets N, Z, C and V like AddWithCarry in the manual. Subtraction is x + ~y + 1.
*/
static uint32_t add_with_carry(uint32_t x, uint32_t y, int carry_in)
{
    uint64_t usum = (uint64_t)x + y + carry_in;
    int64_t ssum = (int64_t)(int32_t)x + (int32_t)y + carry_in;
    uint32_t result = (uint32_t)usum;
    set_nz(result);
    C = (usum >> 32) & 1;
    V = ssum != (int32_t)result;
    return result;
}

/* Input: x, the value, and n, the shift amount (0 to 255).
   Output: x shifted left by n. C gets the last bit shifted out; n = 0 leaves C alone.
*/
static uint32_t shift_lsl(uint32_t x, uint32_t n)
{
    if (n == 0) return x;
    if (n < 32) { C = (x >> (32 - n)) & 1; return x << n; }
    C = (n == 32) ? (x & 1) : 0;
    return 0;
}

/* Input: x and n.
   Output: x shifted right logically by n, with C set as in shift_lsl.
*/
static uint32_t shift_lsr(uint32_t x, uint32_t n)
{
    if (n == 0) return x;
    if (n < 32) { C = (x >> (n - 1)) & 1; return x >> n; }
    C = (n == 32) ? (x >> 31) : 0;
    return 0;
}

/* Input: x and n.
   Output: x shifted right arithmetically by n. 32 and above fill every bit with the sign.
*/
static uint32_t shift_asr(uint32_t x, uint32_t n)
{
    if (n == 0) return x;
    if (n < 32) { C = (x >> (n - 1)) & 1; return (uint32_t)((int32_t)x >> n); }
    C = x >> 31;
    return C ? 0xFFFFFFFFu : 0;
}

/* Input: x and n.
   Output: x rotated right by n. Only n mod 32 moves bits, but any nonzero n sets C to bit 31.
*/
static uint32_t shift_ror(uint32_t x, uint32_t n)
{
    if (n == 0) return x;
    n &= 31;
    if (n) x = (x >> n) | (x << (32 - n));
    C = x >> 31;
    return x;
}

/* Input: cond, the 4-bit condition of a B<cond>.
   Output: true if the flags pass it.
*/
static bool cond_passed(int cond)
{
    switch (cond) {
    case 0x0: return Z;                  // eq
    case 0x1: return !Z;                 // ne
    case 0x2: return C;                  // cs
    case 0x3: return !C;                 // cc
    case 0x4: return N;                  // mi
    case 0x5: return !N;                 // pl
    case 0x6: return V;                  // vs
    case 0x7: return !V;                 // vc
    case 0x8: return C && !Z;            // hi
    case 0x9: return !C || Z;            // ls
    case 0xA: return N == V;             // ge
    case 0xB: return N != V;             // lt
    case 0xC: return !Z && N == V;       // gt
    case 0xD: return Z || N != V;        // le
    }
    return false;
}

/* Input: in, the decoded instruction, pc, its address, and next, the address of the
   next instruction.
   Output: true if the instruction was one of Member B's and it ran, false if not.
   Runs moves, arithmetic, logic, compares, shifts, extends and branches. A branch
   changes next and calls taken_branch() for the cycle count.
*/
bool execute_alu(const Insn &in, uint32_t pc, uint32_t &next)
{
    switch (in.op) {
    // moves
    case MOVS_REG:                               // movs rd, rm
        set_reg(in.rd, r[in.rm]);
        set_nz(r[in.rd]);
        break;
    case MOV: {                                  // mov rd, rm (any registers, flags kept)
        uint32_t value = reg_value(in.rm);
        if (in.rd == 15) { next = value & ~1u; taken_branch(); }
        else set_reg(in.rd, value);
        break;
    }
    case MVNS:                                   // mvns rd, rm
        set_reg(in.rd, ~r[in.rm]);
        set_nz(r[in.rd]);
        break;
    case NOP:
        break;

    // arithmetic
    case ADDS_REG:  set_reg(in.rd, add_with_carry(r[in.rn], r[in.rm], 0));      break;
    case ADDS_IMM3:
    case ADDS_IMM8: set_reg(in.rd, add_with_carry(r[in.rn], in.imm, 0));        break;
    case SUBS_REG:  set_reg(in.rd, add_with_carry(r[in.rn], ~r[in.rm], 1));     break;
    case SUBS_IMM3:
    case SUBS_IMM8: set_reg(in.rd, add_with_carry(r[in.rn], ~in.imm, 1));       break;
    case ADCS:      set_reg(in.rd, add_with_carry(r[in.rn], r[in.rm], C));      break;
    case SBCS:      set_reg(in.rd, add_with_carry(r[in.rn], ~r[in.rm], C));     break;
    case NEGS:      set_reg(in.rd, add_with_carry(~r[in.rm], 0, 1));            break;
    case MULS:                                   // only N and Z change
        set_reg(in.rd, r[in.rn] * r[in.rm]);
        set_nz(r[in.rd]);
        break;
    case ADD_HI: {                               // add rd, rm (flags kept)
        uint32_t value = reg_value(in.rn) + reg_value(in.rm);
        if (in.rd == 15) { next = value & ~1u; taken_branch(); }
        else set_reg(in.rd, value);
        break;
    }
    case ADD_RD_SP: set_reg(in.rd, r[13] + in.imm); break;   // add rd, sp, #imm
    case ADD_SP:    set_reg(13, r[13] + in.imm);    break;   // add sp, #imm
    case SUB_SP:    set_reg(13, r[13] - in.imm);    break;   // sub sp, #imm
    case ADR:       set_reg(in.rd, in.target);      break;   // decoder worked out the address

    // logic and compare
    case ANDS: set_reg(in.rd, r[in.rn] & r[in.rm]);  set_nz(r[in.rd]); break;
    case ORRS: set_reg(in.rd, r[in.rn] | r[in.rm]);  set_nz(r[in.rd]); break;
    case EORS: set_reg(in.rd, r[in.rn] ^ r[in.rm]);  set_nz(r[in.rd]); break;
    case BICS: set_reg(in.rd, r[in.rn] & ~r[in.rm]); set_nz(r[in.rd]); break;
    case TST:  set_nz(r[in.rn] & r[in.rm]); break;
    case CMP_IMM: add_with_carry(r[in.rn], ~in.imm, 1); break;
    case CMP:     add_with_carry(reg_value(in.rn), ~reg_value(in.rm), 1); break;
    case CMN:     add_with_carry(r[in.rn], r[in.rm], 0); break;

    // shifts (immediate amounts: the decoder already turned #0 into #32)
    case LSLS_IMM: set_reg(in.rd, shift_lsl(r[in.rm], in.imm)); set_nz(r[in.rd]); break;
    case LSRS_IMM: set_reg(in.rd, shift_lsr(r[in.rm], in.imm)); set_nz(r[in.rd]); break;
    case ASRS_IMM: set_reg(in.rd, shift_asr(r[in.rm], in.imm)); set_nz(r[in.rd]); break;
    // register amounts: only the bottom byte of rm counts
    case LSLS_REG: set_reg(in.rd, shift_lsl(r[in.rd], r[in.rm] & 0xFF)); set_nz(r[in.rd]); break;
    case LSRS_REG: set_reg(in.rd, shift_lsr(r[in.rd], r[in.rm] & 0xFF)); set_nz(r[in.rd]); break;
    case ASRS_REG: set_reg(in.rd, shift_asr(r[in.rd], r[in.rm] & 0xFF)); set_nz(r[in.rd]); break;
    case RORS:     set_reg(in.rd, shift_ror(r[in.rd], r[in.rm] & 0xFF)); set_nz(r[in.rd]); break;

    // extend and reverse (flags kept)
    case SXTB: set_reg(in.rd, (uint32_t)(int32_t)(int8_t)r[in.rm]);  break;
    case SXTH: set_reg(in.rd, (uint32_t)(int32_t)(int16_t)r[in.rm]); break;
    case UXTB: set_reg(in.rd, r[in.rm] & 0xFF);   break;
    case UXTH: set_reg(in.rd, r[in.rm] & 0xFFFF); break;
    case REV: {
        uint32_t x = r[in.rm];
        set_reg(in.rd, (x >> 24) | ((x >> 8) & 0xFF00) | ((x << 8) & 0xFF0000) | (x << 24));
        break;
    }

    // branches
    case B_COND:                                 // b<cond> label
        if (cond_passed(in.cond)) { taken_branch(); next = in.target; }
        break;
    case BL:                                     // bl label: lr = return address | 1
        set_reg(14, (pc + 4) | 1);
        taken_branch();
        bl_extra();
        next = in.target;
        break;
    case BX:
    case BLX: {                                  // bx rm / blx rm: bit 0 must be 1
        uint32_t target = reg_value(in.rm);
        if (!(target & 1)) fault("INVSTATE");   // check first: a fault leaves lr alone
        if (in.op == BLX) set_reg(14, (pc + 2) | 1);
        taken_branch();
        next = target & ~1u;
        break;
    }

    default:
        return false;
    }
    return true;
}
