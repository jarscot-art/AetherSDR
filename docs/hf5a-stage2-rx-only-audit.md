# HF5A stage 2: RX-only audit

The stage-1 SPLIT/TX sources are based on `58f663f7bcda25c54e8f4c158929a6e18ce10011`.
This opt-in diagnostic does not replace the production executable or change its
settings, COM4/COM5, HF5A PANEL RADIO, CW accessories, AetherKnobs or FIX EDGE AUTO.
Hardware use requires separate operator approval. No hardware test is part of
the stage-2 offline verification.

## Portable tool

Configure `AETHER_BUILD_ICOM_RX_AUDIT=ON` and build `aether-icom-rx-audit`.
It links the existing engine and uses QCoreApplication, without GUI, automation
server, discovery, microphone, device startup, proxy or helper process. Include
the same Qt and vendor runtime DLLs required by the Windows test executables.
Building the desktop app is not required.

`aether-icom-rx-audit --help` and `aether-icom-rx-audit --self-check` do not create
a session or sockets. With no arguments it refuses to connect (exit 2).

Future hardware invocation requires BOTH `--connect` and
`--exclusive-lan-confirmed`, an explicit IPv4 `--host`, and a private JSON
`--credentials` file containing string fields `username` and `password`.
Credentials are neither passed on the command line nor written to the report.
Existing UDP ports can be supplied with `--control-port`, `--civ-port` and
`--audio-port`; these are not Windows COM ports. No radio network setting is
changed. Observation is bounded by `--seconds` (5..120, default 30).

The confirmation flag is an operator acknowledgement, not automatic detection
of exclusive radio ownership. A second RS-BA1 login can interfere with an
existing client even when CI-V writes and TX audio are blocked. Do not connect
while production ownership must remain uninterrupted. A Busy notification is
refused, but detection can occur after login; it cannot guarantee coexistence.
There is no automatic reconnect or CI-V pipe restart in the audit path.

The tool writes JSON snapshots with SPLIT/TX readback validity, age, historical
values, confirmed TX MHz and denied-request/datagram counts. It uses an ephemeral
settings directory. Exit 0 means both readbacks are fresh at the end and no
outbound request was denied; 1 means incomplete/failed observation, 2 means
refused invocation, and 3 means connection failure. A radio-reported TX frequency
in RX is not an RF measurement. Original CI-V freshness/correlation limitations
remain: replies have no transaction IDs and SPLIT/TX are not atomic snapshots.

## Immutable policy and last writer

`IcomOutboundPolicy` defaults to Deny. Existing production constructors retain
their explicit production-policy default for compatibility; the audit entry
constructs an immutable RxAudit policy and the session factory passes it to
every stream. No active-session policy setter exists.

Only exact `FE FE B6 E0 <query> FD` frames are allowed:
`19 00`, `0F`, `1C 03`, `03`, `04`, `26 00`, `21 00`, `21 01`, `21 02`.
Before a directed MK2 identity reply, the stream permits only `19 00`.
Extra data, changed addresses, FC, concatenation and unknown commands are denied.
PTT OFF, CW abort and other cleanup writes have no bypass exception.

All actual UDP writes go through `IcomStream::sendRaw`, including stop and
retained retransmits. The policy runs before the injected test writer or socket.
Tracked payloads are also checked before sequence allocation and retention.
The registered control-packet templates come only from IcomSession's private
canonical login/auth/stream-request writer, are bounded, and are checked again
against the stream role, current SIDs, lengths and RX-only negotiation bits.
Packet type zero alone never grants permission: it carries transport, CI-V and
audio. The audio stream can send maintenance, never payload data (even silence).

Backend audit mode suppresses scope initialization, broad meter/control polling,
hardware cleanup writes and write-capable extensions. It retains the existing
CI-V scheduler, SPLIT/TX decoder, readback invalidation and TransmitModel.
`enableTx=false`, absent negotiated TX codec, no TX timer and early audio refusal
are additional safeguards; the final datagram check is independent of them and
of any TxCoordinator grant. Production behavior and other backends retain their
existing authority.

## Offline verification

`icom_outbound_policy_test` validates exact queries, transport boundaries and
2304 added-data mutations. `icom_rx_audit_egress_test` injects the last writer
without binding any sockets, exercises canonical session setup, identification,
raw/tracked/authorized-TX entry points, forced retained forbidden packets, SID
changes, Busy, scope suppression and teardown. Its capture is behind the actual
policy, so a denied packet appearing there fails the test.

The dedicated Windows workflow builds these targets and relevant production
regressions using the existing check-windows dependency recipe, stages the
vendor DLLs explicitly and runs CTest offline. It additionally removes the final
writer guard temporarily on the runner: the egress test must then fail. The
source is restored byte-for-byte and rebuilt before the final verification.
Existing workflows and the frozen CI test gate are not modified.
