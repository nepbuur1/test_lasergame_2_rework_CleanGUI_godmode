"""Capture an ESP32 boot and a bounded serial trace using the installed pyserial."""
import argparse
from pathlib import Path
import re
import time

import serial

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("port")
parser.add_argument("--seconds", type=float, default=95)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()

with serial.Serial(baudrate=115200, timeout=0.2) as connection:
    connection.port = args.port
    connection.dtr = False
    connection.rts = False
    connection.open()
    connection.reset_input_buffer()
    # GPIO0 stays high; pulse EN through RTS to capture a complete normal boot.
    connection.rts = True
    time.sleep(0.1)
    connection.rts = False
    deadline = time.monotonic() + args.seconds
    pending = b""
    with args.output.open("w", encoding="utf-8") as output:
        while time.monotonic() < deadline:
            pending += connection.read(connection.in_waiting or 1)
            while b"\n" in pending:
                line, pending = pending.split(b"\n", 1)
                text = re.sub(r"\x1b\[[0-9;]*m", "", line.decode("utf-8", errors="replace")).rstrip("\r")
                output.write(text + "\n")
                output.flush()
                print(text, flush=True)
        if pending:
            output.write(pending.decode("utf-8", errors="replace"))
print(f"Serial capture saved to {args.output}", flush=True)
