# HF5A Stage 6.2e — Windows 10 x64 portable candidate

Application source and packaging commit: see `source.json` in this archive.
Normal full desktop, QtKeychain enabled. This is **not an RX-only application**.
Alt + left-drag on the confirmed red TX line is preview only, without CI-V writes.
This package does not authorize a hardware test, transmitting or installation.

## Start without touching production

1. Keep Stage 5.2 FINAL GOOD, production directories and profiles unchanged.
2. Use a **separate Windows test account**. The application uses fixed QtKeychain
   credential names: a different settings directory does NOT isolate native
   credentials within the same Windows account. Do not create an account through
   this package; use an independently prepared test account. Do not copy passwords,
   profiles or application data from production.
3. Extract the inner ZIP into a new writable directory, e.g. `C:\HF5A-Test\Stage6.2e`.
   Verify its `.sha256` from the Actions artifact; `SHA256SUMS.txt` lists payload files.
4. Run **START-HF5A.cmd**, not AetherSDR.exe directly. The launcher refuses an account
   with an existing standard AetherSDR data directory and asks for confirmation of
   a separate test account. It creates a fresh `test-profiles\<GUID>` on EVERY start,
   sets the existing `AETHER_SETTINGS_DIR` override and verifies `--config path`.
   Autoconnect, automation bridge and startup updates are disabled in this new
   profile only. Logs/recordings remain with that profile. Earlier test profiles
   are retained; no production data are migrated. No system environment is changed.
5. Do not connect to a radio for this offline check. No parallel production LAN
   session, no COM4/COM5 changes, no TX/PTT/TUNE. Hardware use needs separate approval.

`AetherSDR.exe` alone retains normal production-profile behavior and is NOT an
isolated entry point. The test launcher is mandatory. A separate account is also
needed for resources outside SettingsPaths and Windows Credential Manager; a
folder or environment variable cannot sandbox those. No services or installer.
Normal mouse gestures outside the TX preview can tune the radio after connection.

## Evidence and limits

The recipe reuses the Stage 6.1d full desktop dependencies/deployment, including
QtKeychain, RTL/libusb, FFTW, ASR/ONNX/sherpa, OpenSSL, Qt plugins and MSVC runtime.
Packaging checks include dumpbin imports, DLL closure, native qwindows plugin,
40 regressions (the twelve preview/CI-V tests plus retained production checks)
and early-exit offline Qt/settings probes under program-specific firewall rules.
Escape and focus/window changes preserve the yellow local selection; a new Alt
drag updates it. SPLIT OFF, TX, disconnected or stale/changed readback clears it.
The confirmed red marker remains independent of that local selection.


The packaged launcher is exercised twice with a deliberately inherited wrong
profile, an unchanged sentinel in the normal profile, and a restricted PATH.
CI also checks refusal of a normal GUI launch under an existing production profile.
These are real Windows settings/Qt startup checks, **not a hardware test or a full
interactive GPU/MainWindow test**. CI does not qualify a particular Windows 10 GPU.

The artifact contains source/workflow commit manifests, EXE/inner-ZIP SHA256 and
per-file SHA256. The external GitHub artifact ZIP has its own digest, stored in
the separate evidence artifact. Do not confuse those three hashes.

## Return to production

Close the candidate and ensure its radio session (if later authorized) is closed.
Return to the original Windows account and original Stage 5.2 shortcut. Since the
candidate uses a separate directory/account and no deployment occurs, no settings
restore or uninstall should be needed. Retain the original archive and backups.
