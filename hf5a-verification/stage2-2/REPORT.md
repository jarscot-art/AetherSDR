# HF5A — etap 2.2: naprawa inicjalizacji RX audit

**Windows/MSVC/Qt 6.12.0: kompilacja PASS; końcowe testy 15/15 PASS.**
Nie wykonano połączenia z radiem ani testu sprzętowego.

- Baza: `f069490fa2c9e1a2153a7490be1ef7fca58196ea`.
- Commit kodu: [`48bb8ee78a2229e1035f6162d06f674e09923686`](https://github.com/jarscot-art/AetherSDR/commit/48bb8ee78a2229e1035f6162d06f674e09923686).
- Gałąź: `hf5a/stage2-2-init-fix`.
- [Uruchomienie Windows CI](https://github.com/jarscot-art/AetherSDR/actions/runs/41890486023).
- [Nowy przenośny tester ZIP](https://github.com/jarscot-art/AetherSDR/actions/runs/41890486023/artifacts/13353394302), bez instalatora i wydania.
- SHA256 EXE: `c87f0fad79d739d59572cf933b1a30f83b3a43beec4b126f9941c495733f7e8a`.
- SHA256 archiwum artefaktu według GitHub: `sha256:281f40abd74670f20504c40cd3e151d7a84c4af481e91d9f6d61e1da126f85f2`.
- Retencja artefaktu do 2027-02-02; logi zachowano również w repozytorium.

## Przyczyna i zmiana

W zgłoszonej kolejności conninfo przychodzi po loginie, ale przed akceptacją tokenu.
W bazie klasyfikator zwracał Ignore i pakiet przepadał. Późniejsze
`requestStreamsIfReady()` czekało na `m_auditAvailable`, którego nie było skąd
ustawić bez następnego powiadomienia. Brak żądania strumieni i liczniki odmów
równe zero odpowiadają tej blokadzie automatu, a nie odrzucaniu wysłanego zapytania.

Nowa ścieżka zachowuje jeden poprawny strukturalnie pakiet z bieżącego SID po
przyjęciu skorelowanego loginu. Nie ustawia dostępności. Po uzyskaniu tokenu
i capabilities konsumuje bufor i ponownie sprawdza SID, końcowy AuthId, RadioId,
stan negocjacji i właściciela przez niezmieniony klasyfikator etapu 2.1.
Tylko poprawne Available prowadzi do pojedynczego żądania RX. Busy i niejednoznaczność
nadal kończą próbę. Maksymalny użyteczny wiek bufora to 5 sekund zegara monotonicznego;
duplikaty go nie przedłużają, konflikt zatrzymuje audyt, a stop/nowy login usuwa dane.

Zmiana obejmuje 6 plików implementacji/dokumentacji/workflow, +923/-0 linii względem bazy:

| Plik | Zakres |
|---|---|
| `src/core/backends/icom/IcomSession.cpp` | Korelacja login/capabilities; buforowanie od linii 518; ponowna weryfikacja od linii 590. |
| `src/core/backends/icom/IcomSession.h` | Stan bufora i limit wieku, linie 162–165. |
| `tests/icom_rx_audit_init_test.cpp` | Regresja kolejności zdarzeń i odmów, bez gniazd. |
| `tests/tests.cmake` | Rejestracja nowego testu. |
| `.github/workflows/hf5a-stage2-2-init-offline.yml` | Osobny job z 15 testami, kontrolą mutacji i paczką przenośną. |
| `docs/hf5a-stage2-2-init-fix.md` | Kontrakt bufora i ograniczenia. |

Folder `hf5a-verification/stage2-2` zawiera dodatkowo raport i dowody, w tym
zachowane wyniki bazowego etapu 2.1. Nie są one zmianami kodu testera.

## Rzeczywiście wykonane testy Windows

| Test | Wynik | Czas CTest [s] |
|---|---|---|
| `icom_outbound_policy_test` | PASS | 0.0104247 |
| `icom_rx_audit_egress_test` | PASS | 0.0307512 |
| `icom_connection_usage_test` | PASS | 0.0079003 |
| `icom_rx_audit_busy_test` | PASS | 0.0329298 |
| `icom_rx_audit_init_test` | PASS | 0.0294613 |
| `icom_rx_audit_self_check` | PASS | 0.028784 |
| `icom_rx_audit_refuses_implicit_connection` | PASS | 0.0213766 |
| `icom_protocol_test` | PASS | 0.0087918 |
| `icom_civ_test` | PASS | 0.0084825 |
| `icom_control_profile_test` | PASS | 0.0381893 |
| `icom_session_lease_test` | PASS | 0.0321775 |
| `icom_backend_seam_test` | PASS | 0.145886 |
| `backend_family_switch_test` | PASS | 3.48828 |
| `transmit_model_test` | PASS | 0.0392069 |
| `icom_replay_test` | PASS | 0.0283627 |

Nowy test przechodzi przez rzeczywisty dispatcher handshake/control:
handshake → login → wczesny conninfo → capabilities → token accepted (seq 2)
→ jedno żądanie RX (TX enable=0, TX codec=0) → własny grant. Obejmuje także
token przed capabilities, złe SID/AuthId/RadioId, duplikaty, wygaśnięcie,
rotację tokenu, rzeczywiste Busy, konflikt, brak potwierdzenia i czyszczenie po stop.
Nie utworzono gniazd UDP. Brak argumentów testera jest oczekiwaną odmową (exit 2)
i dlatego odpowiadający mu test CTest ma wynik PASS.

## Dowody skuteczności i paczka

- Celowe uszkodzenie końcowej blokady UDP: oczekiwany **FAIL**, CTest exit 8; log `mutation.log`.
- Celowe uszkodzenie klasyfikacji własnego grantu: oczekiwany **FAIL**, CTest exit 8; log `busy-mutation.log`.
- Celowe uszkodzenie ponownej weryfikacji wczesnego conninfo: oczekiwany **FAIL**, CTest exit 8; log `init-mutation.log`.

Każdą mutację usunięto, zweryfikowano przywrócenie źródła i przebudowano cel testowy
przed końcowymi 15 testami. Celowe FAIL nie są błędami końcowego zestawu.
Dowody: `windows/results.junit.xml`, `results.json`, `ctest.log`, pliki
`*-restoration.json`, `restoration.json` oraz pełny `windows-job.log`.

Paczka zawiera aplikacyjnie dołączone DLL Qt/vendor/OpenSSL/MSVC, `source.json`
oraz względny `SHA256SUMS.txt`. Sprawdzono importy przez dumpbin i kontroler
zależności. Przenośny `--self-check` zakończył się exit 0 przy PATH ograniczonym
do katalogów systemowych Windows; log potwierdza brak sesji i gniazd.
Hash EXE pochodzi z manifestu wygenerowanego w Windows CI. Nie uruchamiano EXE
na lokalnym Windows HF5A.

## Kontrole statyczne i nienaruszone elementy

Rejestracja testów, zamrożona lista CI oraz `git diff --check` kodu: PASS.
Istniejące workflowy, polityka RX-only, klasyfikator właściciela, końcowy writer,
dziewięć dozwolonych zapytań i CLI z wymogiem wyłączności LAN są niezmienione.
CI potwierdziło brak zmian w źródłach po przywróceniu mutacji (exit 0).
Surowe logi/ZIP zachowano jako dowody bez poprawiania ich formatowania.

Main, poprzednie gałęzie i Draft PR #1 pozostają bez zmian. Nie wykonywano merge,
wydania, instalatora, TX/PTT ani operacji na COM4/COM5, produkcyjnym AetherSDR,
HF5A PANEL RADIO, SOOMFON lub FIX EDGE AUTO.

## Pozostałe ograniczenia

- Test offline odtwarza zgłoszoną kolejność, lecz pełna ramka sprzętowa nie została
  dostarczona. Nie potwierdzamy jeszcze interoperacyjności nowego EXE z radiem.
- Limit 5 s jest konserwatywną granicą klienta. Starszy lub niezgodny pakiet wymaga
  świeżego potwierdzenia; bez niego audyt kończy ograniczoną obserwację bez grantu.
  Pakietów nie przepisuje się na nowy token. Sprzeczne wczesne powiadomienia,
  ukryty właściciel i NAT mogą spowodować bezpieczną odmowę.
- Lokalny czas odebrania nie dowodzi czasu wysłania pakietu przez radio. Nadal
  wymagana jest rzeczywista wyłączność LAN i osobne zatwierdzenie testu sprzętowego;
  samo logowanie może wpływać na inną sesję RS-BA1.

Wynik: nowy tester jest przygotowany i zweryfikowany offline. Test sprzętowy
pozostaje odrębnym, niewykonanym etapem wymagającym zatwierdzenia.
