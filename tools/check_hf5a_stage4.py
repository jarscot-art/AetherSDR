#!/usr/bin/env python3
"""Pin the already qualified readback, TX paths and previous CI to stage 3."""
import subprocess
from pathlib import Path

BASE = "be026cc91c35e976049e326bd706fb31899dc32a"
ROOT = Path(__file__).resolve().parents[1]


def main():
    protected = [
        "src/core/backends", "src/models", "src/gui/RxReadbackWidget.cpp",
        "src/gui/RxReadbackWidget.h", "src/gui/TxReadbackMarker.h",
        "src/gui/SpectrumWidget.cpp", "src/gui/SpectrumWidget.h",
        "src/gui/MainWindow_Wiring.cpp", "src/gui/MainWindow_Session.cpp",
        "src/core/control", "src/core/TxController.cpp", ".github/ci-test-gate.txt",
    ]
    # Include every previous workflow, but permit the new stage-specific job.
    previous = subprocess.check_output(
        ["git", "ls-tree", "-r", "--name-only", BASE, ".github/workflows"],
        cwd=ROOT, text=True).splitlines()
    result = subprocess.run(
        ["git", "diff", "--exit-code", BASE, "--", *protected, *previous], cwd=ROOT)
    if result.returncode:
        return result.returncode
    print("PASS: stage-3 readback, radio backends, models, TX controls and previous workflows unchanged")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
