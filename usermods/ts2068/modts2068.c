// ts2068: the emulated TS2068 (machine/) as a MicroPython module: the
// Phase 2 benchmark, and the machine running in real time on the panel.
//
//   import ts2068
//   ts2068.bench(home_rom, exrom)    # starts on core 0, returns at once
//   ts2068.status()                  # -> dict; state "done" when finished
//
//   ts2068.run(home_rom, exrom)      # real time on core 0, drawn on the panel
//   ts2068.run_status()              # -> dict of frame timings
//   ts2068.stop()
//   ts2068.display_check()           # -> pixels differing from the machine
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

#include "py/mphal.h"

#include "py/runtime.h"

#include "basic_loop.h"
#include "machine.h"
#include "p4_display.h"
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

// run(): see below
static struct {
    volatile bool running, stop;
    bool demo;
    volatile uint32_t frame;
    uint32_t late;
    uint64_t emu_us, display_us;
    uint32_t emu_max, display_max, work_max;
    int64_t started;
} r;
static esp_timer_handle_t tick;
static TaskHandle_t run_handle;

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
    if (b.state == RUNNING || r.running) {
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

// run(home_rom, exrom, demo=False, redraw_all=False): the machine in real
// time on core 0,
// each frame drawn on the panel (platform/p4_display.c). An esp_timer
// wakes the task at 60.1145 Hz; a tick that comes while a frame is still
// running is counted as late. demo=True types the benchmark's BASIC loop
// once the ROM has booted. redraw_all=True converts every line of every
// frame, for measuring the display's worst case. No sound or keyboard yet.


static void tick_cb(void *arg) {
    (void)arg;
    xTaskNotifyGive(run_handle);
}

static void run_task(void *arg) {
    (void)arg;
    while (!r.stop) {
        uint32_t n = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
        if (n == 0) {
            continue;
        }
        r.late += n - 1;
        if (r.demo) {
            basic_loop_frame(r.frame);
        }
        int64_t t0 = esp_timer_get_time();
        ts2068_run_frame();
        int64_t t1 = esp_timer_get_time();
        p4_display_ts2068(ts2068_render_line);
        int64_t t2 = esp_timer_get_time();
        uint32_t emu = t1 - t0, display = t2 - t1;
        r.emu_us += emu;
        r.display_us += display;
        if (emu > r.emu_max) {
            r.emu_max = emu;
        }
        if (display > r.display_max) {
            r.display_max = display;
        }
        if (t2 - t0 > r.work_max) {
            r.work_max = t2 - t0;
        }
        r.frame++;
    }
    esp_timer_stop(tick);
    r.running = false;
    vTaskDelete(NULL);
}

static mp_obj_t run(size_t n_args, const mp_obj_t *args) {
    if (r.running || b.state == RUNNING) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("already running"));
    }
    mp_buffer_info_t home, ex;
    mp_get_buffer_raise(args[0], &home, MP_BUFFER_READ);
    mp_get_buffer_raise(args[1], &ex, MP_BUFFER_READ);
    if (home.len != 0x4000) {
        mp_raise_ValueError(MP_ERROR_TEXT("HOME ROM: not 16K"));
    }
    esp_err_t err = p4_display_init();
    if (err != ESP_OK) {
        mp_raise_msg_varg(&mp_type_OSError, MP_ERROR_TEXT("display: %s"),
            esp_err_to_name(err));
    }
    if (!tick) {
        const esp_timer_create_args_t targs = { .callback = tick_cb, .name = "ts2068" };
        if (esp_timer_create(&targs, &tick) != ESP_OK) {
            mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("timer"));
        }
    }
    ts2068_config_t cfg = {
        .home_rom = home.buf, .exrom = ex.buf, .exrom_size = ex.len,
    };
    if (ts2068_init(&cfg)) {
        mp_raise_ValueError(MP_ERROR_TEXT("EXROM: not 8K or 16K"));
    }
    memset(&r, 0, sizeof(r));
    r.demo = n_args > 2 && mp_obj_is_true(args[2]);
    p4_display_redraw_all(n_args > 3 && mp_obj_is_true(args[3]));
    r.running = true;
    if (xTaskCreatePinnedToCore(run_task, "ts2068", BENCH_STACK, NULL,
        BENCH_PRIORITY, &run_handle, BENCH_CORE) != pdPASS) {
        r.running = false;
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("run task"));
    }
    r.started = esp_timer_get_time();
    esp_timer_start_periodic(tick, FRAME_BUDGET_US);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(run_obj, 2, 4, run);

static mp_obj_t stop(void) {
    if (r.running) {
        r.stop = true;
        while (r.running) {
            mp_hal_delay_ms(10);
        }
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(stop_obj, stop);

// display_check() -> pixels on the panel that differ from the machine's
// screen; call after stop()
static mp_obj_t display_check(void) {
    if (r.running) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("running"));
    }
    return MP_OBJ_NEW_SMALL_INT(p4_display_check(ts2068_render_line));
}
static MP_DEFINE_CONST_FUN_OBJ_0(display_check_obj, display_check);

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

// run_status() -> dict: running, frame, elapsed_us (since run), late
// (ticks that found the last frame still running); per frame, the mean
// and max time in us to run the machine (emu_) and to render and draw its
// screen (display_), and work_max for both; the
// panel's refreshes; frames drawn, unchanged (nothing to draw) and
// replaced (drawn over before the panel showed them); TS2068 lines
// converted in all, and the mean and max time of a frame that was drawn.
static mp_obj_t run_status(void) {
    mp_obj_t d = mp_obj_new_dict(0);
    uint32_t n = r.frame ? r.frame : 1;
    p4_display_stats_t s;
    p4_display_get_stats(&s);
    store(d, MP_QSTR_running, mp_obj_new_bool(r.running));
    store(d, MP_QSTR_frame, mp_obj_new_int_from_uint(r.frame));
    store(d, MP_QSTR_elapsed_us, mp_obj_new_int_from_ll(
        r.started ? esp_timer_get_time() - r.started : 0));
    store(d, MP_QSTR_late, mp_obj_new_int_from_uint(r.late));
    store(d, MP_QSTR_emu_us, mp_obj_new_int_from_uint(r.emu_us / n));
    store(d, MP_QSTR_emu_max, mp_obj_new_int_from_uint(r.emu_max));
    store(d, MP_QSTR_display_us, mp_obj_new_int_from_uint(r.display_us / n));
    store(d, MP_QSTR_display_max, mp_obj_new_int_from_uint(r.display_max));
    store(d, MP_QSTR_work_max, mp_obj_new_int_from_uint(r.work_max));
    store(d, MP_QSTR_refreshes, mp_obj_new_int_from_uint(s.refreshes));
    store(d, MP_QSTR_drawn, mp_obj_new_int_from_uint(s.drawn));
    store(d, MP_QSTR_unchanged, mp_obj_new_int_from_uint(s.unchanged));
    store(d, MP_QSTR_replaced, mp_obj_new_int_from_uint(s.replaced));
    store(d, MP_QSTR_lines, mp_obj_new_int_from_ull(s.lines));
    store(d, MP_QSTR_strips, mp_obj_new_int_from_uint(s.strips));
    store(d, MP_QSTR_draw_us, mp_obj_new_int_from_uint(s.drawn ? s.draw_us_total / s.drawn : 0));
    store(d, MP_QSTR_draw_max, mp_obj_new_int_from_uint(s.draw_us_max));
    store(d, MP_QSTR_flip_max, mp_obj_new_int_from_uint(s.flip_us_max));
    return d;
}
static MP_DEFINE_CONST_FUN_OBJ_0(run_status_obj, run_status);

static const mp_rom_map_elem_t ts2068_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_ts2068) },
    { MP_ROM_QSTR(MP_QSTR_bench), MP_ROM_PTR(&bench_obj) },
    { MP_ROM_QSTR(MP_QSTR_status), MP_ROM_PTR(&status_obj) },
    { MP_ROM_QSTR(MP_QSTR_run), MP_ROM_PTR(&run_obj) },
    { MP_ROM_QSTR(MP_QSTR_run_status), MP_ROM_PTR(&run_status_obj) },
    { MP_ROM_QSTR(MP_QSTR_stop), MP_ROM_PTR(&stop_obj) },
    { MP_ROM_QSTR(MP_QSTR_display_check), MP_ROM_PTR(&display_check_obj) },
};
static MP_DEFINE_CONST_DICT(ts2068_globals, ts2068_globals_table);

const mp_obj_module_t ts2068_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&ts2068_globals,
};

MP_REGISTER_MODULE(MP_QSTR_ts2068, ts2068_module);
