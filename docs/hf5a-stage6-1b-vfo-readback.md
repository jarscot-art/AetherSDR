# HF5A stage 6.1b — unselected VFO observations

Base: `393f555c1bcab46ff17b5041585a0ac900fd0e62`.

The IC-7300MK2 backend queues only the existing `25 01` and `26 01` read
builders on its existing session and paced scheduler. Startup reads and a
2-second poll use separate `vfo.unselected.frequency` and
`vfo.unselected.mode` keys. Duplicate requests coalesce. No policy extension,
new session, GUI, slice, write builder or tuning verb is introduced.

`IcomCivBackend::unselectedVfoReadback()` returns independent optional
frequency and mode/DATA/filter values, their monotonic sample timestamps,
status and session/context generation. Values disappear after 5 seconds even
if the service timer has not run. The existing scheduler's 350 ms timeout
invalidates the affected sample immediately. ACK/NG never confirms a value.

Stage 6.1a's strict decoders validate complete addressed replies before they
may complete a scheduler transaction. Only a currently outstanding request in
the current context can establish validity. Foreign, malformed, stale-session,
unmatched and late-after-timeout responses cannot restore observations.
Disconnect/reconnect and identity reset clear values and timestamps. Observed
RX frequency/mode/DATA/filter changes, SPLIT changes and existing TX-context
invalidation withdraw both unselected observations and cancel queued old reads.
The next poll supersedes the previous request generations.

The samples describe **selected/unselected**, not physical A/B. They are not
an atomic snapshot. CI-V has no transaction ID: an old identical response
arriving during a newer identical request cannot always be distinguished.
An unobservable panel swap with identical selected settings cannot be detected
from these replies; the next poll reconciles the relative observations.

The unselected replies do not enter RX frequency/mode/filter or SCOPE handlers,
and do not publish SliceDelta or TransmitDelta. SPLIT/TX readback, ATU reference,
RIT/XIT, PTT, XFC, scope/edge behavior and the existing TX marker retain their
existing paths. There is no hardware test or production deployment.

The dedicated branch-push workflow uses the existing Windows/MSVC/Qt 6.12.0
dependency recipe, builds eight offline test targets and requires all eight to
execute without failure or skip. The new test injects the final writer without
opening sockets. Existing final-writer tests also cover denied writes and
retransmissions. The workflow retains the stage 6.1a integrity check, including
HEAD/GITHUB_SHA equality and only the four exact generated dependency exceptions.
Qt backend tests require that CI environment; no local Qt result is claimed.
