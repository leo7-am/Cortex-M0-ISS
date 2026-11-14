// iss.cpp - CSCE 2303 Cortex-M0 instruction set simulator.
// Build: make   (or: g++ -std=c++17 -O2 -Wall -o iss iss.cpp decoder.cpp)
// Run:   ./iss [--disasm] [--trace FILE] [--max-instr N] [--seed N] [--cpu m0|m0plus] PROGRAM

// Started from the course skeleton. CP1: load the dump, remember where .text is,
// and print the --disasm listing. Running programs (step) is still the
// skeleton's and gets replaced at CP2.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "decoder.hpp"

//  memory map
const uint32_t ROM_BASE = 0x00000000, ROM_SIZE = 1 << 20;     // 1 MB
const uint32_t RAM_BASE = 0x20000000, RAM_SIZE = 256 << 10;   // 256 KB
uint8_t rom[ROM_SIZE], ram[RAM_SIZE];

//  CPU state 
uint32_t r[16];             // r[13] = sp, r[14] = lr, r[15] = pc
int N, Z, C, V;             // APSR flags
uint64_t instret;           // instructions retired

// options from handout section 6
bool        opt_disasm    = false;
const char *opt_trace     = nullptr;
uint64_t    opt_max_instr = 100000000;
uint32_t    opt_seed      = 2463534242u;
const char *opt_cpu       = "m0";

// Where the .text section is, found while loading.
uint32_t text_start = 0xFFFFFFFF, text_end = 0;

void fault(const char *kind)
{
    fprintf(stderr, "FAULT %s at pc=0x%08x\n", kind, r[15]);
    exit(255);
}

// Pointer to the byte at addr, or nullptr if addr is unmapped.
uint8_t *mem_ptr(uint32_t addr)
{
    if (addr - ROM_BASE < ROM_SIZE) return &rom[addr - ROM_BASE];
    if (addr - RAM_BASE < RAM_SIZE) return &ram[addr - RAM_BASE];
    return nullptr;
}

// Little-endian read of 1, 2 or 4 bytes; faults like the real core.
uint32_t read_mem(uint32_t addr, int size)
{
    if (addr % size) fault("UNALIGNED");
    uint8_t *p = mem_ptr(addr);
    if (!p) fault("BUS");
    uint32_t v = 0;
    for (int i = size - 1; i >= 0; i--) v = v << 8 | p[i];
    return v;
}

void write_mem(uint32_t addr, uint32_t v, int size)
{
    if (addr % size) fault("UNALIGNED");
    if (addr - RAM_BASE >= RAM_SIZE) fault("BUS");      // only RAM is writable
    for (int i = 0; i < size; i++) ram[addr - RAM_BASE + i] = v >> (8 * i);
}

// Used by the disassembler: never faults, unmapped bytes read as 0.
uint16_t read16(uint32_t addr)
{
    uint8_t *lo = mem_ptr(addr), *hi = mem_ptr(addr + 1);
    return (lo ? *lo : 0) | (hi ? *hi : 0) << 8;
}

// Load a `readelf -x` dump. Data lines look like
//   "  0x00000100 094811df 0a2000f0 03f812df 002000df .H... ......"
// an address, up to 16 bytes in 35 columns, then ASCII we ignore.
// A line "Hex dump of section '.text':" starts the .text section.
void load_dump(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); exit(253); }
    char line[512];
    bool in_text = false;
    while (fgets(line, sizeof line, f)) {
        if (strstr(line, "Hex dump of section")) {
            in_text = strstr(line, "'.text'") != nullptr;
            continue;
        }
        char *s = line;
        while (*s == ' ') s++;
        if (strncmp(s, "0x", 2) != 0) continue;           // not a data line
        uint32_t addr = strtoul(s, &s, 16);
        s++;                                              // space after the address
        for (int i = 0; i + 1 < 35 && s[i] && s[i] != '\n'; ) {
            unsigned byte;
            if (s[i] == ' ') { i++; continue; }
            if (sscanf(s + i, "%2x", &byte) != 1) break;
            uint8_t *p = mem_ptr(addr);
            if (!p) { fprintf(stderr, "load error at 0x%08x\n", addr); exit(253); }
            *p = byte;
            if (in_text) {                                // grow the .text range
                if (addr < text_start) text_start = addr;
                if (addr + 1 > text_end) text_end = addr + 1;
            }
            addr++;
            i += 2;
        }
    }
    fclose(f);
}

void reset()
{
    r[13] = read_mem(0, 4) & ~3u;     // sp from address 0, word aligned
    r[15] = read_mem(4, 4);           // pc from the reset vector
    if (!(r[15] & 1)) fault("INVSTATE");
    r[15] &= ~1u;                     // bit 0 only marks Thumb
    r[14] = 0xFFFFFFFF;
}

void set_nz(uint32_t x) { N = x >> 31; Z = (x == 0); }

void svc(int n)
{
    switch (n) {
    case 0x00: exit(r[0] & 0xFF);                                      // EXIT
    case 0x10: putchar(r[0] & 0xFF); break;                            // PUTCHAR
    case 0x11: for (uint32_t a = r[0]; read_mem(a, 1); a++)            // PUTS
                   putchar(read_mem(a, 1));
               break;
    case 0x12: printf("%d", (int32_t)r[0]); break;                     // PUTINT
    // TODO (CP2): the other services in section 5
    default:   fault("UNDEFINED");
    }
}

// Execute one instruction (skeleton version; CP2 switches on decode().op).
void step()
{
    uint32_t pc = r[15];
    uint32_t hw = read_mem(pc, 2);
    uint32_t next = pc + 2;

    if ((hw & 0xF800) == 0x2000) {                    // movs rd, #imm8
        int rd = (hw >> 8) & 7;
        r[rd] = hw & 0xFF;
        set_nz(r[rd]);
    }
    else if ((hw & 0xF800) == 0xE000) {               // b label
        int32_t off = hw & 0x7FF;
        if (off & 0x400) off -= 0x800;
        next = pc + 4 + off * 2;
    }
    else if ((hw & 0xFF00) == 0xDF00) {               // svc #imm8
        svc(hw & 0xFF);
    }
    else {
        fault("UNDEFINED");                           // TODO (CP2): everything else
    }
    r[15] = next;
    instret++;
}

int main(int argc, char **argv)
{
    // Options first, then the program file.
    const char *program = nullptr;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        bool has_value = i + 1 < argc;
        if (a == "--disasm")                       opt_disasm = true;
        else if (a == "--trace" && has_value)      opt_trace = argv[++i];
        else if (a == "--max-instr" && has_value)  opt_max_instr = strtoull(argv[++i], nullptr, 0);
        else if (a == "--seed" && has_value)       opt_seed = strtoul(argv[++i], nullptr, 0);
        else if (a == "--cpu" && has_value)        opt_cpu = argv[++i];
        else if (a[0] != '-' && !program)          program = argv[i];
        else { program = nullptr; break; }         // bad option: show usage below
    }
    if (!program) {
        fprintf(stderr, "usage: iss [--disasm] [--trace FILE] [--max-instr N] "
                        "[--seed N] [--cpu m0|m0plus] PROGRAM\n");
        return 253;
    }

    load_dump(program);

    if (opt_disasm) {
        disassemble(text_start, text_end);
        return 0;
    }

    reset();
    while (true) {
        if (instret >= opt_max_instr) {
            fprintf(stderr, "LIMIT at pc=0x%08x\n", r[15]);
            return 254;
        }
        step();
    }
}
