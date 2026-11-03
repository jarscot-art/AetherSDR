# Icom capability profiles

RFC issue #4984 defines model-specific CI-V command profiles. The implementation
lives entirely below the radio seam in `src/core/backends/icom/IcomModels.*`.
`RadioCapabilities` remains the backend-neutral publication contract; FlexRadio,
HL2, Sim, and their UI behavior do not consult these profiles.

## Identity versus command capability

`IcomModel` contains identity and transport geometry: CI-V address, receiver and
VFO counts, RS-BA1 availability, scope geometry, frequency-byte width, tuning
range, and PA ceiling. `IcomModelProfile` contains command-table facts. Keeping
them separate matters because evidence is often narrow: the live IC-9700 trace
attests its `26 00` shape and repeater registers without proving every scope and
meter fact for that model.

An absent model-specific profile facet means unsupported for code routed through
that facet. Two compatibility floors deliberately remain reachable without
profile evidence: model-neutral Core CI-V controls, and Scope controls when the
discovered identity declares scope geometry. `controls map` reports that split
as `supported: true` with `profileEvidence: none`; reachability is not an
attestation. The backend must not borrow another radio's SET address, enum,
meter curve, front-end ladder, mode vocabulary, or model-specific command
shape. Identity-only rows remain discoverable but do not receive a supported
bring-up profile.

## Initial supported profiles

| Difference | IC-705 | IC-7300MK2 | IC-9700 |
|---|---|---|---|
| CI-V address | `A4` | `B6` | `A2` |
| Network | Wi-Fi | Ethernet | Ethernet |
| `26 00` mode/DATA/filter | Official guide + live | Official guide | Live radio |
| MOD inputs | SET `0116`-`0119`, WLAN=`03` | SET `0081`-`0085`, LAN=`05`, includes ACC | Not attested |
| VOX delay | SET `0359` | SET `0267` | Not attested |
| CI-V Transceive | SET `0131` | SET `0089` | Not attested |
| SSB TX bandwidth | SET `0019`-`0022`, four low edges | SET `0014`-`0017`, six low edges | Not attested |
| Scope modes | Center | Center, Fixed, Scroll-C, Scroll-F; sweep speed | Center measured; other modes not attested |
| Mode vocabulary | Includes WFM; WFM is RX-only | Not yet published | Not yet published |
| Front end | OFF/P.AMP1/P.AMP2; OFF/20 dB | Same documented ladders | Not attested |
| Power/current calibration | IC-705 portable curves, 4 A face | IC-7300MK2 desktop curves, 25 A face | Power remains percent; Vd/Id not claimed |
| CW text keyer | Command `17` | Command `17` | Not attested |
| RX antenna | None | Selectable; live firmware returns ACK without readback | Not attested |
| RF decks | Continuous envelope | Continuous envelope | Three discontinuous decks with 100/75/10 W ceilings |
| FM repeater | Extended registers official-guide; basic tone/level/offset/XFC live-proved | Tone + TSQL, no DTCS claim | Extended registers official-guide + live-proved |
| CI-V data restart | Not enabled | Not enabled | `0x04` data-start recovery, three attempts at 1 s; public implementation + physical watchdog evidence |
| GPS position | `23 00/01`, official guide + live-proved | Not attested | Not attested |
| GPS/NTP clock | SET `0167`-`0169` plus `1A 07/08`, official guide + live-proved | Not attested | Not attested |

The FM row deliberately corrects the assumption in the original IC-9700 PR
that the repeater family must be hidden on IC-705. The IC-705 guide documents
`16 5D` and `1B 00/01/02`, and live hardware proves the basic tone, tone level,
offset, and XFC treatment. The IC-7300MK2 guide documents tone and tone-squelch
but not the DTCS combinations, so its profile is narrower.

Documented command coverage and activated runtime traffic remain separate
facts. `FmRepeaterExtendedReadback` activates `16 5D`, `1B 01`, `1B 02`, and
`1C 03` for the IC-705 and IC-9700 because each model's official CI-V guide
defines those registers independently. The IC-9700 also carries preserved live
trace evidence; the IC-705 extended surface remains guide-proved until an
operator pass exercises it on hardware.

## Effective control registry

Each `ControlSpec` names the `IcomFeature` it requires. `controls map` returns
every declared row plus these active-profile fields:

- `supported`
- `profileFeature`
- `profileEvidence`
- `profileSource`

Unsupported rows are not credited as sent or seen and are excluded from
`controls scrub`. This makes the report model-specific without deleting useful
declarations for features that are implemented but unavailable on the active
radio.

The backend's read-only `profile.show` extension returns the active model, guide
revision, SET-item differences, per-feature evidence, FM repeater access modes,
RX-antenna readback behavior, and the scope-command and GPS facets.
It contains no credentials. `controls map` is the currently public automation
surface; routing the RFC's proposed `icom profile show` bridge spelling remains
separate automation work so this foundation does not cross the radio seam.

## Adding another Icom

1. Add or verify the identity row from the model-specific official CI-V guide.
2. Add an `IcomModelProfile`; do not copy an existing profile wholesale.
3. Record evidence independently for each command family.
4. Leave every unverified facet absent.
5. Add focused fixtures for every SET address, enum, range, and quirk.
6. Confirm `controls map` reports the intended effective surface before live
   testing.
7. Prove radio state, application state, and restart convergence. Never enable
   CI-V Transceive or CI-V Output (for ANT) silently.

## IC-7300MK2 SPLIT and native TX readback

`SplitTxReadbackProfile` activates read-only `0F` and `1C 03` for the MK2.
Its authority is the [IC-7300MK2 CI-V Reference Guide](https://icomuk.co.uk/files/icom/PDF/productAdditionalFile/IC-7300MK2_ENG_CI-V_0.pdf),
PDF pages 4, 14 and 16. A bare `0F` queries SPLIT; its single reply data byte
is `00` (off) or `01` (on). It remains a non-subcommand frame in the codec.
FM DUP `10`/`11`/`12` is a separate model dialect and is never used on the MK2.
`1C 03` is read-only and returns five little-endian BCD frequency bytes.
No SPLIT write, VFO operation, XFC write, PTT, CW or tuning operation is added.
Neither CI-V Transceive nor CI-V Output (for ANT) is enabled by these reads.

The backend queues both reads at startup and nominally every 2000 ms through
the existing scheduler, with its existing pacing, coalescing and priorities.
Only identified-radio replies (controller or broadcast destination), with a normal
`FD` terminator, exact shape, BCD and model-frequency bounds, establish evidence.
The codec retains the distinction from `FC` without changing legacy decoders.
Valid broadcast reports are consumed when already enabled by the operator. A generic `FB` is
not a value; `FA`, a timed-out current attempt, or age beyond 5000 ms invalidates
the corresponding evidence. A newer report protects against an older attempt's
timeout. Expiry notifications are serviced by the existing command-plane pump,
not a new timer. An absolute monotonic `transmitFreqDeadline` travels with native
TX deltas; both model getters of validity check it directly. A delayed queued
publication cannot restart the freshness budget or survive a delayed pump.

SPLIT, dial/mode, RIT/XIT and observed PTT/XFC changes invalidate the previous
TX context and queue a new TX read; the existing RIT/XIT register handling is
unchanged. Context-refresh reads supersede older scheduled reads. Session and
identity changes clear observations. CI-V has no transaction IDs: a delayed
reply indistinguishable from a current reply cannot be proved to belong to an
older physical operation. The result is a bounded-age radio report, not an
atomic snapshot or permission to transmit. The GUI and its calculated XIT
marker are unchanged by this backend-only feature.

### Compatibility with ATU frequency consumers

`TransmitDelta::transmitFreq` carries the actual MK2 `1C 03` value in MHz,
with `transmitFreqValid=true` in the same delta. No RX or tuner decoder writes
that native TX value. `TransmitModel::confirmedTransmitFreq()` returns an
optional qualified value and becomes empty on invalidation. An unrelated
partial delta preserves the observation; a frequency-only legacy delta clears
its explicit provenance. Other backends keep their current publications.

`TransmitModel::transmitFreq()` is deliberately retained as a compatibility
reference, not promoted to a freshness-qualified API. Existing `TxApplet`,
`AtuPreTuneDialog` and `Kat500TxController` use it for ATU matching and carrier
context. For MK2, `TransmitDelta::legacyTransmitFreq` preserves their established
selected-dial reference while actual TX is tracked independently. Even a native
TX reply arriving before the first RX reply cannot initialize or overwrite the
legacy reference. Invalidating TX does not set this reference to zero, and TX polling cannot oscillate it
between the RX and TX VFOs. ATU status and its reference still arrive together,
without waiting for TX readback: the status also completes a local ATU intent.
These legacy consumers do not acquire SPLIT-aware matching in this change.
Backend replacement clears explicit provenance and the separate native
observation, preserving the legacy reference's existing numeric contract.

Unit coverage uses injected receive frames and scheduler time, without radio
sockets. Compilation, executable tests and physical MK2 convergence must be
verified separately; protocol documentation alone is not hardware proof.
