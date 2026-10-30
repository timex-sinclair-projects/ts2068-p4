// p4bridge: the bus model and its MicroPython face.
//
// Emulator side: p4bridge_frame(), plain C, on core 0. Firmware side: the
// p4bridge.MQ object, with the methods the firmware calls on an
// rp2.StateMachine (get, put, rx_fifo, tx_fifo, exec, active, restart),
// on core 1 under the GIL. Between them, two FreeRTOS queues and the
// status byte Y. Semantics: BusModel in pico_host.py (docs/DECISIONS.md D4).

#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "py/mpthread.h"
#include "py/runtime.h"

#include "p4bridge.h"

#define RX_DEPTH 1024           // words the Z80 wrote, not yet read by the firmware
#define TX_DEPTH 4096           // bytes the firmware queued for IN (0Eh)
#define FIFO_REPORT 4           // rx_fifo/tx_fifo report at most a PIO FIFO's depth
#define TX_WAIT_MS 50           // pico_host.py TX_WAIT
#define PORT_0F 0x100           // bit 8 of an RX word: a write to port 0Fh

static QueueHandle_t rxq;       // uint16_t: v, or PORT_0F | v
static QueueHandle_t txq;       // uint8_t
static volatile uint32_t y;     // the status byte the Z80 reads on 0Fh (low 8 bits)
static volatile uint32_t n_frames, n_underruns, n_rx_overruns;
static volatile bool fw_waiting;  // the firmware is blocked waiting for a Z80 write
// Words pushed by the Z80 side and taken by the firmware, and n_taken when the
// current wait began. fw_waiting stays true for a moment after a word wakes
// the firmware (core 1 has to be scheduled and clear it); a wait only says
// "finished with everything" if it began after every push was taken.
static volatile uint32_t n_pushed, n_taken, wait_mark;

static inline void wait_begin(void) {
    wait_mark = n_taken;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    fw_waiting = true;
}

bool p4bridge_init(void) {
    if (rxq == NULL) {
        rxq = xQueueCreate(RX_DEPTH, sizeof(uint16_t));
    }
    if (txq == NULL) {
        txq = xQueueCreate(TX_DEPTH, sizeof(uint8_t));
    }
    return rxq != NULL && txq != NULL;
}

uint8_t p4bridge_frame(uint8_t op, uint8_t value) {
    n_frames++;
    switch (op) {
        case P4B_OUT_DATA:
        case P4B_OUT_STATUS: {
            uint16_t w = op == P4B_OUT_STATUS ? (PORT_0F | value) : value;
            // Auto-busy: every Z80 write drops READY. Before the push, so the
            // firmware on core 1 can't set READY first and have it lost.
            y = 0;
            if (xQueueSend(rxq, &w, 0) == pdTRUE) {
                n_pushed++;
            } else {
                n_rx_overruns++;
            }
            return 0;
        }
        case P4B_IN_STATUS:
            return y & 0xFF;
        case P4B_HELLO:
            return P4B_BRIDGE_VERSION;
        case P4B_IN_DATA: {
            uint8_t b;
            if (xQueueReceive(txq, &b, pdMS_TO_TICKS(TX_WAIT_MS)) == pdTRUE) {
                return b;
            }
            n_underruns++;
            return 0x00;            // PIO pull(noblock) of an empty FIFO
        }
    }
    return 0;
}

void p4bridge_get_stats(p4bridge_stats_t *out) {
    out->frames = n_frames;
    out->underruns = n_underruns;
    out->rx_overruns = n_rx_overruns;
}

bool p4bridge_firmware_waiting(void) {
    if (!fw_waiting) {
        return false;
    }
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    return wait_mark == n_pushed;
}

unsigned p4bridge_rx_waiting(void) {
    return rxq ? uxQueueMessagesWaiting(rxq) : 0;
}

static void check_init(void) {
    if (!p4bridge_init()) {
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("p4bridge queues"));
    }
}

// ---- p4bridge.MQ: the firmware's side ----------------------------------------

typedef struct {
    mp_obj_base_t base;
} mq_obj_t;

static const mp_obj_type_t mq_type;
static const mq_obj_t mq_singleton = {{&mq_type}};

static mp_obj_t mq_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    check_init();
    return MP_OBJ_FROM_PTR(&mq_singleton);
}

// The next word the Z80 wrote; blocks, with the GIL released.
static mp_obj_t mq_get(mp_obj_t self_in) {
    uint16_t w;
    for (;;) {
        MP_THREAD_GIL_EXIT();
        wait_begin();
        BaseType_t ok = xQueueReceive(rxq, &w, pdMS_TO_TICKS(10));
        fw_waiting = false;
        MP_THREAD_GIL_ENTER();
        if (ok == pdTRUE) {
            n_taken++;
            return MP_OBJ_NEW_SMALL_INT(w);
        }
        mp_handle_pending(true);
    }
}
static MP_DEFINE_CONST_FUN_OBJ_1(mq_get_obj, mq_get);

static void tx_push(uint8_t b) {
    while (xQueueSend(txq, &b, 0) != pdTRUE) {
        MP_THREAD_GIL_EXIT();
        BaseType_t ok = xQueueSend(txq, &b, pdMS_TO_TICKS(10));
        MP_THREAD_GIL_ENTER();
        if (ok == pdTRUE) {
            return;
        }
        mp_handle_pending(true);
    }
}

// An int (its low byte), or bytes/bytearray/memoryview/str (every byte).
static mp_obj_t mq_put(mp_obj_t self_in, mp_obj_t x) {
    if (mp_obj_is_int(x)) {
        tx_push(mp_obj_get_int_truncated(x) & 0xFF);
    } else {
        mp_buffer_info_t buf;
        mp_get_buffer_raise(x, &buf, MP_BUFFER_READ);
        const uint8_t *p = buf.buf;
        for (size_t i = 0; i < buf.len; i++) {
            tx_push(p[i]);
        }
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mq_put_obj, mq_put);

static mp_int_t report(UBaseType_t n) {
    return n > FIFO_REPORT ? FIFO_REPORT : (mp_int_t)n;
}

// The firmware polls this. Empty: wait up to a tick for a word (pico_host
// sleeps 0.5 ms), GIL released, so the poll doesn't spin core 1.
static mp_obj_t mq_rx_fifo(mp_obj_t self_in) {
    UBaseType_t n = uxQueueMessagesWaiting(rxq);
    if (n == 0) {
        uint16_t w;
        MP_THREAD_GIL_EXIT();
        wait_begin();
        xQueuePeek(rxq, &w, 1);
        fw_waiting = false;
        MP_THREAD_GIL_ENTER();
        n = uxQueueMessagesWaiting(rxq);
    }
    return MP_OBJ_NEW_SMALL_INT(report(n));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mq_rx_fifo_obj, mq_rx_fifo);

static mp_obj_t mq_tx_fifo(mp_obj_t self_in) {
    UBaseType_t n = uxQueueMessagesWaiting(txq);
    if (n) {
        MP_THREAD_GIL_EXIT();       // pico_host: sleep(0) while the Z80 drains it
        taskYIELD();
        MP_THREAD_GIL_ENTER();
    }
    return MP_OBJ_NEW_SMALL_INT(report(n));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mq_tx_fifo_obj, mq_tx_fifo);

static mp_obj_t mq_active(size_t n_args, const mp_obj_t *args) {
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mq_active_obj, 1, 2, mq_active);

static mp_obj_t mq_restart(mp_obj_t self_in) {
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mq_restart_obj, mq_restart);

// The PIO instructions the firmware executes on SM0 with a visible effect;
// anything else (mov(osr, null), ...) does nothing, as in pico_host.
static mp_obj_t mq_exec(mp_obj_t self_in, mp_obj_t instr_in) {
    size_t len;
    const char *s = mp_obj_str_get_data(instr_in, &len);
    char t[40];
    size_t n = 0;
    for (size_t i = 0; i < len && n < sizeof(t) - 1; i++) {
        if (s[i] != ' ') {
            t[n++] = s[i];
        }
    }
    t[n] = 0;
    if (strcmp(t, "mov(y,invert(null))") == 0) {
        y = 0xFFFFFFFF;
    } else if (strncmp(t, "set(y,", 6) == 0 && n > 7 && t[n - 1] == ')') {
        y = strtoul(t + 6, NULL, 0);
    } else if (strcmp(t, "mov(y,invert(y))") == 0) {
        y = ~y;
    } else if (strcmp(t, "mov(y,null)") == 0) {
        y = 0;
    } else if (strcmp(t, "pull(noblock)") == 0) {
        uint8_t b;
        xQueueReceive(txq, &b, 0);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mq_exec_obj, mq_exec);

// A new StateMachine(0, ...) clears both FIFOs (pio_sm_init); Y stays.
static mp_obj_t mq_reset_fifos(mp_obj_t self_in) {
    xQueueReset(rxq);
    xQueueReset(txq);
    n_taken = n_pushed;             // the dropped words count as taken
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mq_reset_fifos_obj, mq_reset_fifos);

static const mp_rom_map_elem_t mq_locals_table[] = {
    { MP_ROM_QSTR(MP_QSTR_get), MP_ROM_PTR(&mq_get_obj) },
    { MP_ROM_QSTR(MP_QSTR_put), MP_ROM_PTR(&mq_put_obj) },
    { MP_ROM_QSTR(MP_QSTR_rx_fifo), MP_ROM_PTR(&mq_rx_fifo_obj) },
    { MP_ROM_QSTR(MP_QSTR_tx_fifo), MP_ROM_PTR(&mq_tx_fifo_obj) },
    { MP_ROM_QSTR(MP_QSTR_active), MP_ROM_PTR(&mq_active_obj) },
    { MP_ROM_QSTR(MP_QSTR_restart), MP_ROM_PTR(&mq_restart_obj) },
    { MP_ROM_QSTR(MP_QSTR_exec), MP_ROM_PTR(&mq_exec_obj) },
    { MP_ROM_QSTR(MP_QSTR_reset_fifos), MP_ROM_PTR(&mq_reset_fifos_obj) },
};
static MP_DEFINE_CONST_DICT(mq_locals_dict, mq_locals_table);

static MP_DEFINE_CONST_OBJ_TYPE(
    mq_type,
    MP_QSTR_MQ,
    MP_TYPE_FLAG_NONE,
    make_new, mq_make_new,
    locals_dict, &mq_locals_dict
    );

// ---- module functions --------------------------------------------------------

// frame(op, value) -> reply: the emulator's side, from Python, for tests.
static mp_obj_t mod_frame(mp_obj_t op_in, mp_obj_t value_in) {
    check_init();
    uint8_t op = mp_obj_get_int(op_in), value = mp_obj_get_int(value_in);
    MP_THREAD_GIL_EXIT();
    uint8_t r = p4bridge_frame(op, value);
    MP_THREAD_GIL_ENTER();
    return MP_OBJ_NEW_SMALL_INT(r);
}
static MP_DEFINE_CONST_FUN_OBJ_2(mod_frame_obj, mod_frame);

// stats() -> (frames, underruns, rx_overruns, y, rx_waiting, tx_waiting)
static mp_obj_t mod_stats(void) {
    check_init();
    p4bridge_stats_t s;
    p4bridge_get_stats(&s);
    mp_obj_t t[6] = {
        mp_obj_new_int_from_uint(s.frames),
        mp_obj_new_int_from_uint(s.underruns),
        mp_obj_new_int_from_uint(s.rx_overruns),
        mp_obj_new_int_from_uint(y),
        MP_OBJ_NEW_SMALL_INT(uxQueueMessagesWaiting(rxq)),
        MP_OBJ_NEW_SMALL_INT(uxQueueMessagesWaiting(txq)),
    };
    return mp_obj_new_tuple(6, t);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mod_stats_obj, mod_stats);

// replay.c
extern const mp_obj_fun_builtin_fixed_t p4b_replay_start_obj;
extern const mp_obj_fun_builtin_fixed_t p4b_replay_status_obj;

static const mp_rom_map_elem_t p4bridge_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_p4bridge) },
    { MP_ROM_QSTR(MP_QSTR_MQ), MP_ROM_PTR(&mq_type) },
    { MP_ROM_QSTR(MP_QSTR_frame), MP_ROM_PTR(&mod_frame_obj) },
    { MP_ROM_QSTR(MP_QSTR_stats), MP_ROM_PTR(&mod_stats_obj) },
    { MP_ROM_QSTR(MP_QSTR_replay), MP_ROM_PTR(&p4b_replay_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_replay_status), MP_ROM_PTR(&p4b_replay_status_obj) },
};
static MP_DEFINE_CONST_DICT(p4bridge_globals, p4bridge_globals_table);

const mp_obj_module_t p4bridge_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&p4bridge_globals,
};

MP_REGISTER_MODULE(MP_QSTR_p4bridge, p4bridge_module);
