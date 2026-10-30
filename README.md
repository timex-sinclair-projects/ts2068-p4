# esp32-2068

A standalone Timex/Sinclair 2068 with a TS-Pico, running on an Elecrow CrowPanel Advanced 7" ESP32-P4.

The emulator runs on one P4 core; the real TS-Pico MicroPython firmware runs on the other. See [PLAN.md](PLAN.md) for the architecture and phases.

## Layout

| Path | Contents |
|---|---|
| `main/` | `app_main`, task and core setup |
| `platform/` | P4 display, audio, touch, keyboard, storage |
| `machine/` | TS2068 machine, SCLD and paging (lifted from Fuse) |
| `z80/` | Z80 core (chosen in Phase 2) |
| `tspico/` | Bridge hook and `p4bridge` queues |
| `firmware/` | Submodule: tspico-firmware-build (pinned) |
| `third_party/` | Read-only reference submodules; see [PROVENANCE.md](third_party/PROVENANCE.md) |
| `docs/` | [DECISIONS.md](docs/DECISIONS.md), [NOTES.md](docs/NOTES.md) |
| `test/` | Trace fixtures, Z80 suites, BASIC test programs |

## Getting the source

```bash
git clone --recurse-submodules <repo-url>
```

## License

To be set in Phase 2: GPL-2.0-or-later if Fuse's Z80 core is chosen, GPL-3.0 if Z80_JLS is (PLAN.md §2, "Licensing outcome").
