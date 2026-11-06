#!/usr/bin/env python3
"""Pin the Windows test candidate to the Stage 6.1c application and tests."""
import os
from pathlib import Path
import subprocess

BASE = "4fc20d6b5c49697c29e6f6a9a5ae2ac655ab1ce3"
ROOT = Path(__file__).resolve().parents[1]


def main():
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    if os.environ.get("GITHUB_SHA") and os.environ["GITHUB_SHA"] != head:
        raise RuntimeError("Tested HEAD differs from GITHUB_SHA")
    subprocess.run(["git", "merge-base", "--is-ancestor", BASE, "HEAD"], cwd=ROOT, check=True)
    previous = subprocess.check_output(
        ["git", "ls-tree", "-r", "--name-only", BASE, ".github/workflows"],
        cwd=ROOT, text=True).splitlines()
    subprocess.run(["git", "diff", "--exit-code", BASE, "--", "src", "tests", "resources",
                    "CMakeLists.txt", "packaging", "scripts", "tools", ".github/ci-test-gate.txt",
                    ":(top,exclude,literal)tools/check_hf5a_stage6_1d.py", *previous],
                   cwd=ROOT, check=True)
    print("PASS: Stage 6.1c sources/tests, full desktop build defaults, deployment helpers and earlier CI unchanged")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
