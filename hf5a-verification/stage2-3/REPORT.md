# HF5A etap 2.3 — poprawka przygotowana, CI zablokowane

Stan: 2026-11-05, Europe/Warsaw. Kod: `4f6d0e3026e287edf8f43db580c8f5672d1657ab` (GitHub Verified).
Baza: `48bb8ee78a2229e1035f6162d06f674e09923686`. Gałąź: `hf5a/stage2-3-audit-requests-fix`.

## Wynik i blokada

Poprawka i test regresyjny zostały zapisane oraz wypchnięte wyłącznie na nową
gałąź. Nie ma wyniku kompilacji Windows, wykonanych testów Qt ani nowego EXE/ZIP.
GitHub po push nie utworzył przebiegu ani check-run dla commitu.
Publiczna strona Actions podaje:

> Workflows are not currently being run on this fork due to the scale of GitHub Actions usage.
> A repository maintainer can choose to re-enable these.

Dowód: `actions-block.json` i skompresowana oryginalna strona
`github-actions-page.html.gz`. Nie omijano ograniczenia, nie uruchamiano starego
joba jako zastępczego dowodu. Wyniki poprzednich etapów nie są wynikami tej poprawki.

**Krok właściciela:** otworzyć https://github.com/jarscot-art/AetherSDR/actions
po zalogowaniu jako jarscot-art i użyć opcji ponownego włączenia workflowów
przy tym komunikacie. Następnie potwierdzić odblokowanie. Trzeba wznowić
pomocniczy workflow na gałęzi etapu 2.3, bez zmian main i starych workflowów.
Jeżeli GitHub nie udostępni tej opcji, ograniczenie wymaga obsługi po stronie
GitHub; samo zwiększenie timeoutu lub ponowny build lokalny go nie usuwa.

## Źródło odmów

Pięć wejść licznika w `src/core/backends/icom/IcomCivBackend.cpp`:

| Wejście | Linia | Zachowanie |
|---|---:|---|
| `queueRead` | 4499 | odmowa zapytania spoza polityki |
| `queueWrite` | 4534 | odmowa zapisu |
| `queueEmergencyWriteNoReply` | 4564 | odmowa awaryjnego zapisu |
| `sendUserCommand` | 5097 | odmowa komendy użytkownika |
| `invokeExtension` | 7623 | odmowa rozszerzenia |

Wszystkie inkrementacje i pole licznika pozostają bez zmian. Nie ma resetowania
ani maskowania odmów.

Przyjęcie `26 00` wywoływało `requestPassbandState()` (linia 876): odczyty
`1A 03`, `14 07`, `14 08`. Zmiana DATA dodawała odczyt `1A 05` pozycji 17.
Audyt odmawiał tych odczytów, więc brak potwierdzonej szerokości wymuszał
kolejne trzy próby przy następnym raporcie. Z kodu wynika scenariusz
11 odpowiedzi USB-D po USB → `11 × 3 + 1 = 34` odmowy.
**To analiza kodu i przygotowany scenariusz regresji, jeszcze nie wynik jego
wykonania. Bez pełnego historycznego logu nie przypisujemy wszystkich 34 odmów
sprzętowych do konkretnych ramek.**

## Zmiany

5 plików, 802 dodane i 5 usuniętych linii względem bazy (bez tego raportu):

- `src/core/backends/icom/IcomCivBackend.cpp`: 12+/5−. Audyt pomija automatyczny
  inwentarz filtra/PBT/TBW po `26 00` (3414), odczyt TBW po `16 58` (3234),
  odczyt preampu przy zmianie zakresu (2844), scope-follow (2037), cleanup
  CW/PTT przy konflikcie tożsamości (1432/1440), rozszerzony XFC po PTT (4010).
  Wartości mode/DATA/filter i unieważnienie starego odczytu TX są zachowane.
  Warunki produkcyjne pozostają równoważne dotychczasowym.
- `tests/icom_rx_audit_requests_test.cpp`: 184 linie. 11 raportów USB-D,
  kontrola produkcyjnych zapytań filtra/PBT/TBW, rzeczywisty decoder CI-V,
  inicjalizacja, 20 cykli, RIT/XIT, niezamówiony TBW/scope, odczyt SPLIT/TX,
  odzyskiwanie schedulera po timeoutach i zamknięcie. Przechwyt bajtów na
  finalnym writerze, bez socketów. Pięć jawnych niedozwolonych operacji ma
  podnieść licznik do 5; dalsze zwykłe odczyty i disconnect go nie kasują.
- `tests/tests.cmake`: 7 linii; rejestracja oraz izolowany profil ustawień.
- `.github/workflows/hf5a-stage2-3-requests-offline.yml`: 556 linii. Nowy helper
  tylko na nowej gałęzi; receptura zależności poprzedniego Windows joba,
  Qt 6.12.0/MSVC, pełny target AetherSDR, tester i 16 testów pakietu HF5A.
  Cztery zaplanowane mutacje: writer guard, Busy, conninfo init oraz ponowne
  generowanie 34 odmów. Każda ma być cofnięta bajtowo przed finalnymi testami.
  ZIP testera z DLL, dumpbin, sprawdzeniem zależności, SHA256SUMS i self-check
  przy minimalnym PATH. Logi/JUnit jako artefakty 90 dni.
- `docs/hf5a-stage2-3-audit-requests-fix.md`: 43 linie dokumentacji i ograniczeń.

Nie zmieniono listy dziewięciu zapytań (`19 00`, `0F`, `1C 03`, `03`, `04`,
`26 00`, `21 00`, `21 01`, `21 02`), polityki, IcomStream, IcomSession,
Busy, wyłączności LAN, GUI ani źródeł testów poprzednich etapów.

## Kontrole i status testów

PASS: `git diff --check`, `check_test_registration.py --strict`,
`check_ci_test_gate.py`, parser YAML i zgodność wyboru 16 testów, porównanie
bajtów polityki/writera/sesji i wszystkich istniejących workflowów z bazą.
Zamrożona bramka nadal zawiera 7 testów. Szczegóły: `static-checks.json`.

Kompilacja Windows/MSVC/Qt 6.12.0: **NOT RUN**.
Mutacje: **NOT RUN**. Portable self-check: **NOT RUN**.
SHA256 EXE/ZIP: **brak — nowe artefakty nie powstały**.

| Test | Wynik tej wersji |
|---|---|
| `icom_rx_audit_requests_test` | NOT RUN |
| `icom_rx_audit_init_test` | NOT RUN |
| `icom_connection_usage_test` | NOT RUN |
| `icom_rx_audit_busy_test` | NOT RUN |
| `icom_outbound_policy_test` | NOT RUN |
| `icom_rx_audit_egress_test` | NOT RUN |
| `icom_protocol_test` | NOT RUN |
| `icom_replay_test` | NOT RUN |
| `icom_session_lease_test` | NOT RUN |
| `icom_civ_test` | NOT RUN |
| `icom_control_profile_test` | NOT RUN |
| `icom_backend_seam_test` | NOT RUN |
| `transmit_model_test` | NOT RUN |
| `backend_family_switch_test` | NOT RUN |
| `icom_rx_audit_self_check` | NOT RUN |
| `icom_rx_audit_refuses_implicit_connection` | NOT RUN |

Zestaw 16 obejmuje dotychczasowe 15 kontroli HF5A i nową regresję.
Nie jest to cały zestaw repozytorium (923 zarejestrowane testy);
pozostałych testów również nie wykonano.

## Zachowane granice i ryzyka

Main `33975b2354972a83bf7a6378ab2786e17cb49767`, Draft PR #1 i etap 1
`58f663f7bcda25c54e8f4c158929a6e18ce10011`, etap 2.2
`6c637925b196c8cd0975a95ac7695ea1aa6bc8ce` pozostały nienaruszone.
Dowód: `protected-refs.json`. Bez merge, wydania, instalatora, instalacji Qt,
połączenia z radiem, ingerencji w produkcję, COM4/COM5 i programy HF5A.

Pozostaje potwierdzenie kompilacji nowego testu, działania jego asercji,
odtworzenia 34 odmów przez mutację oraz wszystkich wyników na Windows.
Analiza statyczna nie zastępuje tych dowodów. Nie uznajemy kodu ani nowego
portable za gotowe do próby sprzętowej. Zachowano gałąź i raport; wznowienie
CI wymaga odblokowania Actions przez właściciela.
