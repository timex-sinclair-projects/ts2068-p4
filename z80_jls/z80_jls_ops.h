// z80_jls_ops.h: Z80_JLS's Z80Ops and Ports, on z80_glue.h.
//
// Replaces ESPectrum's z80operations.h. ESPectrum switches between
// machine-specific function pointers (_std, _2A3) and draws the screen as
// T-states pass (VIDEO::Draw); here every access goes straight to the
// machine through z80_glue.h, the same calls Fuse's core makes, so the two
// cores see the same contention and the same ports.

#ifndef Z80_JLS_OPS_H
#define Z80_JLS_OPS_H

#include <stdint.h>

extern "C" {
#include "z80_glue.h"
}

#ifdef ESP_PLATFORM
#include "esp_attr.h"
#else
#define IRAM_ATTR
#endif

class Z80Ops
{
public:
    // Opcode fetch: 4 T-states, contended at the address (Fuse: the M1
    // cycle in z80_do_opcodes and the CB/DD/ED/FD prefix fetches)
    static inline uint8_t fetchOpcode(uint16_t address) {
        contend_read(address, 4);
        return readbyte_internal(address);
    }

    static inline uint8_t peek8(uint16_t address) { return readbyte(address); }
    static inline void poke8(uint16_t address, uint8_t value) { writebyte(address, value); }

    static inline uint16_t peek16(uint16_t address) {
        uint8_t lo = readbyte(address);
        return lo | (readbyte(address + 1) << 8);
    }

    static inline void poke16(uint16_t address, RegisterPair word) {
        writebyte(address, word.byte8.lo);
        writebyte(address + 1, word.byte8.hi);
    }

    // Put an address on the bus for wstates T-states, each one contended
    static inline void addressOnBus(uint16_t address, int32_t wstates) {
        while (wstates-- > 0) {
            contend_read_no_mreq(address, 1);
        }
    }

    // T-states with nothing on the bus (ESPectrum: VIDEO::Draw(n, false))
    static inline void internal(int32_t wstates) { tstates += wstates; }

    // Ports::input and Ports::output: the whole I/O cycle
    static inline uint8_t input(uint16_t port) { return readport(port); }
    static inline void output(uint16_t port, uint8_t value) { writeport(port, value); }

    // The /INT line is low for the first z80_interrupt_length T-states of
    // the frame, unless the SCLD masks it
    static inline bool isActiveINT(void) {
        return tstates < z80_interrupt_length && !z80_int_disabled;
    }
};

#endif // Z80_JLS_OPS_H
