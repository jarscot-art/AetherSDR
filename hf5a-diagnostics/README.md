# HF5A isolated Windows hang diagnostics

Only the helper branch contains this harness and its new workflow. The application
and test sources are checked out separately at
`58f663f7bcda25c54e8f4c158929a6e18ce10011`; no existing workflow is changed.
The dependency recipe is copied from that commit's successful `check-windows`.
MSVC Release flags are retained, with executable linker debug information added
(`/DEBUG:FULL /INCREMENTAL:NO /OPT:REF /OPT:ICF`) for usable local PDB stacks.

1. Build the five requested test targets and dependencies, not the desktop app.
2. Smoke-test the Windows SDK collector against its own sleeping probe.
3. Audit `dumpbin /DEPENDENTS` recursively for all five test executables. Save
   `dumpbin /HEADERS`, PATH, import resolution, vendor DLL inventory and the
   standard app's CMake runtime-copy declarations. API-set contracts are identified
   separately; this static search is evidence, not a complete Windows loader emulator.
4. Run each of the four Qt tests in a separate anchored CTest invocation. Limit
   execution to 60 seconds. At that point, before killing the known test PID,
   save windows/text, a normal minidump, module addresses and every thread's
   native stack. Local PDBs and DLL exports are used without network symbol servers.
   Threads remain suspended until the caller terminates the captured test PID.
   The collector has a separate 90-second budget; its own failure is recorded.
   Neither budget is a larger test timeout. Stdout, stderr, JUnit and a per-case
   result survive independently of subsequent cases.
5. Prepare vendor runtime DLLs in an isolated build directory, prepend it only
   to the diagnostic step/child PATH and repeat the four cases with the same limits.
6. If a prepared feature test still times out, build the unchanged parent commit
   `33975b2354972a83bf7a6378ab2786e17cb49767` on that same runner, using the same
   dependency tree, CMake flags and runtime. CMake/Ninja rebuild affected targets.
   Feature EXEs/PDBs are preserved before baseline checkout. No source patch is applied.

The codec test's previously confirmed PASS is not rerun; it is rebuilt for DLL
inspection only. Missing cases are NOT RUN. A green diagnostic workflow does not
mean all test cases passed: consult the individual results and reviewed report.
The default CMake per-test 300-second property remains untouched; the external
60-second watchdog acts first and captures evidence before termination.

Test and collector processes inherit an explicit minimal environment, excluding
Actions tokens and credentials before any process dump is taken. Dumps omit full
heap memory. Artifacts retain logs, dumps, stacks, executables and PDBs for 90 days;
branches and prior reports must not be deleted without the user's approval.
No hardware, PTT, installer, release, merge or production Windows operation occurs.
