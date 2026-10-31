# main.py: at power-on, the TS2068 from the card (emu.start, with the
# TS-Pico for a 32K ROM) and the side panel's menu (osd.start).
#
# To boot to the REPL instead, create /p4/noboot:
#   mpremote exec "open('/p4/noboot', 'w').close()"
# The second's wait first is for mpremote, whose Ctrl-C lands in it and
# stops this, should starting ever go wrong.
#
# After a soft reset (Ctrl-D at the REPL) this runs again, and the machine
# is started afresh: the reset ended the menu's and the firmware's threads,
# though not the machine, a C task.

import os
import time

time.sleep(1)
try:
    os.stat("/p4/noboot")
except OSError:
    try:
        import emu
        import osd
        emu.start()
        osd.start()
    except Exception as e:
        import sys
        sys.print_exception(e)
