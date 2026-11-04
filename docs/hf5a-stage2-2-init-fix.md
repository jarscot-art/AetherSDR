# HF5A stage 2.2 — early conninfo initialization

Base: `f069490fa2c9e1a2153a7490be1ef7fca58196ea`. The operator's IC-7300MK2 log
shows conninfo before token acceptance. Stage 2.1 discarded it while unauthenticated
and then waited for another owner-free notification before requesting streams.
The radio need not repeat that notification, so initialization could time out
without any outbound-policy denial. The full hardware packet bytes have not been
provided: this fix demonstrates the reported ordering, not all firmware variants.

## Bounded deferral, not provisional permission

Only audit sessions with an accepted, correlated login can retain one structurally
valid 0x90 packet from the current control SIDs. Its raw bytes are kept with a
monotonic arrival timestamp; the maximum useful age is 5 seconds. Keeping the
packet never sets availability, sends a stream request or opens media. AuthId and
RadioId are rechecked against the **final** authenticated/selected session state.

Both token acceptance and capabilities delivery reach the same readiness path.
Once both prerequisites exist, the buffer is consumed exactly once and the
unchanged stage-2.1 classifier performs full SID/AuthId/RadioId/state/owner checks.
Only a confirmed owner-free result can cause the RX-only stream request. Real
Busy and ambiguity still refuse the session. A rotated token never rewrites
cached bytes: old-token data is discarded, while data matching the accepted
rotated token can pass full correlation. Fresh confirmation remains mandatory.

Semantic duplicates may differ in transport sequence fields but do not refresh
age. Conflicting early ownership claims stop the audit, irrespective of order;
there is no last-packet-wins overwrite. Expired or uncorrelated data grants
nothing. A subsequent fresh, correlated notification can proceed. If none
arrives, the existing bounded tester observation ends without opening streams.
Stop/new login clears the buffer, age and login qualification. Late or duplicate
login replies cannot replace the current audit token, and stale capabilities
cannot satisfy the deferred packet's radio correlation.

## Offline verification and portable tool

`icom_rx_audit_init_test` drives the actual IcomStream handshake/payload dispatcher
and final writer without sockets. It covers handshake → login → early conninfo →
capabilities → token accepted (seq 2) → one RX-only stream request → own grant,
plus missing data, expiry, duplicate/conflicting notifications, token rotation,
identifier mismatches and reconnect clearing. Time-expiry tests backdate the
monotonic timestamp; they do not sleep or connect to a radio.

The separate stage-2.2 Windows workflow builds only the tester/test targets,
runs all 14 previous CTest checks and the new initialization regression, and
mutation-checks that removing deferred replay brings the failure back. It
restores source before the final suite and packaging. The portable bundle
contains Qt/vendor/OpenSSL/MSVC DLLs, source provenance and SHA256SUMS.txt;
`--self-check` runs with only Windows system directories in PATH.

The immutable RX-only policy, nine CI-V queries, final UDP guard, TX/PTT blocks,
owner-name/IP checks and exclusive-LAN requirement are unchanged. Existing CI
workflows, main, PR #1 and production programs are not modified. See the included
stage-2 audit instructions for invocation; this change does not authorize hardware
use. No radio connection forms part of its verification.

The 5-second age limit is a conservative client bound, not a documented firmware
deadline. Slow negotiation may require a fresh notification and otherwise stops.
Network-delayed packets do not carry a trustworthy receive timestamp; correlation
and the bounded local cache cannot establish the packet's original emission time.
Hidden owner fields, NAT and unsupported token changes still fail closed. Login
itself can disturb another RS-BA1 client, so the tester must not run alongside
production or be treated as an automatic radio-ownership lock.
