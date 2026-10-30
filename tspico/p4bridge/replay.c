// Trace replay: the emulator's side of a recorded session, run on core 0
// through p4bridge_frame(), against the real firmware on core 1. Gate 1 of
// PLAN.md Phase 1. Traces come from test/traces/*.frames, packed by
// tools/p4/pack_frames.py as 3 bytes a frame: op, value, recorded reply.
//
// pico_host logs a status read only when the status changes, so an
// IN (0Fh) line means "poll until the status reads this", the way the ROM's
// polling loop would; other lines are one access each. Replies are checked
// for IN (0Eh) and HELLO; writes have no reply to check.

#include <string.h>

#include "esp_heap_caps.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"

#include "py/runtime.h"

#include "p4bridge.h"

#define REPLAY_CORE 0
#define REPLAY_PRIORITY 5
#define REPLAY_STACK 4096
#define STATUS_TIMEOUT_US 10000000      // a status that never comes: 10 s
#define MAX_BAD 8

enum { IDLE, RUNNING, DONE };

typedef struct {
    uint32_t index;
    uint8_t op, value, expected, got;
} bad_t;

static struct {
    volatile int state;
    uint8_t *buf;
    uint32_t n;                     // frames in the trace
    volatile uint32_t index;        // frames done
    uint32_t mismatches;
    uint32_t status_timeouts;
    uint32_t status_polls;
    uint32_t in_data;               // IN (0Eh) frames
    uint64_t in_data_us;            // their total time
    uint32_t in_data_max_us;
    int64_t t_start, t_end;
    p4bridge_stats_t before;
    uint32_t nbad;
    bad_t bad[MAX_BAD];
} r;

static void record_bad(uint32_t i, uint8_t op, uint8_t value, uint8_t expected, uint8_t got) {
    r.mismatches++;
    if (r.nbad < MAX_BAD) {
        r.bad[r.nbad++] = (bad_t){ i, op, value, expected, got };
    }
}

static void replay_task(void *arg) {
    r.t_start = esp_timer_get_time();
    for (uint32_t i = 0; i < r.n; i++) {
        uint8_t op = r.buf[3 * i], value = r.buf[3 * i + 1], expected = r.buf[3 * i + 2];
        if (op == P4B_IN_STATUS) {
            int64_t deadline = esp_timer_get_time() + STATUS_TIMEOUT_US;
            uint8_t got;
            for (;;) {
                got = p4bridge_frame(op, 0);
                r.status_polls++;
                if (got == expected) {
                    break;
                }
                if (esp_timer_get_time() > deadline) {
                    r.status_timeouts++;
                    record_bad(i, op, value, expected, got);
                    break;
                }
                esp_rom_delay_us(2);    // about a Z80 polling loop's pace
            }
        } else {
            int64_t t0 = esp_timer_get_time();
            uint8_t got = p4bridge_frame(op, value);
            if (op == P4B_IN_DATA) {
                uint32_t dt = esp_timer_get_time() - t0;
                r.in_data++;
                r.in_data_us += dt;
                if (dt > r.in_data_max_us) {
                    r.in_data_max_us = dt;
                }
            }
            if ((op == P4B_IN_DATA || op == P4B_HELLO) && got != expected) {
                record_bad(i, op, value, expected, got);
            }
        }
        r.index = i + 1;
    }
    r.t_end = esp_timer_get_time();
    heap_caps_free(r.buf);
    r.buf = NULL;
    r.state = DONE;
    vTaskDelete(NULL);
}

// replay(frames: bytes): start replaying on core 0. Returns at once.
static mp_obj_t replay_start(mp_obj_t frames_in) {
    if (r.state == RUNNING) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("replay running"));
    }
    if (!p4bridge_init()) {
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("p4bridge queues"));
    }
    mp_buffer_info_t b;
    mp_get_buffer_raise(frames_in, &b, MP_BUFFER_READ);
    if (b.len % 3) {
        mp_raise_ValueError(MP_ERROR_TEXT("frames: 3 bytes each"));
    }
    memset(&r, 0, sizeof(r));
    r.buf = heap_caps_malloc(b.len ? b.len : 1, MALLOC_CAP_8BIT);   // outside the GC heap
    if (r.buf == NULL) {
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("replay buffer"));
    }
    memcpy(r.buf, b.buf, b.len);
    r.n = b.len / 3;
    p4bridge_get_stats(&r.before);
    r.state = RUNNING;
    if (xTaskCreatePinnedToCore(replay_task, "p4b_replay", REPLAY_STACK, NULL,
        REPLAY_PRIORITY, NULL, REPLAY_CORE) != pdPASS) {
        heap_caps_free(r.buf);
        r.buf = NULL;
        r.state = IDLE;
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("replay task"));
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(p4b_replay_start_obj, replay_start);

static mp_obj_t new_dict_item(mp_obj_t d, qstr k, mp_obj_t v) {
    mp_obj_dict_store(d, MP_OBJ_NEW_QSTR(k), v);
    return d;
}

// replay_status() -> dict: state ("idle", "running", "done"), progress,
// mismatches with the first few, timing, and bridge counters for the run.
static mp_obj_t replay_status(void) {
    mp_obj_t d = mp_obj_new_dict(0);
    static const qstr states[] = { MP_QSTR_idle, MP_QSTR_running, MP_QSTR_done };
    new_dict_item(d, MP_QSTR_state, MP_OBJ_NEW_QSTR(states[r.state]));
    new_dict_item(d, MP_QSTR_index, mp_obj_new_int_from_uint(r.index));
    new_dict_item(d, MP_QSTR_total, mp_obj_new_int_from_uint(r.n));
    new_dict_item(d, MP_QSTR_mismatches, mp_obj_new_int_from_uint(r.mismatches));
    new_dict_item(d, MP_QSTR_status_timeouts, mp_obj_new_int_from_uint(r.status_timeouts));
    new_dict_item(d, MP_QSTR_status_polls, mp_obj_new_int_from_uint(r.status_polls));
    new_dict_item(d, MP_QSTR_in_data, mp_obj_new_int_from_uint(r.in_data));
    new_dict_item(d, MP_QSTR_in_data_us, mp_obj_new_int_from_ull(r.in_data_us));
    new_dict_item(d, MP_QSTR_in_data_max_us, mp_obj_new_int_from_uint(r.in_data_max_us));
    int64_t end = r.state == DONE ? r.t_end : esp_timer_get_time();
    new_dict_item(d, MP_QSTR_elapsed_us, mp_obj_new_int_from_ll(r.state == IDLE ? 0 : end - r.t_start));
    p4bridge_stats_t now;
    p4bridge_get_stats(&now);
    new_dict_item(d, MP_QSTR_underruns, mp_obj_new_int_from_uint(now.underruns - r.before.underruns));
    new_dict_item(d, MP_QSTR_rx_overruns, mp_obj_new_int_from_uint(now.rx_overruns - r.before.rx_overruns));
    mp_obj_t bad = mp_obj_new_list(0, NULL);
    for (uint32_t i = 0; i < r.nbad; i++) {
        mp_obj_t t[5] = {
            mp_obj_new_int_from_uint(r.bad[i].index),
            MP_OBJ_NEW_SMALL_INT(r.bad[i].op),
            MP_OBJ_NEW_SMALL_INT(r.bad[i].value),
            MP_OBJ_NEW_SMALL_INT(r.bad[i].expected),
            MP_OBJ_NEW_SMALL_INT(r.bad[i].got),
        };
        mp_obj_list_append(bad, mp_obj_new_tuple(5, t));
    }
    new_dict_item(d, MP_QSTR_bad, bad);
    return d;
}
MP_DEFINE_CONST_FUN_OBJ_0(p4b_replay_status_obj, replay_status);
