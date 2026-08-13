#!/usr/bin/env python3
"""Reset het bord en lees de seriële uitvoer een tijdje mee.

Bedoeld voor tests die zichzelf beoordelen: flash, draai dit, en lees de
uitkomst uit de log in plaats van uit het scherm.

`idf.py monitor` kan dat niet - die wil een terminal en stopt alleen op een
toetsaanslag. Dit script niet: het stopt op een tijdslimiet, of eerder zodra
een regel voorbijkomt die op --until past. Dat laatste scheelt bij een test die
zijn eindoordeel afdrukt de hele resterende wachttijd.

    python3 tools/serial_capture.py --seconds 30 --until "EINDE TEST" \
        --out /tmp/test.log

De reset gaat via de gebruikelijke ESP32-bedrading: RTS aan EN, DTR aan IO0.
Zonder die reset zie je de log pas vanaf het moment dat je meeleest, en mis je
precies het opstarten.
"""

import argparse
import re
import sys
import time

import serial


def reset_board(port):
    # EN laag met IO0 hoog: gewone reset, dus opstarten in de applicatie en
    # niet in de bootloader.
    port.setDTR(False)
    port.setRTS(True)
    time.sleep(0.1)
    port.setRTS(False)
    time.sleep(0.05)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--seconds", type=float, default=30.0)
    parser.add_argument("--until", default=None,
                        help="regex; stop zodra een regel erop past")
    parser.add_argument("--out", default=None)
    parser.add_argument("--no-reset", action="store_true")
    args = parser.parse_args()

    pattern = re.compile(args.until) if args.until else None
    lines = []

    with serial.Serial(args.port, args.baud, timeout=0.2) as port:
        if not args.no_reset:
            reset_board(port)
        port.reset_input_buffer()

        deadline = time.time() + args.seconds
        buffer = b""
        while time.time() < deadline:
            chunk = port.read(4096)
            if not chunk:
                continue
            buffer += chunk
            while b"\n" in buffer:
                raw, buffer = buffer.split(b"\n", 1)
                line = raw.decode("utf-8", errors="replace").rstrip("\r")
                lines.append(line)
                print(line, flush=True)
                if pattern and pattern.search(line):
                    deadline = 0  # klaar: het gezochte is voorbijgekomen
                    break

    if args.out:
        with open(args.out, "w") as f:
            f.write("\n".join(lines) + "\n")

    # Exitcode 2 als er op --until is gewacht maar het nooit kwam. Dan is de
    # test niet afgelopen, en dat is iets anders dan een test die faalt.
    if pattern and not any(pattern.search(line) for line in lines):
        print(f"\n[serial_capture] '{args.until}' is niet voorbijgekomen "
              f"binnen {args.seconds} s", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
