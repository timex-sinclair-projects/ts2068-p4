# btkbd: a Bluetooth LE keyboard for the TS2068 (PLAN.md Phase 3, "Input,
# stage 2"), through the C6 co-processor.
#
#   import btkbd
#   btkbd.start()        # Bluetooth on, and reconnect to the saved keyboard
#   btkbd.pair()         # look for a keyboard for 20 s, pair, and save it
#   btkbd.forget()       # drop the saved keyboard
#   btkbd.status()       # -> a line for the menu
#
# The C6's Bluetooth controller is turned on through esp-hosted (the hosted
# module), then MicroPython's bluetooth module runs NimBLE on the P4. A
# keyboard is a HID-over-GATT device: it's switched to the boot protocol
# where it has one, and its 8-byte key reports go through kbdmap to
# ts2068.keys(), which holds the keys down until the next report. Without
# the boot protocol, its ordinary reports are taken if they're 8 bytes.
#
# The keyboard and its bond (the keys pairing made) are kept in
# /p4/btkbd.json. Pairing is Secure Connections "Just Works", with no
# passkey: MicroPython v1.29 never passes NimBLE's passkey request to
# Python on a connection it made (extmod/nimble/modbluetooth_nimble.c,
# peripheral_gap_event_cb), so a keyboard that wants one waits until the
# pairing times out. Without one, nothing guards against someone in range
# listening in while it pairs.

import binascii
import json
import time

import bluetooth
from micropython import const

import kbdmap
import ts2068

CONF = "/p4/btkbd.json"

_IRQ_SCAN_RESULT = const(5)
_IRQ_SCAN_DONE = const(6)
_IRQ_PERIPHERAL_CONNECT = const(7)
_IRQ_PERIPHERAL_DISCONNECT = const(8)
_IRQ_GATTC_SERVICE_RESULT = const(9)
_IRQ_GATTC_SERVICE_DONE = const(10)
_IRQ_GATTC_CHARACTERISTIC_RESULT = const(11)
_IRQ_GATTC_CHARACTERISTIC_DONE = const(12)
_IRQ_GATTC_DESCRIPTOR_RESULT = const(13)
_IRQ_GATTC_DESCRIPTOR_DONE = const(14)
_IRQ_GATTC_WRITE_DONE = const(17)
_IRQ_GATTC_NOTIFY = const(18)
_IRQ_ENCRYPTION_UPDATE = const(28)
_IRQ_GET_SECRET = const(29)
_IRQ_SET_SECRET = const(30)

_IO_NO_INPUT_OUTPUT = const(3)

_HID = bluetooth.UUID(0x1812)
_PROTOCOL_MODE = bluetooth.UUID(0x2A4E)
_BOOT_KEYBOARD_IN = bluetooth.UUID(0x2A22)
_REPORT = bluetooth.UUID(0x2A4D)
_CCCD = bluetooth.UUID(0x2902)
_NOTIFY = const(0x10)
_APPEARANCE_KEYBOARD = const(0x03C1)

_ble = None
_failed = None
_conf = {}                  # "addr_type", "addr" (hex), "name", "secrets"
_secrets = {}               # (type, key) -> value
_conn = None
_pairing = False
_found = {}                 # scanning: addr -> (addr_type, name)
_status = "Bluetooth off"
_svc = None                 # the HID service's handle range
_chars = []                 # (end_handle, value_handle, properties, uuid)
_todo = []                  # GATT steps still to run, one at a time
_inputs = set()             # value handles whose notifications are keys


def status():
    return _status


def _say(text):
    global _status
    _status = text


def _load():
    global _conf, _secrets
    try:
        with open(CONF) as f:
            _conf = json.load(f)
    except (OSError, ValueError):
        _conf = {}
    _secrets = {}
    for t, k, v in _conf.get("secrets", ()):
        _secrets[t, binascii.a2b_base64(k)] = binascii.a2b_base64(v)


def _save():
    _conf["secrets"] = [(t, binascii.b2a_base64(k).strip(), binascii.b2a_base64(v).strip())
                        for (t, k), v in _secrets.items()]
    with open(CONF, "w") as f:
        json.dump(_conf, f)


def _adv_fields(adv):
    """Advertising data -> {type: [values]}"""
    out, i = {}, 0
    while i + 1 < len(adv):
        n = adv[i]
        if n == 0:
            break
        out.setdefault(adv[i + 1], []).append(bytes(adv[i + 2:i + 1 + n]))
        i += 1 + n
    return out


def _is_keyboard(fields):
    for t in (0x02, 0x03):              # 16-bit service UUIDs
        for v in fields.get(t, ()):
            for j in range(0, len(v) - 1, 2):
                if v[j] | v[j + 1] << 8 == 0x1812:
                    return True
    for v in fields.get(0x19, ()):      # appearance
        if len(v) == 2 and v[0] | v[1] << 8 == _APPEARANCE_KEYBOARD:
            return True
    return False


def _name(fields):
    for t in (0x09, 0x08):
        for v in fields.get(t, ()):
            try:
                return v.decode()
            except UnicodeError:
                pass
    return None


def _release():
    ts2068.keys()


def _cancel():
    """Stop a connection attempt, if one is under way"""
    try:
        _ble.gap_connect(None)
    except OSError:
        pass


def _connect():
    _say("Connecting to " + _conf.get("name", "keyboard"))
    _ble.gap_connect(_conf["addr_type"], binascii.unhexlify(_conf["addr"]), 30000)


def _next():
    """Run the next GATT step, or report the keyboard ready"""
    if _todo:
        _todo.pop(0)()
    else:
        _say("Keyboard: " + _conf.get("name", "connected"))


def _subscribe(value_handle):
    """Find this characteristic's CCCD and turn its notifications on. The
    range starts at the value handle (NimBLE looks after it), and ends at
    the characteristic's end handle."""
    end = [c[0] for c in _chars if c[1] == value_handle][0]
    _ble.gattc_discover_descriptors(_conn, value_handle, end)


def _plan():
    """After the HID service's characteristics: what to write and subscribe"""
    boot = [c for c in _chars if c[3] == _BOOT_KEYBOARD_IN]
    mode = [c for c in _chars if c[3] == _PROTOCOL_MODE]
    if boot and mode:
        # The boot protocol: 0 to Protocol Mode, without a response
        _ble.gattc_write(_conn, mode[0][1], b"\x00", 0)
        targets = [boot[0][1]]
    else:
        targets = [c[1] for c in _chars if c[3] == _REPORT and c[2] & _NOTIFY]
    if not targets:
        _say("No key reports on this keyboard")
        return
    for h in targets:
        _inputs.add(h)
        _todo.append(lambda h=h: _subscribe(h))
    _next()


def _keys(data):
    if len(data) == 7:                  # no reserved byte
        data = data[:1] + b"\x00" + data[1:]
    if len(data) != 8 or kbdmap.rollover(data):
        return
    rows = kbdmap.rows(data)
    if rows == b"\xff" * 8:
        ts2068.keys()
    else:
        ts2068.keys(rows)


def _irq(event, data):
    global _conn, _svc, _pairing
    if event == _IRQ_SCAN_RESULT:
        addr_type, addr, adv_type, rssi, adv = data
        fields = _adv_fields(adv)
        key = bytes(addr)
        if key in _found:
            if _found[key][1] is None and _name(fields):
                _found[key] = (addr_type, _name(fields))
        elif _is_keyboard(fields):
            _found[key] = (addr_type, _name(fields))
            _ble.gap_scan(None)
    elif event == _IRQ_SCAN_DONE:
        if _pairing:
            if not _found:
                _pairing = False
                _say("No keyboard found")
                return
            addr, (addr_type, name) = next(iter(_found.items()))
            _conf.clear()
            _secrets.clear()
            _conf.update(addr_type=addr_type, addr=binascii.hexlify(addr).decode(),
                         name=name or "keyboard")
            _connect()
    elif event == _IRQ_PERIPHERAL_CONNECT:
        _conn = data[0]
        _say("Pairing..." if _pairing else "Securing...")
        _ble.gap_pair(_conn)
    elif event == _IRQ_PERIPHERAL_DISCONNECT:
        conn = data[0]
        _release()
        if conn == _conn:
            _conn = None
            _inputs.clear()
        if _conf.get("addr") and not _pairing:
            _connect()                  # the keyboard may wake again
        elif _pairing:
            _pairing = False
            _say("Pairing failed")
    elif event == _IRQ_ENCRYPTION_UPDATE:
        conn, encrypted, authenticated, bonded, key_size = data
        if not encrypted:
            _say("Pairing refused")
            return
        if _pairing:
            _pairing = False
            _save()
        del _chars[:]
        del _todo[:]
        _svc = None
        _say("Reading keyboard...")
        _ble.gattc_discover_services(_conn, _HID)
    elif event == _IRQ_GATTC_SERVICE_RESULT:
        conn, start, end, uuid = data
        if uuid == _HID:
            _svc = (start, end)
    elif event == _IRQ_GATTC_SERVICE_DONE:
        if _svc:
            _ble.gattc_discover_characteristics(_conn, _svc[0], _svc[1])
        else:
            _say("Not a keyboard (no HID)")
    elif event == _IRQ_GATTC_CHARACTERISTIC_RESULT:
        conn, end_handle, value_handle, properties, uuid = data
        _chars.append((end_handle, value_handle, properties, bluetooth.UUID(uuid)))
    elif event == _IRQ_GATTC_CHARACTERISTIC_DONE:
        _plan()
    elif event == _IRQ_GATTC_DESCRIPTOR_RESULT:
        conn, dsc_handle, uuid = data
        if bluetooth.UUID(uuid) == _CCCD:
            _todo.insert(0, lambda: _ble.gattc_write(_conn, dsc_handle, b"\x01\x00", 1))
    elif event == _IRQ_GATTC_DESCRIPTOR_DONE:
        _next()
    elif event == _IRQ_GATTC_WRITE_DONE:
        _next()
    elif event == _IRQ_GATTC_NOTIFY:
        conn, value_handle, notify_data = data
        if value_handle in _inputs:
            _keys(bytes(notify_data))
    elif event == _IRQ_SET_SECRET:
        sec_type, key, value = data
        key = sec_type, bytes(key)
        if value is None:
            return _secrets.pop(key, None) is not None
        _secrets[key] = bytes(value)
        if not _pairing:
            _save()
        return True
    elif event == _IRQ_GET_SECRET:
        sec_type, index, key = data
        if key is None:
            i = 0
            for (t, _), value in _secrets.items():
                if t == sec_type:
                    if i == index:
                        return value
                    i += 1
            return None
        return _secrets.get((sec_type, bytes(key)), None)


def _on():
    """Bluetooth on, once a boot: the C6's controller, then NimBLE. A
    failed start can't be retried in the same boot (NimBLE hangs if it's
    started again), so a failure sticks until a reset."""
    global _ble, _failed
    if _ble:
        return True
    if _failed:
        _say("Bluetooth failed: %s" % _failed)
        return False
    _say("Bluetooth starting...")
    try:
        import hosted
        hosted.bt_enable()
        time.sleep_ms(1000)             # the C6's controller, ready for HCI
        b = bluetooth.BLE()
        b.active(True)
    except Exception as e:
        _failed = repr(e)
        _say("Bluetooth failed: %s" % _failed)
        return False
    _load()
    b.config(bond=True, le_secure=True, mitm=False, io=_IO_NO_INPUT_OUTPUT)
    b.irq(_irq)
    _ble = b
    _say("Bluetooth on")
    return True


def start():
    """Bluetooth on, and connect to the saved keyboard if there is one"""
    if not _on():
        return
    if _conf.get("addr") and _conn is None:
        _connect()
    elif not _conf.get("addr"):
        _say("No keyboard: Pair one")


def pair():
    """Look for a keyboard (one in pairing mode) for 20 s, then pair"""
    global _pairing
    if not _on():
        return
    if _conn is not None:
        _ble.gap_disconnect(_conn)
    _cancel()                           # a reconnect in progress
    _found.clear()
    _pairing = True
    _say("Looking for a keyboard...")
    _ble.gap_scan(20000, 30000, 30000, True)


def forget():
    global _conf
    if _ble and _conn is not None:
        _ble.gap_disconnect(_conn)
    if _ble:
        _cancel()
    _conf = {}
    _secrets.clear()
    try:
        import os
        os.remove(CONF)
    except OSError:
        pass
    _say("No keyboard: Pair one")


def saved():
    """The saved keyboard's name, or None"""
    if not _conf:
        _load()
    return _conf.get("name") if _conf.get("addr") else None
