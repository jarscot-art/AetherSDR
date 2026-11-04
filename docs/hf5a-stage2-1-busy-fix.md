# HF5A stage 2.1 — correlated RS-BA1 ownership

Base: `11b1480d6270cd2942c767c11046543735a8fbfe`. Hardware use still requires
separate operator approval and an exclusive LAN session. This build does not
replace production AetherSDR or change COM4/COM5. See
`hf5a-stage2-rx-only-audit.md` for the unchanged invocation and RX-only policy.

## Cause and correction

The `0x90` connection-usage packet reports **busy even for our newly granted
streams**. The previous audit rejected it before correlating the session.
The production parser's byte-0x60 heuristic remains for production compatibility;
the audit path now uses one strict, contextual classification and never falls
through to that heuristic. No changes to `IcomOutboundPolicy`, allowed CI-V
queries, final outbound guard, TX, PTT, scope, RIT/XIT or FIX EDGE are included.

The wire evidence is the public MIT kappanhang `controlstream.go` conninfo example
(144-byte reply: inner length 128, reply 03/type 00, busy LE32=1, owner `icom-pc`
and owner IPv4), and wfview's published conninfo field layout. wfview is used
only as a read-only protocol reference; no implementation is copied.

Audit accepts a notification only with current control SIDs, current AuthId and
selected RadioId after authentication. A stale or uncorrelated notification
cannot claim streams or report Busy. Current malformed notifications fail closed.
Before sending the stream request, audit waits for a correlated owner-free
notification (busy=0, empty owner and zero owner IP). It sends the request once.

The login owner label is fresh per login (`HF5A-` plus ten random hex digits,
15 bytes). A subsequent busy=1 is our grant only if its owner label and IPv4 match
this login and the connected UDP socket's routed local IPv4, after the availability
check and stream request. Different owner/IP means Busy; missing owner/IP or
an unsolicited own-looking grant means ambiguous and stops the audit. AuthId is
never adopted from a conninfo response. Duplicates do not reopen media. An
owner-free notification after grant closes the session; stop clears all ownership
evidence. Production login still uses `icom-pc`.

## Verification and limitations

`icom_connection_usage_test` exercises the strict classifier without Qt.
`icom_rx_audit_busy_test` injects actual incoming bytes through IcomSession and
observes the final writer without binding any sockets. Existing twelve checks,
including the immutable policy/egress tests, run alongside these two on Windows
MSVC/Qt 6.12.0 in a separate workflow. The workflow also deliberately restores
the old blanket-Busy error in a temporary CI worktree, requires the regression
test to fail, then restores exact source before the final suite/package.

The portable artifact includes app-local Qt/vendor/OpenSSL/MSVC DLLs, source
provenance and a relative SHA256 manifest. `--self-check` is run with only Windows
system directories in PATH. It does not open a session. No hardware check is
performed by CI or by this change.

This is correlation, not cryptographic authentication of UDP ownership. It does
not permit concurrent operation with production. Login itself may affect an
existing radio session. NAT, a hidden owner, an unexpected token rotation in
conninfo, or firmware which does not echo the login label will prevent a grant.
Missing/reordered availability can cause the bounded observation to expire;
there is no fallback that claims the radio without confirmation. In particular,
offline fixtures are not a capture of HF5A's failed hardware run: the overlapping
parser defect is proven, but that run's full packet was not supplied. Hardware
compatibility of the new strict ownership checks needs separately approved testing.
