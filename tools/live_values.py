#!/usr/bin/env python3
"""Live view of the telemetry captured from the diagnostic port.

Reads req/res log lines as produced by the Arduino sketches (optionally
prefixed with a Unix timestamp, as in data/bus.txt) and displays the decoded
values in real time.

Usage:
    python3 tools/live_values.py data/bus.txt          # replay file, then follow
    python3 tools/live_values.py data/bus.txt --no-follow
    cat /dev/ttyACM0 | python3 tools/live_values.py    # straight from serial

Byte mapping and conversion formulas are documented in
"Reverse Engineering Midea's ODU Diagnostic Port":
https://medium.com/@florian.mckee/reverse-engineering-mideas-odu-diagnostic-port-af603e159053
"""

import math
import re
import sys
import time

LINE_RE = re.compile(r"res=0x([0-9A-Fa-f]{20}), status=OK")


# ── conversions ───────────────────────────────────────────────────────────────

def ntc_temp(v):
    """Ambient/coil temperature: 8-bit ADC reading of an NTC divider (Beta model).

    0x00 and 0xFF are fault codes, displayed as -66.0 and 255.0 by the tester.
    """
    if v == 0:
        return -66.0
    if v == 255:
        return 255.0
    t = 1.0 / (1.0 / 298.15 + math.log(0.81 * (255.0 - v) / v) / 4150.0) - 273.15
    return round(t * 2) / 2  # tester rounds to nearest 0.5


def discharge_temp(v):
    """Discharge temperature: same divider, Steinhart-Hart model.

    0x00 reads as -48; the firmware clamps above ~0xE6, and 0xFE/0xFF are
    fault codes shown literally, so those are passed through raw.
    """
    if v == 0:
        return -48
    if v >= 254:
        return v
    L = math.log((255.0 - v) / v)
    return round(1.0 / (2.873e-3 + 2.491e-4 * L + 9.74e-7 * L**3) - 273.15)


def ac_voltage(v):
    return int(v * 32 / 25 + 40)


def current_draw(v):
    return math.trunc((0.117 * v + 0.92) * 100) / 100


def uint16(lo, hi):
    return lo | (hi << 8)


# ── field table: (label, unit, response type, extractor over payload bytes) ──

FIELDS = {
    "indoor_ambient":  ("Indoor ambient temperature",       "°C", 0x00, lambda b: ntc_temp(b[2])),
    "indoor_coil":     ("Indoor coil temperature",          "°C", 0x00, lambda b: ntc_temp(b[3])),
    "outdoor_ambient": ("Outdoor ambient temperature",      "°C", 0x00, lambda b: ntc_temp(b[5])),
    "outdoor_coil":    ("Outdoor coil temperature",         "°C", 0x00, lambda b: ntc_temp(b[4])),
    "discharge":       ("Compressor discharge temperature", "°C", 0x00, lambda b: discharge_temp(b[6])),
    "mode":            ("Operating mode",                   "",     0x02, lambda b: b[8]),
    "freq_target":     ("Compressor frequency (target)",    "Hz",   0x02, lambda b: b[2]),
    "freq_actual":     ("Compressor frequency (actual)",    "Hz",   0x02, lambda b: b[3]),
    "fan_speed":       ("Outdoor fan speed",                "",     0x00, lambda b: uint16(b[7], b[8])),
    "eev_steps":       ("EEV opening steps",                "",     0x01, lambda b: uint16(b[5], b[6])),
    "voltage":         ("Input voltage",                    "V",    0x01, lambda b: ac_voltage(b[3])),
    "current":         ("Current draw",                     "A",    0x01, lambda b: current_draw(b[2])),
}

# two-column layout, matching the article
LAYOUT = [
    ("indoor_ambient",  "freq_target"),
    ("indoor_coil",     "freq_actual"),
    ("outdoor_ambient", "fan_speed"),
    ("outdoor_coil",    "eev_steps"),
    ("discharge",       "voltage"),
    ("mode",            "current"),
]


class LiveView:
    def __init__(self):
        self.values = {}      # field key -> formatted value string
        self.frames = 0
        self.last_render = 0.0
        self.started = False

    def feed(self, line):
        m = LINE_RE.search(line)
        if not m:
            return
        frame = bytes.fromhex(m.group(1))
        if sum(frame) % 256 != 0 or frame[0] != 0x55:
            return
        self.frames += 1
        for key, (_, unit, rtype, extract) in FIELDS.items():
            if frame[1] == rtype:
                value = extract(frame)
                self.values[key] = f"{value} {unit}".strip() if unit else str(value)

    def cell(self, key, label_width=33, value_width=10):
        label, _, _, _ = FIELDS[key]
        value = self.values.get(key, "--")
        return f"{label:<{label_width}}{value:>{value_width}}"

    def render(self, force=False):
        now = time.monotonic()
        if not force and now - self.last_render < 0.2:
            return
        self.last_render = now
        if not self.started:
            sys.stdout.write("\x1b[2J\x1b[?25l")  # clear screen, hide cursor
            self.started = True
        out = ["\x1b[H"]  # cursor to top-left
        out.append("Midea ODU diagnostic port - live values\x1b[K\n\x1b[K\n")
        for left, right in LAYOUT:
            out.append(f"  {self.cell(left)}    {self.cell(right)}\x1b[K\n")
        out.append(f"\x1b[K\n  frames: {self.frames}   updated: {time.strftime('%H:%M:%S')}\x1b[K\n")
        sys.stdout.write("".join(out))
        sys.stdout.flush()

    def close(self):
        if self.started:
            sys.stdout.write("\x1b[?25h\n")  # show cursor again
            sys.stdout.flush()


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    follow = "--no-follow" not in sys.argv[1:]
    view = LiveView()
    try:
        if not args or args[0] == "-":
            for line in sys.stdin:
                view.feed(line)
                view.render()
        else:
            with open(args[0], "r", errors="replace") as f:
                for line in f:  # replay what's already there
                    view.feed(line)
                view.render(force=True)
                while follow:  # then behave like tail -f
                    line = f.readline()
                    if line:
                        view.feed(line)
                        view.render()
                    else:
                        time.sleep(0.2)
        view.render(force=True)
    except KeyboardInterrupt:
        pass
    finally:
        view.close()


if __name__ == "__main__":
    main()
