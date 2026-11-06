# HF5A stage 6.1c — unselected VFO presentation

Base: `f08e91a4d1b4f225aacea0d42bbfa8934143e847`.

The existing stage 6.1b scheduler, codec and final UDP policy remain authoritative.
There are no new reads, writes, sessions, timers for polling, or controls. The
relative unselected VFO is not physical B and is not the TX frequency.

## Data path and lifetime

`IcomCivBackend` publishes an additive `UnselectedVfoObservation` through the
existing `IRadioBackend::transmitChanged` / `TransmitDelta` seam. `RadioModel`
retains its backend-generation fence. `TransmitModel` stores this observation
independently of TX, RX dial and the legacy ATU/KAT500 reference. No new signal
or GUI include of an Icom backend is needed.

A coalesced, QObject-lifetime-bound callback publishes the current backend state
after scheduler mutation. It captures no old sample: context changes, disconnect
and reconnect before delivery cannot replay the old snapshot. Frequency and mode
carry separate absolute monotonic deadlines computed from the original backend
sample ages, never from delivery time. Expired/invalid snapshots fail closed in
the model; mode, DATA and filter validity is atomic. Teardown and legacy backend
replacement clear all independent provenance. A deadline alone cannot distinguish
an identical delayed CI-V reply; stage 6.1b's documented protocol limitation remains.

`RxReadbackWidget` adds a second informational line to the existing double-height
status panel. Its existing 100 ms refresh checks deadlines even in radio silence.
The SPLIT/TX line and SpectrumWidget marker wiring are unchanged. ThemeManager
supplies both normal and unavailable text colors; accessibility names, tooltips
and value-change notifications describe the relative VFO and its limitations.

## Offline verification

The dedicated Windows/MSVC/Qt 6.12.0 workflow requires exactly nine tests:
`rx_readback_gui_test`, `icom_unselected_vfo_test`, `icom_civ_test`,
`icom_outbound_policy_test`, `icom_rx_audit_egress_test`,
`icom_rx_audit_requests_test`, `icom_backend_seam_test`, `transmit_model_test`,
`backend_family_switch_test`. Inventory and JUnit must report nine executed passes,
with no skipped tests. Logs and source integrity evidence are uploaded even on
failure. The frozen CI gate and prior workflows are unchanged.

Model/GUI regressions cover initial unknown values, frequency versus mode expiry,
DATA and filter invalidation, late queued delivery, context changes, disconnect,
backend replacement, theme updates, independent TX/RX/ATU, and zero control intents.
The real codec-to-model-to-GUI test uses an injected terminal UDP writer, without
opening a socket. Existing egress tests cover allowed reads, forbidden writes and
retransmissions. There is no hardware test, production installation or deployment.

Qt model/GUI tests require CI; local static checks do not constitute their PASS.

## Local checks before push

- PASS: `git diff --check`, strict test registration and the unchanged frozen CI gate.
- PASS: strict engine boundary (eight touched source files), accessibility checks,
  and theme seed consistency.
- PASS: YAML parsing, branch/Qt pin, nine registered targets and required JUnit results.
- PASS (Linux/GCC, C++20): `icom_civ_test` and `icom_outbound_policy_test`;
  the policy test retains 11 exact queries and rejects 2816 added-data mutations.
- Not executed locally: Qt model/backend/GUI tests and the documentation site build.
  Qt is unavailable locally; no Qt or system libraries were installed. The Windows
  workflow runs the nine required tests; no CI outcome is claimed before it runs.
