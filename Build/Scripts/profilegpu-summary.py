"""Summarises the first ProfileGPU table in an Unreal log: passes with their inclusive GPU time, as a tree.
Usage: python3 Build/Scripts/profilegpu-summary.py <log> [min_ms=0.2] [max_depth=6]   (ADR-0023; any platform)
"""
import re
import sys


def main():
    path = sys.argv[1]
    min_ms = float(sys.argv[2]) if len(sys.argv) > 2 else 0.2
    max_depth = int(sys.argv[3]) if len(sys.argv) > 3 else 6
    inside = False
    frame = None
    with open(path, encoding="utf-8", errors="replace") as log:
        for line in log:
            if "GPU Profile for Frame" in line:
                if inside:
                    break
                inside = True
                continue
            if not inside:
                continue
            match = re.search(r"Frame Time\s*:\s*([0-9.]+)ms", line)
            if match:
                frame = float(match.group(1))
                print("frame %.2f ms" % frame)
                continue
            cells = line.rstrip("\n").split("\u2503")
            if len(cells) < 4:
                if frame is not None and "LogRHI" not in line:
                    break
                continue
            inclusive = re.search(r"([0-9.]+) ms", cells[2])
            name = cells[3].rstrip()
            if not inclusive or not name.strip():
                continue
            depth = (len(name) - len(name.lstrip(" "))) // 3
            ms = float(inclusive.group(1))
            if ms >= min_ms and depth <= max_depth:
                print("%7.3f  %s%s" % (ms, "  " * depth, name.strip()))


if __name__ == "__main__":
    main()
