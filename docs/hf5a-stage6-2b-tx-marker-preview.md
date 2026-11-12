# HF5A Stage 6.2b — TX marker mouse preview

Base: `ffc5fa7b56bf6568a2b19ae6ae23eff17eda9352` (Stage 6.1d).
Branch: `hf5a/stage6-2b-tx-marker-preview`.

Only informational GUI state is added. SpectrumWidget receives existing model
observations and deadlines, never a backend or a CI-V command. The only model
addition exposes the existing SPLIT deadline without changing its lifecycle.
The existing confirmed TxReadbackMarker is retained and drawn through the
waterfall as well as the spectrum; the separate warning-token dotted marker
and explicit PODGLĄD — BEZ ZAPISU label hold the local proposed MHz.

Alt + left drag near the confirmed line opts into preview without replacing
normal mouse behavior. SPLIT OFF, TX/MOX/TUNE,
disconnect, deadline expiry and a changed confirmed TX frequency clear it.
SPLIT and TX deadlines are checked at input and paint time as well as during
the existing 100 ms readback refresh. Cancelled gestures consume their remaining
move/release events; recovery cannot resurrect the proposal. Wheel/native zoom
is consumed only during this reserved gesture. No radio signals are emitted.
Nothing is stored in settings and there is no write completion callback.

## Offline evidence

`tx_marker_preview_test` drives real SpectrumWidget mouse/key/wheel events,
checks independent rendering and confirmed-frequency preservation, and observes
every widget signal. RX signal routes connect to a real Icom backend with an
injected final UDP writer (no socket). Preview and cancellation must produce
zero datagrams; a normal RX click must produce a datagram as a positive control.
No test connects to a radio. Test settings use TestSettingsProfile.

The dedicated Windows/MSVC/Qt 6.12.0 workflow builds twelve CTest registrations:
the nine Stage 6.1c regression tests, this new GUI test, and the existing full
SpectrumWidget geometry and Retina regressions. It requires all twelve to run
and pass (no skipped tests), records JUnit/logs and pins tested HEAD to GITHUB_SHA.
The real-widget tests explicitly enable AETHER_BUILD_SPECTRUM_GESTURE_TEST;
they are not part of the default build or frozen per-PR gate.

Local Qt compilation/execution is unavailable (Qt6_DIR-NOTFOUND). No local GUI
PASS is claimed. The documentation site dependencies are absent and disk space
is limited; no npm installation/site build was performed. CI results require
independent verification after push; this task does not wait or poll CI.

Stage 6.2a's unfinished changes remain in a named git stash on their original
branch lineage, excluded from this commit. No backend codec, policy, scheduler,
CI-V allowlist, RX/filter/scope/EDGE implementation or TX/PTT path is changed.
Stage 5.2 production, main, PR #1 and FINAL GOOD archives remain untouched.

Local checks completed: `git diff --check`, test registration (strict), frozen
CI gate, and targeted engine-boundary check (strict) passed. The boundary scan
reported only four pre-existing vendor-include warnings. The accessibility
scan reported only two pre-existing zoom-button description warnings.
`icom_civ_test` passed under GCC/C++20; `icom_outbound_policy_test` passed with
11 exact queries and 2,816 rejected added-data mutations. The new Qt GUI test
and Windows workflow have not been executed locally.

Stage 6.2e: Escape and focus/window changes retain the local yellow selection.
An interrupted drag remains consumed until release/new press. A new Alt drag
updates the selection. Unchanged valid readback refreshes deadlines without
invalidating the cached red overlay; invalid/expired data still clears it.
