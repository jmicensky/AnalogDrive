#!/usr/bin/env python3
"""Checks HardwareProfiler captures for clipped recordings before they're
imported into ProfilerModels/.

A capture whose recording hit 0 dBFS models the audio interface's hard
clipping instead of the hardware, which sounds like a harsh crackle on
transients (this is what ruined the Oct 2026 tube captures). The plugin can't
undo that, so catch it here and re-record with less level into the interface.

Usage:
    python3 scripts/check_capture_clipping.py "/Applications/HardwareProfiler Projects/TUBE_PRE_triode" [more projects...]

Checks every gain-stage folder's *_rec.wav. Per stage it reports the peak
level and verdict:
    CLIPPED  a flat top: 3+ consecutive samples sitting at the file's own peak.
             Checked at whatever level the file peaks at, not just 0 dBFS:
             some captures were hard-limited at exactly -6 dBFS (CAPI TEST
             QUICK, CAPIVP28_NewRouting), which a full-scale check misses.
    HOT      peak above -1 dBFS (no flat top, but no safety margin either)
    ok       everything else
Exits 1 if any stage is CLIPPED, so it can gate an import script.
"""
import array
import math
import sys
import wave
from pathlib import Path

FLAT_TOP_RUN = 3        # consecutive samples at the peak that mean a real clip;
                        # a clean 24-bit sine touches its peak on ~1 sample
HOT_DBFS = -1.0


def read_samples(path):
    """Returns (interleaved int samples, full-scale value, 1-LSB step, channel count)."""
    with wave.open(str(path)) as w:
        width, channels = w.getsampwidth(), w.getnchannels()
        raw = w.readframes(w.getnframes())
    if width == 3:
        # Widen 24-bit to 32-bit (sample lands in the top 3 bytes) so array()
        # can parse it without a slow per-sample Python loop.
        wide = bytearray(len(raw) // 3 * 4)
        wide[1::4], wide[2::4], wide[3::4] = raw[0::3], raw[1::3], raw[2::3]
        return array.array("i", wide), float(1 << 31), 1 << 8, channels
    if width == 2:
        return array.array("h", raw), float(1 << 15), 1, channels
    if width == 4:
        return array.array("i", raw), float(1 << 31), 1, channels
    raise ValueError(f"unsupported sample width {width} bytes")


def analyse(path):
    """Returns (peak as 0-1 fraction of full scale, longest flat-top run, samples at peak)."""
    samples, full, lsb, channels = read_samples(path)
    if not samples:
        return 0.0, 0, 0
    peak = max(max(samples), -min(samples))
    limit = peak - lsb
    longest = 0
    for ch in range(channels):
        run = 0
        for v in samples[ch::channels]:
            if v >= limit or v <= -limit:
                run += 1
                longest = max(longest, run)
            else:
                run = 0
    at_peak = sum(1 for v in samples if v >= limit or v <= -limit)
    return peak / full, longest, at_peak


def stage_sort_key(name):
    try:
        return (0, float(name))      # "0", "50", "-18", "+3"
    except ValueError:
        return (1, name)


def check_project(project):
    stages = sorted((d for d in project.iterdir() if d.is_dir() and any(d.glob("*_rec.wav"))),
                    key=lambda d: stage_sort_key(d.name))
    if not stages:
        print(f"{project.name}: no gain-stage folders with *_rec.wav found")
        return None

    print(project.name)
    any_clipped = False
    for stage in stages:
        worst_peak, total_clipped, clip_level = 0.0, 0, None
        for rec in sorted(stage.glob("*_rec.wav")):
            peak, run, at_peak = analyse(rec)
            worst_peak = max(worst_peak, peak)
            if run >= FLAT_TOP_RUN:
                total_clipped += at_peak
                clip_level = peak if clip_level is None else max(clip_level, peak)
        peak_db = 20 * math.log10(worst_peak) if worst_peak > 0 else float("-inf")
        if clip_level is not None:
            verdict, any_clipped = (f"CLIPPED  ({total_clipped} samples flat-topped at "
                                    f"{20 * math.log10(clip_level):+.2f} dBFS)"), True
        elif peak_db > HOT_DBFS:
            verdict = "HOT"
        else:
            verdict = "ok"
        print(f"  {stage.name:>6}: peak {peak_db:+6.2f} dBFS  {verdict}")
    return any_clipped


def main(args):
    if not args:
        print(__doc__)
        return 2
    any_clipped = False
    for arg in args:
        result = check_project(Path(arg))
        any_clipped = any_clipped or bool(result)
    if any_clipped:
        print("\nClipped stages found: re-record them with less level into the interface "
              "(aim for the loudest stage peaking around -6 dBFS).")
    return 1 if any_clipped else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
