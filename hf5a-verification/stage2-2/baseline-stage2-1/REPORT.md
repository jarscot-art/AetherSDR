# HF5A etap 2.1 — zachowane wyniki offline

Commit kodu: `f069490fa2c9e1a2153a7490be1ef7fca58196ea`.
[Windows Actions](https://github.com/jarscot-art/AetherSDR/actions/runs/41842733374): SUCCESS.
MSVC 19.51.36260.0, Qt 6.12.0. Końcowe CTest: 14/14 PASS, 4,04 s.
Portable `--self-check`: exit 0 przy PATH ograniczonym do systemu Windows.
[Portable ZIP](https://github.com/jarscot-art/AetherSDR/actions/runs/41842733374/artifacts/13331272272).
SHA256 EXE: `8ce2aefc7090cb0f01dbb93a345dfe5448c9048b9a13b69abc0854c53364479b`.

| Test | Wynik |
|---|---|
| `icom_outbound_policy_test` | PASS |
| `icom_rx_audit_egress_test` | PASS |
| `icom_connection_usage_test` | PASS |
| `icom_rx_audit_busy_test` | PASS |
| `icom_rx_audit_self_check` | PASS |
| `icom_rx_audit_refuses_implicit_connection` | PASS |
| `icom_protocol_test` | PASS |
| `icom_civ_test` | PASS |
| `icom_control_profile_test` | PASS |
| `icom_session_lease_test` | PASS |
| `icom_backend_seam_test` | PASS |
| `backend_family_switch_test` | PASS |
| `transmit_model_test` | PASS |
| `icom_replay_test` | PASS |

Próby z celowym uszkodzeniem zakończyły się oczekiwanym FAIL: usunięcie końcowej
blokady wyjściowej wywołało 7 błędów asercji; ponowne klasyfikowanie własnego
grantu jako Busy — 2. Źródła przywrócono przed końcowymi 14 testami i pakowaniem.

To dowody offline. Następny test sprzętowy zgłoszony przez operatora ujawnił
blokadę inicjalizacji przy conninfo przed autoryzacją; naprawa tego odrębnego
problemu jest przedmiotem etapu 2.2. Ta archiwizacja nie uruchamia radia ani
ponownie etapu 2.1. Surowe logi i ZIP pozostawiono bez normalizacji bajtów.
