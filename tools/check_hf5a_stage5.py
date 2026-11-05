#!/usr/bin/env python3
"""Require the qualified production sources and previous workflows unchanged."""
import subprocess
from pathlib import Path

BASE = "82019c813cf90708aa2a1abcecdc4c34d72cbf02"
ROOT = Path(__file__).resolve().parents[1]

if __name__ == "__main__":
    previous = subprocess.check_output(
        ["git", "ls-tree", "-r", "--name-only", BASE, ".github/workflows"],
        cwd=ROOT, text=True).splitlines()
    subprocess.run(["git", "diff", "--exit-code", BASE, "--", "src", "CMakeLists.txt",
                    ".github/ci-test-gate.txt", *previous], cwd=ROOT, check=True)
    print("PASS: all production sources, build defaults and earlier workflows unchanged")
