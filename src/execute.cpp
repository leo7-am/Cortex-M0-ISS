// execute.cpp - runs ONE instruction (members A and B add their instructions here).
// The machine (machine.cpp) calls execute() once per instruction with r[15] = its address.
// Read the top of machine.hpp first: it lists the functions to use and THE RULE
// (no register or flag changes before you know the instruction won't fault).

// For now only the course skeleton's three instructions are here: MOVS #imm8, B and SVC.
#include "machine.hpp"
#include "decoder.hpp"

static void set_nz(uint32_t x) { N = x >> 31; Z = (x == 0); }

void execute()
{
    uint32_t pc = r[15];
    uint16_t hw = fetch16(pc);
    // read16 peeks at the next halfword without faulting, because decode() needs it
    // to recognise BL. Only a real 32-bit instruction fetches it properly.
    Insn in = decode(pc, hw, read16(pc + 2));
    if (in.size == 4) fetch16(pc + 2);           // BL: 2nd halfword (traced, may fault)
    uint32_t next = pc + in.size;

    switch (in.op) {
    case MOVS_IMM:                               // movs rd, #imm8
        set_reg(in.rd, in.imm);
        set_nz(r[in.rd]);
        break;

    case B:                                      // b label
        taken_branch();
        next = in.target;
        break;

    case SVC:                                    // svc #imm8
        svc(in.imm);
        break;

    //Member A: loads and stores 
    // Address = base register + immediate. For the sp forms the decoder already
    // set rn = 13 (sp), so they work the same way.
    case LDR_IMM:
    case LDR_SP:   set_reg(in.rd, mem_read(r[in.rn] + in.imm, 4)); 
        break;
    case LDRH_IMM: set_reg(in.rd, mem_read(r[in.rn] + in.imm, 2));
         break;
    case LDRB_IMM: set_reg(in.rd, mem_read(r[in.rn] + in.imm, 1)); 
        break;
    case STR_IMM:
    case STR_SP:   mem_write(r[in.rn] + in.imm, r[in.rd], 4);
         break;
    case STRH_IMM: mem_write(r[in.rn] + in.imm, r[in.rd], 2);
         break;
    case STRB_IMM: mem_write(r[in.rn] + in.imm, r[in.rd], 1); 
        break;

    // Address = base register + index register.
    case LDR_REG:  set_reg(in.rd, mem_read(r[in.rn] + r[in.rm], 4)); 
        break;
    case LDRH_REG: set_reg(in.rd, mem_read(r[in.rn] + r[in.rm], 2)); 
        break;
    case LDRB_REG: set_reg(in.rd, mem_read(r[in.rn] + r[in.rm], 1)); 
        break;
    case STR_REG:  mem_write(r[in.rn] + r[in.rm], r[in.rd], 4);
         break;
    case STRH_REG: mem_write(r[in.rn] + r[in.rm], r[in.rd], 2); 
        break;
    case STRB_REG: mem_write(r[in.rn] + r[in.rm], r[in.rd], 1); 
        break;

    // Signed loads: copy the top bit of the byte/halfword into the upper bits.
    case LDRSB_REG: {
        int8_t value = mem_read(r[in.rn] + r[in.rm], 1);
        set_reg(in.rd, (int32_t)value);
        break;
    }
    case LDRSH_REG: {
        int16_t value = mem_read(r[in.rn] + r[in.rm], 2);
        set_reg(in.rd, (int32_t)value);
        break;
    }

    // ldr rd, [pc, #imm]: the decoder already worked out the address.
    case LDR_LIT:  set_reg(in.rd, mem_read(in.target, 4)); break;

    // Member A: multiple registers
    // Lowest-numbered register goes to the lowest address.
    case STM: {                                  // stmia rn!, {list}
        uint32_t addr = r[in.rn];
        for (int i = 0; i < 8; i++)
            if (in.list >> i & 1) { mem_write(addr, r[i], 4); addr += 4; }
        set_reg(in.rn, addr);                    // STM always writes back
        break;
    }
    case LDM: {                                  // ldmia rn!, {list}
        uint32_t addr = r[in.rn];
        uint32_t value[8] = {};
        for (int i = 0; i < 8; i++)              // read everything first (THE RULE)
            if (in.list >> i & 1) { value[i] = mem_read(addr, 4); addr += 4; }
        for (int i = 0; i < 8; i++)
            if (in.list >> i & 1) set_reg(i, value[i]);
        if (!(in.list >> in.rn & 1))             // write back only if rn is not in the list
            set_reg(in.rn, addr);
        break;
    }
    case PUSH: {                                 // push {list, lr}: list bit 14 = lr
        int count = 0;
        for (int i = 0; i < 16; i++) if (in.list >> i & 1) count++;
        uint32_t addr = r[13] - 4 * count;       // new sp: the stack grows down
        for (int i = 0; i < 16; i++)
            if (in.list >> i & 1) { mem_write(addr, r[i], 4); addr += 4; }
        set_reg(13, r[13] - 4 * count);
        break;
    }
    case POP: {                                  // pop {list, pc}: list bit 15 = pc
        uint32_t addr = r[13];
        uint32_t value[16] = {};
        for (int i = 0; i < 16; i++)             // read everything first (THE RULE)
            if (in.list >> i & 1) { value[i] = mem_read(addr, 4); addr += 4; }
        bool pops_pc = in.list >> 15 & 1;
        if (pops_pc && !(value[15] & 1))         // pc needs bit 0 set (Thumb)
            fault("INVSTATE");
        for (int i = 0; i < 15; i++)
            if (in.list >> i & 1) set_reg(i, value[i]);
        set_reg(13, addr);
        if (pops_pc) {                           // popping pc is a return (a branch)
            next = value[15] & ~1u;
            taken_branch();
        }
        break;
    }

    // TODO (Member B): arithmetic, logic, shifts, flags, branches
    //   MOVS_REG, MOV, MVNS,
    //   ADDS_REG, ADDS_IMM3, ADDS_IMM8, ADD_HI, ADD_RD_SP, ADD_SP, SUB_SP, ADR,
    //   ADCS, SUBS_REG, SUBS_IMM3, SUBS_IMM8, SBCS, NEGS, MULS,
    //   ANDS, ORRS, EORS, BICS, TST, CMP_IMM, CMP, CMN,
    //   LSLS_IMM, LSRS_IMM, ASRS_IMM, LSLS_REG, LSRS_REG, ASRS_REG, RORS,
    //   SXTB, SXTH, UXTB, UXTH, REV, NOP,
    //   B_COND (taken_branch() only when taken), BL (taken_branch() + bl_extra()), BX, BLX

    default:                                     // UNDEF, and anything not written yet
        fault("UNDEFINED");
    }
    r[15] = next;
}
