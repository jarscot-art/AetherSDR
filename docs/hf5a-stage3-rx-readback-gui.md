# HF5A stage 3 — RX-only readback GUI

Base: `2bfd9ebd36409ed5d7783ff534eee244ccc4ba04`.
Branch: `hf5a/stage3-rx-readback-gui`.

The existing desktop receives an informational status strip and a passive
**TX readback** line on its existing spectrum frequency coordinates. The line
has no hit region, drag handler or tuning action. Other backends retain their
existing behavior. Neither readback presentation nor its expiry timer sends
radio commands or creates a session.

SPLIT is independently confirmed by `0F`; DUP replies cannot become SPLIT.
TX is independently reported by `1C 03`, not inferred from VFO B or SPLIT.
RX dial is the strictly validated selected-dial `03`/`00` report. The displayed
**TX − RX dial** excludes RIT; it is not a claim about the RIT-shifted RX passband.
Samples are independent, not an atomic radio snapshot. Existing RIT/XIT handling
and the legacy ATU/KAT500 frequency reference are unchanged.

Values require their own validity and absolute monotonic deadline. No defaults
are displayed as confirmed OFF/zero. Missing, expired, rejected or disconnected
data withdraw the affected value and derived difference. A receive-context change
invalidates old TX. A queued sample cannot restart its lifetime. Presentation
refreshes every 100 ms without polling the radio; painting checks the TX deadline.

## Isolated portable GUI

The helper workflow builds Windows/MSVC/Qt 6.12.0 with
`AETHER_RX_READBACK_GUI_ONLY=ON`. This compile-time entry selection prevents the
portable binary from entering normal desktop startup, autoconnect, bridges,
controllers or production settings. It uses the existing audited runner with
one immutable RX-only backend and one session. The nine-query allowlist, final
UDP writer, Busy and exclusive-LAN checks are unchanged.

The portable `aether-icom-rx-readback-gui.exe` contains the shared readback strip
and existing SpectrumWidget, with user tuning interactions disabled and unwired.
The scale is centered visually on confirmed RX dial with a 20 kHz span. Out-of-range
TX is still shown numerically, never falsely clamped to an edge. No scope enabling
or data-output writes are permitted, so this audit viewer does **not** claim to
show a live FFT/waterfall. It shows a frequency scale and confirmed markers only.

The normal AetherSDR desktop layout and GUI startup remain the default build;
the isolated portable is deliberately a small read-only view of the same widgets.
Do not replace a production EXE with this package. No installer is produced.

Offline check: `aether-icom-rx-readback-gui.exe --self-check`.
No-argument launch refuses to connect. Future hardware use requires separate
approval, `--connect --exclusive-lan-confirmed --host <existing-IP>` and the
same bounded `--credentials <private-JSON>`/`--seconds` options as stage 2.
No second LAN client may be running. Do not change COM4/COM5 or LAN configuration.
Closing the window tears down the same session and returns failure for an
incomplete observation; the observation limit remains
5..120 seconds. Each launch uses a temporary settings directory.

## Verification

The stage 3 helper retains the 16 stage 2 checks and four guard mutations, and
adds the GUI/model/decoder/marker regression plus isolated GUI self-check and
implicit-connection refusal (19 CTest entries). It builds the existing desktop
sources with the isolated entry. The default production entry is not executed.
The GUI regression covers OFF/ON, both signs of the frequency difference,
unknown/expired/disconnected state, delayed samples, malformed RX, DUP, old
session generations, real scheduler timeout, legacy ATU reference, painted
marker pixels, out-of-range markers and actual final-writer bytes.

Packaging checks every imported DLL and runs the GUI self-check with a minimal
PATH, then records EXE hashes, SHA256SUMS, JUnit and source-integrity evidence.
This is offline verification; no connection to an IC-7300MK2 is performed.
