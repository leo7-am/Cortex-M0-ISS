// iss.cpp - CSCE 2303 Cortex-M0 instruction set simulator: main().
// Build: make
// Run:   ./iss [--disasm] [--trace FILE] [--max-instr N] [--seed N] [--cpu m0|m0plus] PROGRAM
//
// Files:
//   iss.cpp      command line (Member A at CP1, Member C at CP2)
//   machine.cpp  memory, loader, reset, fetch loop, faults, SVC, trace, cycles (Member C)
//   execute.cpp  the instructions (Members A and B)
//   decoder.cpp  decoder and --disasm (Member A)
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "decoder.hpp"
#include "machine.hpp"

// Read a number in decimal, hex (0x...) or octal (0...). False if it isn't one.
static bool parse_number(const char *s, uint64_t &out)
{
    char *end;
    out = strtoull(s, &end, 0);
    return *s != '\0' && *s != '-' && *end == '\0';
}

int main(int argc, char **argv)
{
    // options from handout section 6, then the program file
    bool        opt_disasm    = false;
    const char *opt_trace     = nullptr;
    uint64_t    opt_max_instr = 100000000;
    uint64_t    opt_seed      = 2463534242u;
    std::string opt_cpu       = "m0";
    const char *program       = nullptr;

    bool ok = true;
    for (int i = 1; i < argc && ok; i++) {
        std::string a = argv[i];
        bool has_value = i + 1 < argc;
        if (a == "--disasm")                       opt_disasm = true;
        else if (a == "--trace" && has_value)      opt_trace = argv[++i];
        else if (a == "--max-instr" && has_value)  ok = parse_number(argv[++i], opt_max_instr);
        else if (a == "--seed" && has_value)       ok = parse_number(argv[++i], opt_seed);
        else if (a == "--cpu" && has_value)        { opt_cpu = argv[++i];
                                                     ok = opt_cpu == "m0" || opt_cpu == "m0plus"; }
        else if (a[0] != '-' && !program)          program = argv[i];
        else                                       ok = false;   // unknown option or 2nd program
    }
    if (!ok || !program) {
        fprintf(stderr, "usage: iss [--disasm] [--trace FILE] [--max-instr N] "
                        "[--seed N] [--cpu m0|m0plus] PROGRAM\n");
        return 2;
    }

    load_program(program);                       // exits 253 if the dump is bad

    if (opt_disasm) {
        for (auto &range : text_ranges) disassemble(range.first, range.second);
        return 0;
    }

    set_options((uint32_t)opt_seed, opt_cpu == "m0plus");
    if (opt_trace && !open_trace(opt_trace)) {
        perror(opt_trace);
        return 253;
    }
    int status = run(opt_max_instr);
    fflush(stdout);
    close_trace();
    return status;
}
