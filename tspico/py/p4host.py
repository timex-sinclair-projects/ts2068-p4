# The TS-Pico firmware on the ESP32-P4: MicroPython's counterpart of
# tspico-firmware-build tools/emu/pico_host.py.
#
# The firmware (/TS, unmodified) runs here on core 1. Its port 0Eh/0Fh
# state machine is p4bridge.MQ, a C model that the emulator side reaches
# from core 0 (docs/DECISIONS.md D4). Everything else RP2040-specific is a
# stand-in installed in sys.modules BEFORE the firmware is imported, as
# pico_host.micropython_env() does:
#
#   rp2        StateMachine(0, TS_IO_DUAL) -> p4bridge.MQ; asm_pio a no-op
#   machine    Pin and SPI do nothing, freq() fixed, and NO mem32: with
#              mem32, TS/tspico_io.py writes RP2040 PIO registers, which
#              on the P4 are arbitrary addresses
#   TS.sdcard  SDCard(spi, cs) -> the board's microSD (machine.SDCard)
#   select     nothing on stdin, ever (the console stays ours)
#
# COMPAT = True also reports what pico_host reports (free RAM, flash and
# card sizes, uname, sys.implementation, BUILD_VERSION), so the firmware
# sends the same bytes as in the Phase 0 traces (test/traces). It changes
# nothing else.
#
# Temporary: PLAN.md §1.3's upstream board layer replaces this file.

import sys
import gc
import _thread
import machine as _machine
import p4bridge

COMPAT = True
BUILD_VERSION = "unknown"       # p4install writes /p4/build.txt: the firmware commit, as pico_host shows it

# The board's microSD (tools/p4/board.json via /p4/sd.json); None: no card.
SD_ARGS = None


class _Mod:
    """A stand-in module: an object whose attributes are the names."""

    def __init__(self, **names):
        for k, v in names.items():
            setattr(self, k, v)


def _ident(*a, **k):
    # asm_pio and friends, used bare (@native) or with arguments (@asm_pio(...)).
    if len(a) == 1 and callable(a[0]) and not k:
        return a[0]
    return lambda f: f


class Pin:
    IN = OUT = PULL_UP = PULL_DOWN = OPEN_DRAIN = 0

    def __init__(self, *a, **k):
        self.v = k.get("value", 0)

    def value(self, *a):
        if a:
            self.v = a[0]
        return self.v

    def on(self):
        self.v = 1

    def off(self):
        self.v = 0

    def toggle(self):
        self.v ^= 1

    def __call__(self, *a):
        return self.value(*a)


class SPI:
    def __init__(self, *a, **k):
        pass


class _OtherSM:
    """The ROM / BANK state machines and NULL_SM: nothing to drive here."""

    def __init__(self, *a, **k):
        pass

    def active(self, *a):
        pass

    def put(self, *a):
        pass

    def get(self, *a):
        return 0

    def exec(self, *a):
        pass

    def restart(self):
        pass

    def rx_fifo(self):
        return 0

    def tx_fifo(self):
        return 0


def StateMachine(n, prog=None, *a, **k):
    io = sys.modules.get("TS.tspico_io")
    if n == 0 and io is not None and prog is getattr(io, "TS_IO_DUAL", None):
        mq = p4bridge.MQ()
        mq.reset_fifos()
        return mq
    return _OtherSM()


class PIO:
    OUT_LOW = OUT_HIGH = IN_LOW = IN_HIGH = 0
    SHIFT_LEFT = SHIFT_RIGHT = 0
    JOIN_NONE = JOIN_TX = JOIN_RX = 0
    IRQ_SM0 = IRQ_SM1 = IRQ_SM2 = IRQ_SM3 = 0


# ---- the card ---------------------------------------------------------------

_card = None


def _real_card():
    global _card
    if _card is None:
        if SD_ARGS is None:
            raise OSError(19, "ENODEV")
        _card = _machine.SDCard(**SD_ARGS)
    return _card


class SDCard:
    """TS.sdcard.SDCard(spi, cs): the board's card, as a block device. The
    firmware identifies a card by CID; pico_host's stand-in reports 0x5A5A."""

    CID = 0x5A5A
    CIDBYTES = bytes(16)

    def __init__(self, *a, **k):
        self._dev = _real_card()

    def readblocks(self, n, buf, off=None):
        return self._dev.readblocks(n, buf) if off is None else self._dev.readblocks(n, buf, off)

    def writeblocks(self, n, buf, off=None):
        return self._dev.writeblocks(n, buf) if off is None else self._dev.writeblocks(n, buf, off)

    def ioctl(self, op, arg):
        return self._dev.ioctl(op, arg)


# ---- what pico_host reports (COMPAT) ----------------------------------------

def _make_os():
    import os
    names = {}
    for k in dir(os):
        if not k.startswith("__"):
            names[k] = getattr(os, k)
    m = _Mod(**names)
    if COMPAT:
        def statvfs(p=""):
            full = p if p.startswith("/") else (os.getcwd().rstrip("/") + "/" + p)
            if full == "/sd" or full.startswith("/sd/"):
                return (4096, 4096, 61440, 57000, 57000, 0, 0, 0, 0, 255)    # pico_host: a 240 MB card
            return (4096, 4096, 352, 182, 182, 0, 0, 0, 0, 255)              # pico_host: the Pico's 1.4 MB flash

        def uname():
            return ("rp2", "rp2", "1.29.0", "v1.29.0 (host)", "Raspberry Pi Pico (emulated)")
        m.statvfs = statvfs
        m.uname = uname
    return m


def _make_gc():
    m = _Mod(collect=gc.collect, enable=gc.enable, disable=gc.disable,
             isenabled=gc.isenabled, mem_alloc=gc.mem_alloc, mem_free=gc.mem_free)
    if COMPAT:
        m.mem_free = lambda: 180_000                    # pico_host's figure
    return m


def _make_sys():
    names = {}
    for k in dir(sys):
        if not k.startswith("__"):
            names[k] = getattr(sys, k)
    m = _Mod(**names)
    if COMPAT:
        m.implementation = _Mod(name="micropython", version=(1, 29, 0, ""),
                                _machine="Raspberry Pi Pico (emulated)")
    return m


class _Poll:
    def register(self, *a):
        pass

    def unregister(self, *a):
        pass

    def poll(self, *a):
        return []

    def ipoll(self, *a):
        return ()


# ---- install and run --------------------------------------------------------

def install():
    """Stand-ins in place, then import the firmware. Returns TS.tspico."""
    if "TS.tspico" in sys.modules:
        return sys.modules["TS.tspico"]
    sys.modules["rp2"] = _Mod(StateMachine=StateMachine, asm_pio=_ident, PIO=PIO)
    sys.modules["machine"] = _Mod(Pin=Pin, SPI=SPI, freq=lambda *a: 270_000_000,
                                  reset=_machine.reset, unique_id=_machine.unique_id,
                                  idle=_machine.idle)
    gc_mod = _make_gc()
    sys.modules["gc"] = gc_mod
    import TS
    sd = _Mod(SDCard=SDCard)
    sys.modules["TS.sdcard"] = sd
    TS.sdcard = sd
    os_mod = _make_os()
    sys_mod = _make_sys()
    import TS.tspico as fw
    import TS.tspico_io as io
    import TS.catalog, TS.native, TS.channels, TS.printer      # noqa: F401
    for name in list(sys.modules):
        if not name.startswith("TS.") or name == "TS.sdcard":
            continue
        m = sys.modules[name]
        if hasattr(m, "os"):
            m.os = os_mod
        if hasattr(m, "sys"):
            m.sys = sys_mod
        if hasattr(m, "gc"):
            m.gc = gc_mod
        if hasattr(m, "mem_free"):
            m.mem_free = gc_mod.mem_free
    io.select = _Mod(POLLIN=1, poll=_Poll)
    fw.machine = sys.modules["machine"]
    sys.modules["dev_tspico"] = fw                          # extcmd looks for /dev_tspico.py first
    if COMPAT:
        fw.BUILD_VERSION = BUILD_VERSION
    return fw


def _load_settings():
    global SD_ARGS, BUILD_VERSION
    import json
    try:
        with open("/p4/sd.json") as f:
            a = json.load(f)
        if "data" in a:
            a["data"] = tuple(a["data"])
        SD_ARGS = a
    except OSError:
        SD_ARGS = None
    try:
        with open("/p4/build.txt") as f:
            BUILD_VERSION = f.read().strip()
    except OSError:
        pass


def start(telemetry=True):
    """The firmware's main loop on a thread (core 1, like everything Python).
    Returns at once; the REPL stays usable."""
    _load_settings()
    fw = install()
    fw.TLM_ENABLED = telemetry
    _thread.stack_size(32 * 1024)

    def run():
        try:
            fw.TS2068_IO()
        except BaseException as e:      # noqa: BLE001
            sys.print_exception(e)
            print("[p4host] firmware stopped:", repr(e))
    _thread.start_new_thread(run, ())
    return fw


def ready():
    """The firmware is in its main loop: READY, and the boot pre-load queued
    (pico_host's selftest test)."""
    s = p4bridge.stats()
    return s[3] == 0xFFFFFFFF and s[5] >= 1
