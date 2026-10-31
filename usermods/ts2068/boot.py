# boot.py: our modules (emu, osd, p4host and the rest) live in /p4, so that
# the flash's root is the TS-Pico's own view of its flash: config.ini,
# words.txt, assets, TMP, activity.log (tools/p4/install.py).
import sys

sys.path.append("/p4")
