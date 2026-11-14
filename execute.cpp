// execute.cpp - runs ONE instruction (members A and B add their instructions here).
// The machine (machine.cpp) calls execute() once per instruction with r[15] = its address.
// Read the top of machine.hpp first: it lists the functions to use and THE RULE
// (no register or flag changes before you know the instruction won't fault).
//
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

    // TODO (Member A): loads and stores
    //   LDR_IMM, LDRB_IMM, LDRH_IMM, STR_IMM, STRB_IMM, STRH_IMM,
    //   LDR_REG, LDRB_REG, LDRH_REG, LDRSB_REG, LDRSH_REG, STR_REG, STRB_REG, STRH_REG,
    //   LDR_SP, STR_SP, LDR_LIT, LDM, STM, PUSH, POP
    //   (POP {pc}: bit 0 of the loaded value must be 1, else fault("INVSTATE"); taken_branch())

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
