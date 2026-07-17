#!/usr/bin/env python3
"""Read the firmware's battery-drain log and report %/hour per power phase.

    python tools/powerlog.py COM6           # pull from the device, print report
    python tools/powerlog.py --file log.csv # re-read a saved capture

Sends `powerlog` and reads the CSV the firmware dumps between PLOG_START and
PLOG_END. Stdlib + pyserial only, same as tools/screenshot.py.

The report answers one question: which phase is eating the battery? It rates
each phase in %/h, and converts to mA if you pass --mah for your cell.
"""
import argparse
import sys

PHASES = ("active", "doze", "deep", "boot", "wake")


def capture(port, baud=115200, timeout=20):
    try:
        import serial
    except ImportError:
        sys.exit("pyserial missing: pip install pyserial")
    import time

    with serial.Serial(port, baud, timeout=1) as s:
        time.sleep(0.3)
        s.reset_input_buffer()
        s.write(b"powerlog\n")
        s.flush()

        lines, started, deadline = [], False, time.time() + timeout
        while time.time() < deadline:
            raw = s.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", "replace").strip()
            if line == "PLOG_START":
                started, lines = True, []
                continue
            if line == "PLOG_END":
                return lines
            if started:
                lines.append(line)
    sys.exit("timed out waiting for PLOG_END — is the firmware running?")


def parse(lines):
    rows = []
    for line in lines:
        if not line or line.startswith("#") or line.startswith("i,"):
            continue
        f = line.split(",")
        if len(f) != 8:
            continue
        try:
            pct = None if f[3] == "-" else int(f[3])
            rows.append({
                "t": int(f[1]), "clock": f[2], "pct": pct, "phase": f[4],
                "charging": f[5] == "1", "vbus": f[6] == "1",
            })
        except ValueError:
            continue
    return rows


def report(rows):
    if len(rows) < 2:
        sys.exit(f"not enough samples to measure a slope ({len(rows)})")

    # Attribute each interval to the phase it was spent in — the phase of the
    # *earlier* sample. A sample records the phase in force at that instant, and
    # every phase change writes a sample at the moment it happens, so the span
    # after a sample was spent wholly in that sample's phase. (Using the later
    # sample instead charges each transition's preceding span to the new phase,
    # which smears up to one interval of the old phase's drain into the new one.)
    spans = {p: {"secs": 0, "drop": 0.0} for p in PHASES}
    charging_secs = 0

    for a, b in zip(rows, rows[1:]):
        dt = b["t"] - a["t"]
        if dt <= 0:
            continue                      # clock went backwards; skip
        if a["pct"] is None or b["pct"] is None:
            continue
        if a["charging"] or b["charging"] or a["vbus"] or b["vbus"]:
            charging_secs += dt           # on USB: drain is meaningless
            continue
        s = spans.setdefault(a["phase"], {"secs": 0, "drop": 0.0})
        s["secs"] += dt
        s["drop"] += a["pct"] - b["pct"]  # positive = discharging

    span_h = (rows[-1]["t"] - rows[0]["t"]) / 3600.0
    first = next((r["pct"] for r in rows if r["pct"] is not None), None)
    last = next((r["pct"] for r in reversed(rows) if r["pct"] is not None), None)

    print(f"samples   {len(rows)}   span {span_h:.1f} h "
          f"({rows[0]['clock']} -> {rows[-1]['clock']})")
    if first is not None and last is not None:
        print(f"battery   {first}% -> {last}%   net {first - last:+d} points")
    if charging_secs:
        print(f"excluded  {charging_secs / 3600.0:.1f} h on USB power")
    print()
    print(f"{'phase':8} {'hours':>7} {'drop':>7} {'%/hour':>8}   {'est. mA':>8}")
    print("-" * 46)
    return spans, span_h


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port", nargs="?", help="serial port, e.g. COM6")
    ap.add_argument("--file", help="read a saved CSV instead of the device")
    ap.add_argument("--mah", type=float, default=0,
                    help="battery capacity in mAh, to convert %%/h into mA")
    args = ap.parse_args()

    if args.file:
        with open(args.file) as fh:
            lines = fh.read().splitlines()
    elif args.port:
        lines = capture(args.port)
    else:
        ap.error("give a port or --file")

    rows = parse(lines)
    spans, _ = report(rows)

    for phase in PHASES:
        s = spans.get(phase)
        if not s or s["secs"] < 60:
            continue
        hours = s["secs"] / 3600.0
        rate = s["drop"] / hours
        ma = f"{rate * args.mah / 100.0:8.1f}" if args.mah else "       -"
        print(f"{phase:8} {hours:7.2f} {s['drop']:7.1f} {rate:8.2f}   {ma}")

    if not args.mah:
        print("\n(pass --mah <capacity> to convert %/hour into mA)")

    print("\nNote: the AXP2101 reports battery % from voltage, not a coulomb\n"
          "count, so load changes move it independently of real charge. Rates\n"
          "over short windows (< ~2 h) are unreliable; long, constant-load\n"
          "windows like an overnight deep-sleep run are the trustworthy ones.")


if __name__ == "__main__":
    main()
