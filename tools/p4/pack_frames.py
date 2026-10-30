#!/usr/bin/env python3
"""Pack a trace's .frames file (test/traces) into the 3-bytes-a-frame form
p4bridge.replay() takes: op, value, recorded reply.

    python3 tools/p4/pack_frames.py test/traces/fuse-smoke.frames out.bin
"""

import sys

OPS = {"OUT0E": 0, "IN0E": 1, "IN0F": 2, "OUT0F": 3, "HELLO": 4}


def pack(lines):
    out = bytearray()
    for n, line in enumerate(lines, 1):
        if not line.strip():
            continue
        try:
            op, value, arrow, reply = line.split()
            assert arrow == "->"
            out += bytes((OPS[op], int(value, 16), int(reply, 16)))
        except (ValueError, KeyError, AssertionError):
            sys.exit("line %d: can't read %r" % (n, line))
    return bytes(out)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    with open(sys.argv[1]) as f:
        data = pack(f.readlines())
    with open(sys.argv[2], "wb") as f:
        f.write(data)
    print("%s: %d frames" % (sys.argv[2], len(data) // 3))
