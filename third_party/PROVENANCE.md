# Provenance

Where every piece of third-party code in this tree came from. Lifted code keeps its license header and attribution (PLAN.md §5).

## Reference submodules (read-only)

These are pinned checkouts, used for reading and for lifting from. Nothing under them is built into the image directly.

| Path | Repo | Pin | Commit | License |
|---|---|---|---|---|
| `firmware` | https://github.com/timex-sinclair-projects/tspico-firmware-build | `v2.3.1` | `58ccbcdf4145ca07acd613c57de34639d976ef59` | MIT (code), CC BY 4.0 (docs) |
| `third_party/fuse-tspico` | https://github.com/timex-sinclair-projects/fuse-tspico | `tspico-v1.10.0-1` | `a43958dd428768b11b7d45a03197ded5ee4b84f9` | GPL-2.0-or-later |
| `third_party/libspectrum` | https://git.code.sf.net/p/fuse-emulator/libspectrum | `master` (`libspectrum-1.7.0-11`) | `2d95bbd9c70d540cf093c3dfffc6d5a0da528997` | GPL-2.0-or-later |
| `third_party/ZX-ESPectrum-IDF` | https://github.com/EremusOne/ZX-ESPectrum-IDF | `v1.4.15` | `2a2c3350eb2b26dd871fc1f98cbf4f6c93c41948` | GPL-3.0 |

`firmware` is the TS-Pico firmware itself, not a reference: `src/TS` runs on core 1 unchanged (PLAN.md §5, "The firmware is the same code on every platform").

## Lifted files

One row per file copied into `machine/`, `z80/` or `tspico/`. Note any local change in the last column.

| Our path | Source submodule | Source path | Source commit | Local changes |
|---|---|---|---|---|
| _none yet_ | | | | |

## Not used

- **TSRun** (https://github.com/josef-jelinek/TSRun): no license. Its README may be read as documentation; no code is copied.
