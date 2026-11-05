# HF5A — etap 4: pełny desktop testowy

Ta paczka zawiera pełny AetherSDR 26.11.1 z odczytami etapu 3. **To nie jest
aplikacja RX-only.** Zachowuje normalne funkcje TX, PTT, CW, strojenia, VFO,
RIT/XIT, scope i waterfall. Odczyty SPLIT/TX są informacyjne; nie tworzą
drugiej sesji LAN, nie przestrajają radia i nie utożsamiają TX z VFO B.

Wspólne `RxReadbackWidget`, `TransmitModel`, `MainWindow_Wiring` i znacznik
`SpectrumWidget` pozostają bez zmian względem
`be026cc91c35e976049e326bd706fb31899dc32a`. TX pochodzi z potwierdzonego
`1C 03`; RX jest częstotliwością dial, a różnica to TX−RX dial (bez dodawania
RIT). Ważność, timeouty, wygaszanie i rozłączenia zachowują istniejący kontrakt.

## Uruchomienie oddzielnej wersji

Rozpakuj ZIP do nowego katalogu, oddzielnego od instalacji produkcyjnej.
Uruchamiaj `AetherSDR-HF5A-stage4.exe` bez argumentów. Nie zastępuj EXE
produkcyjnego i nie kopiuj jego ustawień. Nie ma instalatora ani usług.

Każde uruchomienie tworzy **nowy profil** `%TEMP%/hf5a-desktop-stage4-XXXXXX`.
Ustawienia i logi pozostają w tym katalogu do ręcznego usunięcia; kolejne
uruchomienie ich nie wczytuje. To świadomy koszt izolacji pierwszych testów:
układ zapisany podczas testu nie jest przywracany przy następnym uruchomieniu.
Ścieżka profilu jest zapisywana w logu. Zmienne środowiska wskazujące inny
profil lub autostart mostka automatyzacji są ignorowane.

Automatyczne połączenie, start mostka automatyzacji oraz sprawdzanie
aktualizacji są wyłączone na początku każdego uruchomienia. Dane logowania
pozostają tylko w pamięci sesji: ten wariant nie zawiera QtKeychain, więc nie
czyta ani nie nadpisuje poświadczeń produkcyjnych. Baza, migracje, logi,
motywy, MIDI, pamięci i stare pliki presetów respektują profil testowy.
Zwykłe buildy bez `AETHER_HF5A_DESKTOP_TEST=ON` zachowują domyślne ścieżki.

Pełny desktop nadal wykonuje normalne wykrywanie urządzeń. Nie jest to
blokada sieci ani blokada TX. Połączenie wymaga ręcznego działania operatora;
nie łącz tej wersji z radiem równolegle do produkcyjnego AetherSDR. Niniejszy
etap nie obejmuje żadnego testu sprzętowego ani zmiany konfiguracji LAN/COM.

## Automatyczna weryfikacja

Dedykowany workflow `hf5a-stage4-desktop-offline.yml` uruchamia się po pushu
na `hf5a/stage4-desktop-readback`. Używa MSVC i Qt 6.12.0, GPU spectrum oraz
zależności z wcześniejszego joba Windows. Kompiluje pełny desktop, CLI audytu
i 31 wybranych testów (nie deklaruje wykonania całej listy testów projektu).
Obejmuje odczyty GUI, politykę audytu, inicjalizację, wyłączność LAN, istniejące
PTT/TX innych backendów, ustawienia, motywy i pamięci.

`--hf5a-offline-self-check` tworzy prawdziwy MainWindow i przez dwie sekundy
obsługuje zdarzenia startowe. Sprawdza obecność readbacku, izolację ustawień,
brak połączenia lub próby połączenia oraz brak potwierdzonego TX. **Sam
argument nie blokuje sieci.** CI przed jego użyciem wymusza reguły Windows
Firewall dla EXE w obu kierunkach, sprawdza aktywność zapory i zapisuje reguły.
Test nie jest instrukcją uruchamiania na komputerze HF5A.

Pakowanie następuje dopiero po poprawnym CTest. Weryfikacja DLL obejmuje
`dumpbin /DEPENDENTS`, kontrolę zależności i uruchomienie EXE z ograniczonym
PATH. ZIP zawiera Qt, biblioteki runtime, dokumentację i manifest plików.
`portable-manifest.json` podaje SHA256 EXE i **wewnętrznego portable ZIP**;
opakowanie artefaktu GitHuba ma osobny hash. Logi, JUnit, CSV, ślady izolacji,
reguły zapory i SHA256 są zachowywane w artefaktach GitHub Actions przez 90 dni.

Build wyłącza opcjonalne ASR zgodnie z wcześniejszą konfiguracją kwalifikacyjną.
Nie zawiera instalatora, updatera EXE ani usług. Nie należy aktualizować tej
paczki z menu aplikacji ani importować do niej profilu produkcyjnego.

## Kontrole lokalne przed push

Sprawdzono rejestrację testów, zamrożoną listę CI, integralność odziedziczonych
ścieżek readback/TX, składnię YAML i listę 31 testów, granicę GUI/engine,
regułę QSettings, dostępność zmienionego dialogu motywów oraz `git diff --check`.
Zbudowano dokumentację użytkownika (`npm run build`). Nie uruchamiano lokalnie
kompilacji C++/Qt ani testów sprzętowych. Wyników Windows CI nie oceniano
w tej sesji; wymagają niezależnej weryfikacji po pushu.
