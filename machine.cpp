// machine.cpp - the machine around the CPU (Member C: Farah Habib).
// See machine.hpp for what each function is for and how execute.cpp uses it.
#include "machine.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include "decoder.hpp"

// ============================================================ state
uint8_t  rom[ROM_SIZE], ram[RAM_SIZE];
uint32_t r[16];
int      N, Z, C, V;
uint64_t instret, cycles;
std::vector<std::pair<uint32_t, uint32_t>> text_ranges;

static int      branch_penalty = 2;          // 2 on the M0, 1 on the M0+
static uint32_t rand_state     = 2463534242u;
static FILE    *trace_file     = nullptr;
static bool     tracing        = false;      // switched by TRACE_ON / TRACE_OFF

// Filled in while one instruction runs, used for its trace line and cycle count.
struct MemWrite { uint32_t addr, value; int size; };
static uint32_t              cur_pc;         // address of the instruction running now
static std::string           encoding;       // "2005", or "f000f803" for BL
static std::vector<MemWrite> mem_writes;     // stores made by this instruction
static int                   extra_cycles;   // data accesses and branch penalties

// Thrown by the EXIT service. step() catches it so EXIT still gets its trace line.
struct ExitRequest { int status; };

static const char *REG_NAMES[16] = {"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8",
                                    "r9", "r10", "r11", "r12", "sp", "lr", "pc"};

void set_options(uint32_t seed, bool m0plus)
{
    rand_state = seed;
    branch_penalty = m0plus ? 1 : 2;
}

// ============================================================ memory
// Pointer to the byte at addr, or nullptr if addr is in neither ROM nor RAM.
static uint8_t *mem_ptr(uint32_t addr)
{
    if (addr - ROM_BASE < ROM_SIZE) return &rom[addr - ROM_BASE];
    if (addr - RAM_BASE < RAM_SIZE) return &ram[addr - RAM_BASE];
    return nullptr;
}

static bool in_ram(uint32_t addr) { return addr - RAM_BASE < RAM_SIZE; }

[[noreturn]] void fault(const char *kind) { throw Fault{kind, cur_pc}; }

// The checks every data access goes through, in the handout's order:
// alignment FIRST, so misaligned is UNALIGNED even if it is also unmapped or a ROM store.
static uint8_t *check(uint32_t addr, int size, bool write)
{
    if (addr % size) fault("UNALIGNED");
    uint8_t *p = mem_ptr(addr);
    if (!p) fault("BUS");                        // unmapped
    if (write && !in_ram(addr)) fault("BUS");    // a store to ROM
    return p;
}

// Little endian: the byte at the lowest address is the lowest byte of the value.
static uint32_t raw_read(uint32_t addr, int size)
{
    uint8_t *p = check(addr, size, false);
    uint32_t v = 0;
    for (int i = size - 1; i >= 0; i--) v = v << 8 | p[i];
    return v;
}

uint32_t mem_read(uint32_t addr, int size)
{
    uint32_t v = raw_read(addr, size);
    extra_cycles++;                              // a data access costs one cycle
    return v;
}

void mem_write(uint32_t addr, uint32_t value, int size)
{
    uint8_t *p = check(addr, size, true);
    if (size < 4) value &= (1u << (8 * size)) - 1;   // keep only the stored bytes
    for (int i = 0; i < size; i++) p[i] = value >> (8 * i);
    extra_cycles++;
    mem_writes.push_back({addr, value, size});   // for the trace line
}

uint16_t fetch16(uint32_t addr)
{
    // A failed fetch reports the address it tried to fetch, not cur_pc.
    if (addr % 2) throw Fault{"UNALIGNED", addr};
    uint8_t *p = mem_ptr(addr);
    if (!p || !mem_ptr(addr + 1)) throw Fault{"BUS", addr};
    uint16_t hw = p[0] | p[1] << 8;
    char hex[8];
    snprintf(hex, sizeof hex, "%04x", hw);
    encoding += hex;
    return hw;
}

// Used by the disassembler (decoder.cpp): never faults, unmapped bytes read as 0.
uint16_t read16(uint32_t addr)
{
    uint8_t *lo = mem_ptr(addr), *hi = mem_ptr(addr + 1);
    return (lo ? *lo : 0) | (hi ? *hi : 0) << 8;
}

void set_reg(int n, uint32_t value)
{
    if (n == 13) value &= ~3u;                   // sp is always word aligned on the M0
    r[n] = value;
}

void taken_branch() { extra_cycles += branch_penalty; }
void bl_extra()     { extra_cycles += 1; }

// ============================================================ loader
[[noreturn]] static void load_error(const char *msg, unsigned long n)
{
    fprintf(stderr, "load error: %s %lu\n", msg, n);
    exit(253);
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Remember [start, end) as .text, joining it to the previous range if they touch.
static void add_text(uint32_t start, uint32_t end)
{
    if (!text_ranges.empty() && text_ranges.back().second == start)
        text_ranges.back().second = end;
    else
        text_ranges.push_back({start, end});
}

// Load a `readelf -x` dump (or a raw .bin image at address 0). Data lines look like
//   "  0x00000100 094811df 0a2000f0 03f812df 002000df .H... ......"
// an address, then up to 4 groups of hex bytes in the next 35 columns, then ASCII.
// "Hex dump of section '.text':" tells us which section the next lines belong to.
void load_program(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); exit(253); }

    size_t len = strlen(path);
    if (len > 4 && strcmp(path + len - 4, ".bin") == 0) {      // raw binary
        size_t n = fread(rom, 1, ROM_SIZE, f);
        if (fgetc(f) != EOF) load_error("binary larger than ROM, bytes >", ROM_SIZE);
        fclose(f);
        add_text(0, n);
        return;
    }

    std::string section;
    char buf[1024];
    unsigned long lineno = 0;
    while (fgets(buf, sizeof buf, f)) {
        lineno++;
        std::string s = buf;
        // strip spaces and the newline at both ends
        size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) continue;
        s = s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);

        if (s.rfind("Hex dump of section", 0) == 0) {
            size_t q1 = s.find('\''), q2 = s.find('\'', q1 + 1);
            section = (q1 != std::string::npos && q2 != std::string::npos)
                          ? s.substr(q1 + 1, q2 - q1 - 1) : "";
            continue;
        }
        if (s.rfind("0x", 0) != 0) continue;    // headers, notes, blank lines

        // the address: 8 hex digits after "0x"
        if (s.size() < 10) load_error("bad line", lineno);
        uint32_t addr = 0;
        for (int i = 2; i < 10; i++) {
            int h = hexval(s[i]);
            if (h < 0) load_error("bad line", lineno);
            addr = addr << 4 | h;
        }
        // the bytes: columns 11..45, spaces between groups are ignored
        std::string hex;
        for (size_t i = 11; i < 46 && i < s.size(); i++)
            if (s[i] != ' ') hex += s[i];
        if (hex.size() % 2) load_error("bad line", lineno);
        std::vector<uint8_t> bytes;
        for (size_t i = 0; i < hex.size(); i += 2) {
            int hi = hexval(hex[i]), lo = hexval(hex[i + 1]);
            if (hi < 0 || lo < 0) load_error("bad line", lineno);
            bytes.push_back(hi << 4 | lo);
        }

        if (section == ".text" && !bytes.empty()) add_text(addr, addr + bytes.size());
        for (uint8_t b : bytes) {
            uint8_t *p = mem_ptr(addr);
            if (!p) load_error("byte outside ROM and RAM, line", lineno);
            *p = b;
            addr++;
        }
    }
    fclose(f);
}

// ============================================================ reset
// sp = word at 0 (bits 1:0 cleared), pc = word at 4 (bit 0 must be 1 = Thumb),
// lr = 0xFFFFFFFF, every other register and flag 0.
static void reset()
{
    cur_pc = 0;
    uint32_t sp = raw_read(0, 4), vector = raw_read(4, 4);
    memset(r, 0, sizeof r);
    N = Z = C = V = 0;
    set_reg(13, sp);
    r[14] = 0xFFFFFFFF;
    if (!(vector & 1)) throw Fault{"INVSTATE", vector};   // report the original value
    r[15] = vector & ~1u;
}

// ============================================================ trace
bool open_trace(const char *path)
{
    trace_file = fopen(path, "w");
    tracing = trace_file != nullptr;
    return tracing;
}

void close_trace()
{
    if (trace_file) fclose(trace_file);
    trace_file = nullptr;
}

// "pc encoding changed-registers memory-writes NZCV=xxxx"
static void write_trace_line(uint32_t pc, const uint32_t before[15])
{
    fprintf(trace_file, "%08x %s", pc, encoding.c_str());
    for (int i = 0; i < 15; i++)                 // r0..r12, sp, lr (pc is left out)
        if (r[i] != before[i]) fprintf(trace_file, " %s=%08x", REG_NAMES[i], r[i]);
    std::stable_sort(mem_writes.begin(), mem_writes.end(),
                     [](const MemWrite &a, const MemWrite &b) { return a.addr < b.addr; });
    for (const MemWrite &w : mem_writes)         // 2, 4 or 8 hex digits for 1, 2, 4 bytes
        fprintf(trace_file, " [%08x]=%0*x", w.addr, 2 * w.size, w.value);
    fprintf(trace_file, " NZCV=%d%d%d%d\n", N, Z, C, V);
}

// ============================================================ fetch loop
// Run one instruction. Only if it finishes (no Fault thrown) does it "retire":
// count it, add its cycles and write its trace line.
static void step()
{
    uint32_t pc = r[15];
    cur_pc = pc;
    encoding.clear();
    mem_writes.clear();
    extra_cycles = 0;
    uint32_t before[15];
    memcpy(before, r, sizeof before);

    bool exiting = false;
    int status = 0;
    try {
        execute();                               // members A and B
    } catch (ExitRequest &e) {                   // EXIT: finish this line first
        exiting = true;
        status = e.status;
    }
    instret++;
    cycles += 1 + extra_cycles;
    if (trace_file && tracing) write_trace_line(pc, before);
    if (exiting) throw ExitRequest{status};
}

int run(uint64_t max_instr)
{
    try {
        reset();
        while (true) {
            if (instret >= max_instr) {
                fflush(stdout);
                fprintf(stderr, "LIMIT at pc=0x%08x\n", r[15]);
                return 254;
            }
            step();
        }
    } catch (ExitRequest &e) {
        return e.status;
    } catch (Fault &f) {
        fflush(stdout);
        fprintf(stderr, "FAULT %s at pc=0x%08x\n", f.kind, f.pc);
        return 255;
    }
}

// ============================================================ SVC services
// stdin is read all at once on first use. GETCHAR, GETINT and READ share it.
static std::vector<uint8_t> in_data;
static size_t in_pos = 0;
static bool   in_loaded = false;

static void load_stdin()
{
    if (in_loaded) return;
    in_loaded = true;
    uint8_t chunk[4096];
    size_t n;
    while ((n = fread(chunk, 1, sizeof chunk, stdin)) > 0)
        in_data.insert(in_data.end(), chunk, chunk + n);
}

static int peek_byte()                           // next byte, or -1 at end of input
{
    load_stdin();
    return in_pos < in_data.size() ? in_data[in_pos] : -1;
}

static int get_byte()
{
    int b = peek_byte();
    if (b >= 0) in_pos++;
    return b;
}

// Skip white space, then an optional sign and decimal digits.
// No digits: return 0 and put back everything after the white space (sign included).
static int32_t get_int()
{
    while (true) {
        int c = peek_byte();
        if (c == ' ' || (c >= 9 && c <= 13)) in_pos++;   // space, \t \n \v \f \r
        else break;
    }
    size_t start = in_pos;
    bool negative = false;
    if (peek_byte() == '+' || peek_byte() == '-') {
        negative = peek_byte() == '-';
        in_pos++;
    }
    uint32_t value = 0;                          // wraps like the 32-bit register
    int digits = 0;
    while (peek_byte() >= '0' && peek_byte() <= '9') {
        value = value * 10 + (get_byte() - '0');
        digits++;
    }
    if (digits == 0) { in_pos = start; return 0; }
    return negative ? (int32_t)(0u - value) : (int32_t)value;
}

// True if all of [addr, addr + count) is inside ONE region (and in RAM, for a write).
static bool range_ok(uint32_t addr, uint32_t count, bool write)
{
    if (count == 0) return true;
    uint64_t end = (uint64_t)addr + count - 1;
    if (end > 0xFFFFFFFFu) return false;
    bool rom_ok = addr - ROM_BASE < ROM_SIZE && end - ROM_BASE < ROM_SIZE;
    bool ram_ok = in_ram(addr) && end - RAM_BASE < RAM_SIZE;
    return ram_ok || (rom_ok && !write);
}

static void out(const void *data, size_t n, int fd)
{
    if (fd == 1) fwrite(data, 1, n, stdout);
    else { fwrite(data, 1, n, stderr); fflush(stderr); }
}

static void put64(uint64_t v)                    // 64-bit result: low in r0, high in r1
{
    r[0] = (uint32_t)v;
    r[1] = (uint32_t)(v >> 32);
}

// svc #n is the M0's "ecall". The number is in the instruction; arguments are in
// r0..r3, results in r0 (and r1). Service memory accesses don't cost cycles.
void svc(int n)
{
    switch (n) {
    case 0x00:                                               // EXIT
        throw ExitRequest{(int)(r[0] & 0xFF)};
    case 0x10:                                               // PUTCHAR
        putchar(r[0] & 0xFF);
        break;
    case 0x11: {                                             // PUTS
        std::string s;
        for (uint32_t a = r[0]; ; a++) {
            uint8_t *p = mem_ptr(a);
            if (!p) fault("BUS");                            // string left mapped memory
            if (*p == 0) break;
            s += (char)*p;
        }
        out(s.data(), s.size(), 1);
        break;
    }
    case 0x12:                                               // PUTINT
        printf("%d", (int32_t)r[0]);
        break;
    case 0x13:                                               // PUTHEX
        printf("0x%08X", r[0]);
        break;
    case 0x14:                                               // GETCHAR (-1 at end)
        r[0] = (uint32_t)get_byte();
        break;
    case 0x15:                                               // GETINT
        r[0] = (uint32_t)get_int();
        break;
    case 0x22: {                                             // READ(fd, buf, count)
        uint32_t fd = r[0], buf = r[1], count = r[2];
        if (fd != 0)      { r[0] = (uint32_t)-9; break; }
        if (count == 0)   { r[0] = 0; break; }
        load_stdin();
        size_t avail = in_data.size() - in_pos;
        uint32_t got = count < avail ? count : (uint32_t)avail;
        if (got > 0) {                                       // at EOF the buffer isn't touched
            if (!range_ok(buf, got, true)) fault("BUS");
            memcpy(mem_ptr(buf), &in_data[in_pos], got);     // not listed in the trace
            in_pos += got;
        }
        r[0] = got;
        break;
    }
    case 0x23: {                                             // WRITE(fd, buf, count)
        uint32_t fd = r[0], buf = r[1], count = r[2];
        if (fd != 1 && fd != 2) { r[0] = (uint32_t)-9; break; }
        if (count == 0)         { r[0] = 0; break; }
        if (!range_ok(buf, count, false)) fault("BUS");
        if (fd == 2) fflush(stdout);
        out(mem_ptr(buf), count, fd);
        r[0] = count;
        break;
    }
    case 0x30:                                               // INSTRET, counting this SVC
        put64(instret + 1);
        break;
    case 0x31:                                               // CYCLES, this SVC = 1 cycle
        put64(cycles + 1);
        break;
    case 0x32: {                                             // RAND: xorshift32
        uint32_t x = rand_state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        rand_state = x;
        r[0] = x;
        break;
    }
    case 0x40: {                                             // DUMP_REGS (stderr)
        fflush(stdout);
        for (int i = 0; i < 16; i++)
            fprintf(stderr, "%s%s=%08x", i ? " " : "", REG_NAMES[i], i == 15 ? cur_pc : r[i]);
        fprintf(stderr, " NZCV=%d%d%d%d\n", N, Z, C, V);
        fflush(stderr);
        break;
    }
    case 0x41: {                                             // DUMP_MEM(addr, count) (stderr)
        uint32_t addr = r[0], count = r[1];
        if (!range_ok(addr, count, false)) fault("BUS");
        fflush(stdout);
        for (uint32_t i = 0; i < count; i += 16) {           // 16 bytes per row
            fprintf(stderr, "%08x:", addr + i);
            for (uint32_t j = i; j < count && j < i + 16; j++)
                fprintf(stderr, " %02x", *mem_ptr(addr + j));
            fprintf(stderr, "\n");
        }
        fflush(stderr);
        break;
    }
    case 0x42: tracing = true;  break;                       // TRACE_ON (own line included)
    case 0x43: tracing = false; break;                       // TRACE_OFF (own line excluded)
    default:   fault("UNDEFINED");                           // no such service
    }
}
