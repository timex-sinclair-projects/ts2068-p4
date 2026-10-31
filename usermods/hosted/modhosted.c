// hosted: the ESP32-C6 co-processor, reached through esp-hosted, for what
// MicroPython's own modules don't do with it.
//
//   import hosted, bluetooth
//   hosted.bt_enable()               # -> the C6's firmware version (a, b, c)
//   bluetooth.BLE().active(True)
//
// MicroPython's bluetooth module starts NimBLE on the P4, but the C6's
// Bluetooth controller must be initialised and enabled first, over
// esp-hosted's RPC, as esp-hosted's host_nimble_bleprph_host_only_vhci
// example does; without it NimBLE never syncs and active(True) times out.

#include "py/runtime.h"

#include "esp_hosted.h"
#include "esp_hosted_misc.h"

static mp_obj_t bt_enable(void) {
    esp_hosted_coprocessor_fwver_t ver = { 0 };
    esp_err_t err;

    esp_hosted_connect_to_slave();
    esp_hosted_get_coprocessor_fwversion(&ver);
    err = esp_hosted_bt_controller_init();
    if (err == ESP_OK) {
        err = esp_hosted_bt_controller_enable();
    }
    if (err != ESP_OK) {
        mp_raise_msg_varg(&mp_type_OSError, MP_ERROR_TEXT("C6 BT controller: %d"), (int)err);
    }
    mp_obj_t t[3] = {
        mp_obj_new_int(ver.major1), mp_obj_new_int(ver.minor1), mp_obj_new_int(ver.patch1)
    };
    return mp_obj_new_tuple(3, t);
}
static MP_DEFINE_CONST_FUN_OBJ_0(bt_enable_obj, bt_enable);

static const mp_rom_map_elem_t hosted_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_hosted) },
    { MP_ROM_QSTR(MP_QSTR_bt_enable), MP_ROM_PTR(&bt_enable_obj) },
};
static MP_DEFINE_CONST_DICT(hosted_globals, hosted_globals_table);

const mp_obj_module_t hosted_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&hosted_globals,
};

MP_REGISTER_MODULE(MP_QSTR_hosted, hosted_module);
