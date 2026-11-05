#!/usr/bin/env python3
"""Exercise the normal EXE's early-exit paths, never enter MainWindow.

Run only on the disposable Windows CI worker with program-specific firewall
rules already installed. No station credentials or native profile are seeded.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import uuid


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    args.report.mkdir(parents=True, exist_ok=True)
    profile = Path(tempfile.mkdtemp(prefix="profile-", dir=args.report.resolve()))
    env = os.environ.copy()
    env["AETHER_SETTINGS_DIR"] = str(profile)
    env.pop("AETHER_AUTOMATION", None)
    env.pop("QT_PLUGIN_PATH", None)
    env.pop("QT_QPA_PLATFORM_PLUGIN_PATH", None)
    env["QT_QPA_PLATFORM"] = "offscreen"
    evidence = {"exe": str(args.exe.resolve()), "profile": str(profile), "commands": []}

    def run(*arguments, expected=0):
        result = subprocess.run([str(args.exe.resolve()), *arguments], env=env,
                                capture_output=True, text=True, encoding="utf-8",
                                errors="replace", timeout=45)
        evidence["commands"].append({"args": arguments, "exit": result.returncode,
                                     "stdout": result.stdout, "stderr": result.stderr})
        if result.returncode != expected:
            raise RuntimeError(f"{arguments}: exit={result.returncode}, expected={expected}")
        return result.stdout.strip()

    try:
        marker = args.report.resolve() / "qt-startup.txt"
        run("--update-selftest", str(marker))
        if marker.read_text().strip() != "26.11.1":
            raise RuntimeError("Wrong Qt startup version marker")
        actual = run("--config", "path")
        if Path(actual).resolve() != (profile / "AetherSDR.db").resolve():
            raise RuntimeError("Settings CLI escaped the explicit profile")
        token = uuid.uuid4().hex
        run("--config", "set", "HF5AStage5Sentinel", token)
        if run("--config", "get", "HF5AStage5Sentinel") != token:
            raise RuntimeError("Settings did not persist between processes")
        # This command refuses BEFORE any credential write or keychain access.
        run("--config", "set", "MqttPass", "disposable-not-a-secret", expected=1)
        exported = run("--config", "export")
        if "disposable-not-a-secret" in exported:
            raise RuntimeError("Plaintext credential was persisted")
        evidence["databaseSHA256"] = hashlib.sha256((profile / "AetherSDR.db").read_bytes()).hexdigest()
        evidence["status"] = "PASS"
    except Exception as exc:
        evidence["status"] = "FAIL"
        evidence["error"] = str(exc)
    (args.report / "normal-exe-offline.json").write_text(json.dumps(evidence, indent=2))
    print(json.dumps(evidence, indent=2))
    return 0 if evidence["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
