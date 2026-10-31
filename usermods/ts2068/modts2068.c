// ts2068: the emulated TS2068 (machine/) as a MicroPython module, for now
// only to time it on the P4 (PLAN.md Phase 2, "P4 benchmark").
//
//   import ts2068
//   ts2068.bench(home_rom, exrom)    # starts on core 0, returns at once
//   ts2068.status()                  # -> dict; state "done" when finished
//
// The benchmark boots the ROM, types the fixed BASIC loop
// (test/machine/basic_loop.c), and times every frame with nothing else
// running: no display, no audio, no TS-Pico. Frames from `start` to the
// end are the measured window; the BASIC loop runs through all of it.
// At the end it reports an FNV-1a hash of the 48K of HOME RAM, which a
// host run of the same frames (run_ts2068 --bench --ram) must match.

#include <string.h>

#include "esp_memory_utils.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"

#include "py/runtime.h"

#include "basic_loop.h"
#include "machine.h"
#include "z80_core.h"

#define BENCH_CORE 0
#define BENCH_PRIORITY 5
#define BENCH_STACK 4096
#define FRAME_BUDGET_US 16635           // one 60.1145 Hz frame
#define HIST_BUCKETS 20                 // 1 ms each; the last takes the rest

static struct {
    volatile enum { IDLE, RUNNING, DONE } state;
    volatile uint32_t frame;
    uint32_t frames, start;
    uint64_t boot_us, window_us;        // frames before `start`, and from it
    uint32_t min_us, max_us, over_budget;
    uint32_t hist[HIST_BUCKETS];
    uint32_t ram_hash;
    ts2068_state_t cpu;
} b;

static uint32_t fnv1a(const uint8_t *p, size_t n) {
    uint32_t h = 2166136261u;
    while (n--) {
        h = (h ^ *p++) * 16777619u;
    }
    return h;
}

static void bench_task(void *arg) {
    (void)arg;
    for (uint32_t i = 0; i < b.frames; i++) {
        basic_loop_frame(i);
        int64_t t0 = esp_timer_get_time();
        ts2068_run_frame();
        uint32_t dt = esp_timer_get_time() - t0;
        if (i < b.start) {
            b.boot_us += dt;
        } else {
            b.window_us += dt;
            if (dt < b.min_us) {
                b.min_us = dt;
            }
            if (dt > b.max_us) {
                b.max_us = dt;
            }
            if (dt > FRAME_BUDGET_US) {
                b.over_budget++;
            }
            uint32_t k = dt / 1000;
            b.hist[k < HIST_BUCKETS ? k : HIST_BUCKETS - 1]++;
        }
        b.frame = i + 1;
    }
    b.ram_hash = fnv1a(ts2068_home_ram(), 0xc000);
    ts2068_get_state(&b.cpu);
    b.state = DONE;
    vTaskDelete(NULL);
}

// bench(home_rom, exrom, frames=2800, start=300)
static mp_obj_t bench(size_t n_args, const mp_obj_t *args) {
    if (b.state == RUNNING) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("already running"));
    }
    mp_buffer_info_t home, ex;
    mp_get_buffer_raise(args[0], &home, MP_BUFFER_READ);
    mp_get_buffer_raise(args[1], &ex, MP_BUFFER_READ);
    if (home.len != 0x4000) {
        mp_raise_ValueError(MP_ERROR_TEXT("HOME ROM: not 16K"));
    }
    ts2068_config_t cfg = {
        .home_rom = home.buf, .exrom = ex.buf, .exrom_size = ex.len,
    };
    if (ts2068_init(&cfg)) {           // copies the ROMs: the buffers may go
        mp_raise_ValueError(MP_ERROR_TEXT("EXROM: not 8K or 16K"));
    }
    memset(&b, 0, sizeof(b));
    b.frames = n_args > 2 ? mp_obj_get_int(args[2]) : 2800;
    b.start = n_args > 3 ? mp_obj_get_int(args[3]) : 300;
    b.min_us = UINT32_MAX;
    b.state = RUNNING;
    if (xTaskCreatePinnedToCore(bench_task, "ts2068", BENCH_STACK, NULL,
        BENCH_PRIORITY, NULL, BENCH_CORE) != pdPASS) {
        b.state = IDLE;
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("bench task"));
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(bench_obj, 2, 4, bench);

static void store(mp_obj_t d, qstr k, mp_obj_t v) {
    mp_obj_dict_store(d, MP_OBJ_NEW_QSTR(k), v);
}

// status() -> dict: state, frame, frames, start; boot_us and window_us
// (time spent in frames before and from `start`), min_us, max_us,
// over_budget, hist (1 ms buckets); when done, ram_hash, pc, frame_tstate.
// ram_internal: whether the emulated RAM is in the P4's internal memory.
// z80: which Z80 core this image has ("fuse" or "jls").
static mp_obj_t status(void) {
    static const qstr states[] = { MP_QSTR_idle, MP_QSTR_running, MP_QSTR_done };
    mp_obj_t d = mp_obj_new_dict(0);
    store(d, MP_QSTR_state, MP_OBJ_NEW_QSTR(states[b.state]));
    store(d, MP_QSTR_z80, mp_obj_new_str(z80_core_name, strlen(z80_core_name)));
    store(d, MP_QSTR_frame, mp_obj_new_int_from_uint(b.frame));
    store(d, MP_QSTR_frames, mp_obj_new_int_from_uint(b.frames));
    store(d, MP_QSTR_start, mp_obj_new_int_from_uint(b.start));
    store(d, MP_QSTR_boot_us, mp_obj_new_int_from_ull(b.boot_us));
    store(d, MP_QSTR_window_us, mp_obj_new_int_from_ull(b.window_us));
    store(d, MP_QSTR_min_us, mp_obj_new_int_from_uint(b.min_us));
    store(d, MP_QSTR_max_us, mp_obj_new_int_from_uint(b.max_us));
    store(d, MP_QSTR_over_budget, mp_obj_new_int_from_uint(b.over_budget));
    mp_obj_t hist[HIST_BUCKETS];
    for (int i = 0; i < HIST_BUCKETS; i++) {
        hist[i] = mp_obj_new_int_from_uint(b.hist[i]);
    }
    store(d, MP_QSTR_hist, mp_obj_new_list(HIST_BUCKETS, hist));
    store(d, MP_QSTR_ram_internal, mp_obj_new_bool(esp_ptr_internal(ts2068_home_ram())));
    if (b.state == DONE) {
        store(d, MP_QSTR_ram_hash, mp_obj_new_int_from_uint(b.ram_hash));
        store(d, MP_QSTR_pc, MP_OBJ_NEW_SMALL_INT(b.cpu.pc));
        store(d, MP_QSTR_frame_tstate, mp_obj_new_int_from_uint(b.cpu.frame_tstate));
    }
    return d;
}
static MP_DEFINE_CONST_FUN_OBJ_0(status_obj, status);

static const mp_rom_map_elem_t ts2068_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_ts2068) },
    { MP_ROM_QSTR(MP_QSTR_bench), MP_ROM_PTR(&bench_obj) },
    { MP_ROM_QSTR(MP_QSTR_status), MP_ROM_PTR(&status_obj) },
};
static MP_DEFINE_CONST_DICT(ts2068_globals, ts2068_globals_table);

const mp_obj_module_t ts2068_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&ts2068_globals,
};

MP_REGISTER_MODULE(MP_QSTR_ts2068, ts2068_module);
