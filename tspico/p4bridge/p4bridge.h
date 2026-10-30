// p4bridge: the TS-Pico's ports 0Eh/0Fh between the emulator (core 0) and
// the TS-Pico firmware in MicroPython (core 1), inside one ESP32-P4 image.
//
// The model is BusModel from tspico-firmware-build tools/emu/pico_host.py,
// which models PIO0 SM0 (TS_IO_DUAL) on the RP2040. Frame ops are bridge v1
// (docs/EMULATOR_BRIDGE.md). See docs/DECISIONS.md D4.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define P4B_BRIDGE_VERSION 1

enum {
    P4B_OUT_DATA = 0,       // OUT (0Eh),A   data to the Pico     reply ignored
    P4B_IN_DATA = 1,        // IN A,(0Eh)    data from the Pico   reply = the byte
    P4B_IN_STATUS = 2,      // IN A,(0Fh)    the status byte      reply = the byte
    P4B_OUT_STATUS = 3,     // OUT (0Fh),A   SYNC / BREAK         reply ignored
    P4B_HELLO = 4,          // HELLO         version 1            reply = 1
};

// Create the queues. Safe to call more than once; false if out of memory.
bool p4bridge_init(void);

// One Z80 access, from the emulator's side. Callable from any task except
// one holding the MicroPython GIL. P4B_IN_DATA blocks for up to 50 ms
// waiting for the firmware to queue a byte, then reads 00h.
uint8_t p4bridge_frame(uint8_t op, uint8_t value);

typedef struct {
    uint32_t frames;        // every p4bridge_frame call
    uint32_t underruns;     // IN (0Eh) with nothing queued after 50 ms: read 00h
    uint32_t rx_overruns;   // Z80 writes dropped because the RX queue was full
} p4bridge_stats_t;

void p4bridge_get_stats(p4bridge_stats_t *out);

// The firmware is blocked in MQ.get() or an empty MQ.rx_fifo(), waiting for
// the Z80 to write, and that wait began after it took every earlier write:
// it has finished whatever it was doing.
bool p4bridge_firmware_waiting(void);

// Z80 writes queued that the firmware hasn't taken yet.
unsigned p4bridge_rx_waiting(void);
