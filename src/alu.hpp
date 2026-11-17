#pragma once
#include <cstdint>
#include "decoder.hpp"

/* Input: in, the decoded instruction, pc, its address, and next, the address of the
   next instruction.
   Output: true if it was a move, arithmetic, logic, shift, extend or branch instruction
   and it ran; false if not, with nothing changed. A branch sets next to its target.
*/
bool execute_alu(const Insn &in, uint32_t pc, uint32_t &next);
