# Comment on micropython/micropython#17805

Posted 2026-10-31: https://github.com/micropython/micropython/issues/17805#issuecomment-6548588949

For https://github.com/micropython/micropython/issues/17805,
"BLE: Central Device cannot pair/bond Peripheral Device". Same bug, seen on
an ESP32-P4 (NimBLE through an ESP32-C6, esp-hosted), MicroPython v1.29.0,
pairing a Logitech Keys-To-Go 2 keyboard as the central. Still in master.

---

I hit this on v1.29.0 (ESP32-P4, central, pairing a BLE keyboard) and
traced it. NimBLE does raise the passkey request on the central: with
`CONFIG_BT_NIMBLE_LOG_LEVEL_DEBUG` the log shows the pairing
request/response and public keys crossing, then
`send passkey action request 3`. It never reaches Python because of which
GAP callback the connection has.

In `extmod/nimble/modbluetooth_nimble.c`:

- `gap_advertise()` registers `central_gap_event_cb`, which handles
  `BLE_GAP_EVENT_PASSKEY_ACTION` (and `BLE_GAP_EVENT_REPEAT_PAIRING`).
- `gap_connect()` (`mp_bluetooth_gap_peripheral_connect`) registers
  `peripheral_gap_event_cb`, which handles only `CONNECT` and `DISCONNECT`
  and passes the rest to `commmon_gap_event_cb`, which handles neither of
  those two events.

So on a connection we made, `ble_gap_passkey_event()` calls our callback,
the event falls through, nothing is passed to Python, and the peer waits
until the 30 s SMP timeout (`_IRQ_ENCRYPTION_UPDATE` with everything 0, as
in the report above). NimBLE doesn't send passkey events to
`ble_gap_event_listener`s either, so user code can't work round it.

Moving the two cases from `central_gap_event_cb` into
`commmon_gap_event_cb` fixes it for both roles. The same
`REPEAT_PAIRING` handling matters for centrals too, when a peripheral has
forgotten a bond the central still holds.

Pairing without MITM (`io=_IO_CAPABILITY_NO_INPUT_OUTPUT`, `mitm=False`)
works today as a workaround: Just Works never raises the event.

I can open a PR with the move if that helps.
