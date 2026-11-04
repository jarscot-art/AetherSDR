# HF5A — etap 2: RX-only, implementacja i dowody offline

Wynik: implementacja RX-only gotowa do osobnego zatwierdzenia testu sprzętowego.
Kompilacja Windows/MSVC/Qt 6.12.0 PASS; 12/12 testów offline PASS; przenośny
EXE po uzupełnieniu DLL OpenSSL PASS. Nie wykonano testu na radiu.

Repozytorium: `jarscot-art/AetherSDR`. Gałąź: `hf5a/stage2-rx-only-audit`.
Punkt wyjścia: `58f663f7bcda25c54e8f4c158929a6e18ce10011`.
Ostatnia kompilacja i wykonanie 12 testów: `b058dcc2b2d6365f2ffed78462745830fc8ff85e`.
Wersja pomocniczych workflowów po poprawce pakowania:
`88370a51314c7ab2fd8b306f9775d29173ec1792`.
Źródła C++, testy i CMake w tych rewizjach są identyczne z `31b6b418`;
po tej rewizji zmieniano wyłącznie nowe pomocnicze workflowy.
Zmiana implementacji względem punktu wyjścia: **16 plików, +1666 / −37 linii**.
Materiały dowodowe w `hf5a-verification/stage2/` są liczone osobno: 162 pliki
archiwum i raportu. Łącznie etap 2 obejmuje 178 plików, w tym 16 plików
implementacji, testów, dokumentacji i pomocniczych workflowów.

## Co zostało zabezpieczone

`IcomOutboundPolicy` bez jawnego wyboru trybu odmawia wszystkiego. Narzędzie
audytu tworzy wyłącznie politykę `rxAudit()`. Backend i sesja przechowują ją jako
`const`; fabryka sesji przekazuje tę samą politykę każdemu z trzech strumieni.
Brak metody przełączającej politykę podczas połączenia. Dotychczasowe konstruktory
produkcyjne zachowują jawny domyślny tryb `production()` dla kompatybilności.

Dokładna lista CI-V to `19 00`, `0F`, `1C 03`, `03`, `04`, `26 00`, `21 00`,
`21 01`, `21 02`, w ramce `FE FE B6 E0 ... FD`. Przed skierowaną odpowiedzią
identyfikującą IC-7300MK2 wolno wysłać tylko `19 00`. Dodatkowy bajt zmienia
zapytanie w niedozwoloną ramkę; przykładowo `0F 01` jest odrzucane. Inny adres,
nieprawidłowe zakończenie i sklejone komendy również są odrzucane.

Jedyny rzeczywisty zapis UDP znajduje się w `IcomStream::sendRaw`, za kontrolą
polityki i przed wstrzykniętym writerem testowym. `stop()` i retransmisje
korzystają z tej samej funkcji. Weryfikacja przed kolejkowaniem zapobiega także
zajmowaniu numerów sekwencyjnych i bufora retransmisji przez odrzucone dane.
Wymuszone wpisanie zabronionego pakietu do bufora nadal nie omija końcowej blokady.

Dozwolony transport jest ograniczony do własnych SID i właściwej roli strumienia:
handshake, ping, IDLE, pojedyncze żądanie retransmisji, rozłączenie oraz otwarcie
własnego kanału CI-V. Logowanie, autoryzacja/odnowienie/deautoryzacja i negocjacja
strumieni wymagają zarejestrowanego przez prywatną ścieżkę sesji wzorca z buildera.
Typ pakietu zero sam w sobie nie daje uprawnień. Każdy payload kanału audio jest
odrzucany, także cisza i pakiet z ważnym grantem TX.

Negocjacja wymusza `enableTx=false`; istniejący builder koduje `txenable=0`
i `txCodec=0`. Nie powstaje timer TX. Zablokowano pompowanie i padding audio,
restart CI-V, wake, zapisy scope i sterujące rozszerzenia. Audit nie wykonuje
produkcyjnych zapisów porządkowych CW-abort/PTT-OFF przy rozłączeniu. Także PTT-OFF
nie jest wyjątkiem od polityki. Nie zmieniono algorytmów RIT/XIT, FIX EDGE,
GUI ani dekodera/ważności odczytów SPLIT/TX etapu 1. Audit pomija ramki scope
i szerokie odpytywanie produkcyjne; używa istniejącego schedulera do wskazanych
odczytów, okresowo co 2 sekundy.

Obsługa błędu w testerze zamyka sesję przez kolejkę zdarzeń, aby nie usuwać jej
wewnątrz własnego callbacku błędu. Ścieżka inicjalizacji odczytów sprawdza także,
czy obserwator sygnału nie usunął backendu przed kolejnym dispatch.

## Narzędzie przenośne

`aether-icom-rx-audit` jest celem opcjonalnym (`AETHER_BUILD_ICOM_RX_AUDIT=ON`,
domyślnie OFF). Używa `QCoreApplication` oraz istniejącego silnika; nie tworzy
GUI, serwera automatyki, discovery, wejścia mikrofonowego ani procesu pośredniego.
Bez argumentów odmawia połączenia (kod 2). `--help` i `--self-check` nie tworzą
sesji ani gniazd. Wszystkie uruchomienia narzędzia w tym zadaniu były offline.

Przyszła próba sprzętowa wymaga jednocześnie `--connect` i
`--exclusive-lan-confirmed`, jawnego adresu IPv4 oraz pliku JSON z danymi konta.
Plik musi być zwykłym plikiem, a nie urządzeniem lub dowiązaniem; ma ograniczoną
wielkość. Dane konta nie trafiają do raportu ani parametrów wiersza poleceń.
Ustawienia trafiają do tymczasowego katalogu. Czas obserwacji jest ograniczony
do 5–120 sekund. Raporty JSON zawierają ważność/wiek odczytów, potwierdzoną
częstotliwość TX i liczniki odmów. Historyczna wartość nie staje się przez to
potwierdzonym odczytem TX. Częstotliwość zgłoszona w RX jest odczytem radia,
a nie pomiarem emisji RF.

Przenośny artefakt CI zawiera jeden EXE, app-local DLL Qt/vendor/MSVC, dokumentację,
licencję, pochodzenie źródeł oraz manifest SHA256. Workflow sprawdza brak innych
EXE i uruchamia `--self-check` z PATH ograniczonym do katalogów systemowych.
Nie powstaje instalator, usługa ani wydanie.

## Lokalizacja zmian

| Plik | Zakres |
|---|---|
| `src/core/backends/icom/IcomOutboundPolicy.h` | Typ polityki, kontekst, domyślna odmowa |
| `src/core/backends/icom/IcomOutboundPolicy.cpp` | Dokładna lista CI-V i walidacja datagramów |
| `src/core/backends/icom/IcomStream.h` | Niemutowalna polityka i prywatne wzorce transportu |
| `src/core/backends/icom/IcomStream.cpp` | Jeden kontrolowany writer UDP, stop, retransmisje |
| `src/core/backends/icom/IcomSession.h` | Polityka i fabryka strumieni |
| `src/core/backends/icom/IcomSession.cpp` | Negocjacja RX, brak TX audio, kontrola ID/Busy/teardown |
| `src/core/backends/icom/IcomCivBackend.h` | Opcjonalny tryb audytu i diagnostyka |
| `src/core/backends/icom/IcomCivBackend.cpp` | Odczyty audytu, blokada zapisów/wake/scope/cleanup |
| `src/core/backends/icom/IcomRxAuditMain.cpp` | Samodzielny tester, jawny start, izolacja ustawień |
| `CMakeLists.txt` | Polityka w silniku i opcjonalny cel testera |
| `tests/icom_outbound_policy_test.cpp` | Testy polityki i 2304 mutacje danych |
| `tests/icom_rx_audit_egress_test.cpp` | Testy rzeczywistych bajtów za końcową blokadą |
| `tests/tests.cmake` | Rejestracja nowych testów, izolacja ustawień |
| `docs/hf5a-stage2-rx-only-audit.md` | Instrukcja i granice bezpieczeństwa |
| `.github/workflows/hf5a-stage2-rx-only-offline.yml` | Osobny job pomocniczy Windows, dowody i artefakt |
| `.github/workflows/hf5a-stage2-rx-only-repackage.yml` | Pakowanie zachowanego EXE bez ponownej kompilacji |

Punkty do przeglądu w rewizji kodu: `IcomOutboundPolicy.cpp:34` i `:51`,
`IcomStream.cpp:187`, `:219`, `:229`, `:490`, `IcomSession.cpp:53`, `:82`,
`:158`, `:430`, `:506`, `:670`, `:692`, `IcomCivBackend.cpp:1004`, `:1819`,
`:4486`, `:4519`, `:4554`, `:8088`. Numery dotyczą rewizji `31b6b418`.

## Kontrole lokalne

Samodzielny test polityki zbudowano przez GCC 14.2/C++20 z
`-Wall -Wextra -Werror`, bez Qt. Wynik: PASS, 9 zapytań, 2304 mutacje, 0 błędów.
Nie instalowano Qt ani nie wykonywano lokalnej kompilacji Qt.

Siedem kontroli statycznych zakończyło się kodem 0:
`check_engine_boundary.py --strict`, `gen_touchpoint_manifest.py --check`,
`check_capability_records.py --strict`, `check_command_plane.py --strict`,
`check_test_registration.py --strict`, `check_ci_test_gate.py`,
`gen_seam_probe_table.py --check`. Kontrola granic zgłasza 101 istniejących ustaleń,
0 blokujących; command plane zachowuje bazowe 125 wpisów, 0 blokujących.
Zamrożona lista CI pozostaje bez zmian. `git diff --check 58f663f7 31b6b418`: PASS.
Logi znajdują się w `local-checks/`.

## Wyniki Windows

Run [41777405546](https://github.com/jarscot-art/AetherSDR/actions/runs/41777405546)
(rewizja `31b6b418`): kompilacja PASS, bazowy egress PASS, mutacja wykryta
(7 asercji FAIL, oczekiwany CTest exit 8), źródło przywrócone bajt w bajt,
rebuild PASS, końcowe 12/12 testów PASS, integralność źródeł PASS. Wynik całego
joba **FAIL** z powodu dodatkowego sprawdzenia przenośnego katalogu o 05:31:36 UTC.
Nie był to błąd kompilacji ani testów CTest. W logu kompilacji występują
nieblokujące ostrzeżenia istniejącego kodu (m.in. FFTW C4005 oraz konwersje
typów C4267/C4805); nie usuwano ich w etapie 2. `--self-check` przy ograniczonym PATH
nie uruchomił się poprawnie; poprzedni workflow nie zapisywał jego kodu zakończenia.
Konkretne braki zostały następnie ustalone w runie 41780111902: dwa DLL OpenSSL,
opisane poniżej. Nie był to błąd polityki RX-only.
Oryginalny ZIP logów: SHA256
`449de07579bf34ff20ac39611f6771850afdbab0231e9da59166b5b343ffd392`.

Run [41780111902](https://github.com/jarscot-art/AetherSDR/actions/runs/41780111902)
(`b058dcc2`): kompilacja PASS, ponownie 12/12 testów PASS, mutacja wykryta
(oczekiwany exit 8), przywrócenie identycznych bajtów źródła i rebuild PASS,
integralność źródeł PASS. Cały job **FAIL** wyłącznie na sprawdzeniu pakowania.
`dumpbin /DEPENDENTS` i istniejący `check-deploy-dependencies.ps1` wykazały:
`aether-icom-rx-audit.exe` importuje `libssl-3-x64.dll` oraz `libcrypto-3-x64.dll`;
obu nie było w katalogu przenośnym. Dotychczasowy pełny PATH runnera zapewniał
te biblioteki z SDK. CMake wybrał `C:/Program Files/OpenSSL`, wersję 3.6.5,
dla domyślnie włączonego MQTT/TLS we wspólnym silniku (`CMakeLists.txt:845`, `:3584`).
Narzędzie nie uruchamia klienta MQTT, lecz jego EXE nadal ma te importy.
OpenMP nie był ustaloną przyczyną tej awarii: kontrola wskazała dokładnie OpenSSL.
Oryginalny ZIP dowodów: SHA256
`e8670b663413c81ce90d99341c4744a0272eb3039e62b34ab4da7ee482cf5907`.

Run [41782953139](https://github.com/jarscot-art/AetherSDR/actions/runs/41782953139)
(`88370a51`, checkout kodu `b058dcc2`): **SUCCESS**. Pobrano zachowany artefakt EXE
z poprzedniego runu, sprawdzono jego pochodzenie i wcześniejszy JUnit 12/12.
Nie uruchamiano w tym jobie CMake, kompilacji ani CTest. Dołączono dokładne
biblioteki SDK OpenSSL 3.6.5. Następnie:

- kontrola importów: PASS, 46 obrazów PE, wszystkie importy w pakiecie lub OS;
- pełne dołączenie CRT/OpenMP istniejącym skryptem: PASS, bez instalatora;
- `--self-check` przy PATH ograniczonym do Windows/System32 i Windows: PASS, exit 0;
- jeden EXE w pakiecie, bez innych programów pomocniczych: PASS;
- hash EXE przed/po pakowaniu identyczny: PASS;
- manifest zawiera 50 plików; jego CSV jest dodatkowym plikiem pakietu.

SHA256 EXE:
`5F8F4EB6E3605DDA351687F3B5CC109CFA8EC8B0F485F86B0C6997E936D982E3`.
Nie naprawiano kodu RX-only w celu rozwiązania problemu pakowania. Nowy pełny
workflow również dołącza OpenSSL; zmianę pakowania sprawdza osobny pomocniczy
workflow, a pełna kompilacja reaguje na zmiany źródeł/testów/CMake.

[Przenośny artefakt, 86 560 069 bajtów](https://github.com/jarscot-art/AetherSDR/actions/runs/41782953139/artifacts/13299434867)
— ZIP SHA256 `70ec3da6f5ae4d9bbdcd1a3d336885546c4a06286db9e026c954a936e26cd365`.
Jest to artefakt CI, bez publikacji wydania. Ze względu na limit lokalnego narzędzia
pobierania 32 MiB nie pobierano go do workspace. Zachowano metadane, pełny manifest,
log audytu importów i zgodności EXE. Oryginalny ZIP logów pakowania zachowano:
SHA256 `26d5d27a43a42f603f3fd93e9a2aa5c56af0f1dadaa9705cf78e94220ea26621`.

Poniższe testy rzeczywiście wykonano w runie 41780111902; run pakowania tylko
odczytał ich wcześniejsze wyniki.

| Test (run 41780111902) | Wynik |
|---|---|
| icom_outbound_policy_test | PASS |
| icom_rx_audit_egress_test | PASS |
| icom_protocol_test | PASS |
| icom_replay_test | PASS |
| icom_session_lease_test | PASS |
| icom_civ_test | PASS |
| icom_control_profile_test | PASS |
| icom_backend_seam_test | PASS |
| transmit_model_test | PASS |
| backend_family_switch_test | PASS |
| icom_rx_audit_self_check | PASS |
| icom_rx_audit_refuses_implicit_connection | PASS (oczekiwana odmowa, exit 2) |

Bez blokady writer zarejestrował 22 pakiety CI-V zamiast 2 i 2 pakiety audio zamiast
0. Po przywróceniu blokady: CI-V `before=2, after_forbidden=2, denied=30`; audio
`packets=0, denied=4`. To obserwacja rzeczywistych bajtów za polityką, bez gniazd.
Testy potwierdziły też bezpieczne odroczone usunięcie sesji przy błędzie i
przerwanie inicjalizacji, gdy obserwator usunie backend.

Pierwsza weryfikacja `a200ce5d` — run
[41774825649](https://github.com/jarscot-art/AetherSDR/actions/runs/41774825649):
kompilacja PASS, końcowe 12/12 testów PASS. Kontrolowane usunięcie blokady
wykryte: 7 nieudanych asercji, CTest exit 8 (oczekiwany wynik negatywny).
Przywrócenie dokładnych bajtów źródła, ponowna kompilacja i kontrola integralności:
PASS. Oryginalny ZIP dowodów zachowano, SHA256
`6a96860fbfb88a694318dd91f4c71b02fd5e9fba18dd133c7dca1e4890dd2f83`.

## Granice potwierdzenia i ryzyka

1. Nie wykonano połączenia z radiem ani drugiego logowania LAN. Druga sesja
   RS-BA1 może zakłócić działającego klienta już podczas logowania; blokada
   CI-V/TX nie gwarantuje współdzielenia. Flaga potwierdzenia jest oświadczeniem
   operatora. Busy jest odrzucane, lecz może nadejść dopiero po logowaniu.
2. Przed testem sprzętowym potrzebne jest osobne zatwierdzenie i kontrolowane
   okno wyłącznego użycia LAN. Nie wymaga to ponownej konfiguracji LAN ani COM.
   Gdy produkcyjna sesja musi działać bez przerwy, narzędzia nie wolno podłączać.
3. Testy wstrzykują writer dokładnie za blokadą i nie otwierają gniazd. Potwierdzają
   zachowanie kodu i bajtów, bez potwierdzenia odpowiedzi firmware ani zachowania
   arbitrażu sesji rzeczywistego IC-7300MK2.
4. CI-V nie zawiera identyfikatorów transakcji; zachowane ograniczenia etapu 1
   obejmują niemożność pełnego odróżnienia spóźnionej odpowiedzi oraz brak
   atomowego obrazu SPLIT/TX. Ważność jest ograniczana przez istniejący mechanizm.
5. Weryfikacja runtime dotyczy runnera Windows. Windows 10 HF5A i realny transport
   nie zostały uruchomione. Pełnej aplikacji GUI i całego zestawu testów repozytorium
   w tym jobie nie budowano ani nie uruchamiano.
6. Właściwy kanał produkcyjny pozostaje w trybie production; RX-only dotyczy
   dedykowanego testera. Innych procesów nadających/sterujących radiem narzędzie
   nie kontroluje. To zabezpieczenie ścieżek programu, a nie izolacja przed
   celową modyfikacją pliku EXE lub dowolnym obcym kodem w procesie.
7. Hasło i plik konta wymagają ochrony po stronie operatora. JSON obserwacji nie
   zawiera danych konta, ale istniejący log informacyjny IcomSession może zawierać
   nazwę użytkownika (`IcomSession.cpp:269`). Nie należy publicznie publikować
   przyszłych logów sprzętowych bez przeglądu. Logi offline używają danych testowych.
8. Artefakty Actions są przechowywane przez 90 dni. Dowody tekstowe i oryginalne
   ZIP-y logów są dodatkowo zachowane na gałęzi. Niczego nie usuwano.

Gałęzie etapu 1 i main, Draft PR #1, produkcyjny EXE i ustawienia Windows,
COM4/COM5 oraz programy HF5A nie należą do zmian etapu 2. Nie wykonano merge,
publikacji wydania ani testu sprzętowego. Wynik kontroli referencji i Draft PR #1
znajduje się w `repository-state-before-final-evidence.json`.

## Archiwum i interpretacja kontroli diff

Podkatalogi `windows-run-*` i `windows-package-*` zawierają oryginalne ZIP-y,
rozpakowane dane, metadane GitHub, pełne logi jobów oraz zweryfikowane podsumowania.
Każdy ma manifest SHA256. Logi Windows zachowują CRLF i oryginalne końcowe spacje;
nie normalizowano ich w celu wyciszenia kontroli whitespace. `git diff --check`
przeszedł dla 16 plików implementacji i pomocniczych workflowów; raport i metadane
sprawdzono oddzielnie. Pełny `git diff --cached --check` dla surowego archiwum zgłasza oryginalne
białe znaki logów Windows (exit 2); kontrola raportu/metadanych bez surowych
logów zakończyła się kodem 0. Wszystkie 162 pliki archiwum sprawdzono także
przez porównanie hashy surowych bajtów plików z blobami indeksu Git: PASS.
Oryginalne ZIP-y stanowią niezależne zachowanie danych bez takich przekształceń.

W tym etapie nie wykonano całej suity repozytorium, testu Windows 10 HF5A,
rzeczywistego połączenia RS-BA1 ani potwierdzenia zachowania firmware. Brak
nieoczekiwanych FAIL w wybranych 12 testach; oba FAIL jobów pakowania i oczekiwane
FAIL mutanta są jawnie zachowane powyżej. Gotowość offline nie jest zgodą na
równoległą sesję LAN ani uruchomienie testu sprzętowego.
