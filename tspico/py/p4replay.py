# Gate 1 (PLAN.md Phase 1): replay a Phase 0 trace through p4bridge on
# core 0 against the real firmware on core 1, and report.
#
#   import p4replay; p4replay.run("fuse-smoke")
#
# The trace is /traces/<name>.bin (tools/p4/pack_frames.py). The firmware
# must have the trace's flash and card contents (test/traces/seeds).

import time
import p4bridge
import p4host

OPS = ("OUT0E", "IN0E", "IN0F", "OUT0F", "HELLO")


def wait_ready(timeout_s=120):
    t0 = time.ticks_ms()
    while not p4host.ready():
        if time.ticks_diff(time.ticks_ms(), t0) > timeout_s * 1000:
            raise OSError("firmware never reached its main loop")
        time.sleep_ms(100)
    return time.ticks_diff(time.ticks_ms(), t0)


def run(name, telemetry=False, boot=True):
    if boot:
        t0 = time.ticks_ms()
        p4host.start(telemetry=telemetry)
        ms = wait_ready()
        print("[p4replay] firmware ready after %d ms (import + boot: %d ms)"
              % (ms, time.ticks_diff(time.ticks_ms(), t0)))
    with open("/traces/%s.bin" % name, "rb") as f:
        frames = f.read()
    print("[p4replay] %s: %d frames" % (name, len(frames) // 3))
    p4bridge.replay(frames)
    last = -1
    while True:
        s = p4bridge.replay_status()
        if s["state"] == "done":
            break
        if s["index"] // 500 != last:
            last = s["index"] // 500
            print("[p4replay] %d/%d" % (s["index"], s["total"]))
        time.sleep_ms(200)
    report(s)
    return s


def report(s):
    el = s["elapsed_us"]
    print("[p4replay] done: %d frames in %.2f s (%.0f us/frame)"
          % (s["total"], el / 1e6, el / max(1, s["total"])))
    if s["in_data"]:
        print("[p4replay] IN 0Eh: %d, mean %.0f us, max %d us"
              % (s["in_data"], s["in_data_us"] / s["in_data"], s["in_data_max_us"]))
    print("[p4replay] status polls %d, status timeouts %d, underruns %d, rx overruns %d"
          % (s["status_polls"], s["status_timeouts"], s["underruns"], s["rx_overruns"]))
    print("[p4replay] writes waited %.2f s for the firmware, %d timed out"
          % (s["write_wait_us"] / 1e6, s["write_wait_timeouts"]))
    print("[p4replay] mismatches: %d" % s["mismatches"])
    for i, op, v, exp, got in s["bad"]:
        print("[p4replay]   frame %d: %s %02X expected %02X got %02X" % (i, OPS[op], v, exp, got))
    print("[p4replay] %s" % ("PASS" if s["mismatches"] == 0 else "FAIL"))
