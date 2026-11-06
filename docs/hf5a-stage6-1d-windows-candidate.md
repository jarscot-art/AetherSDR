# HF5A — Stage 6.1d: Windows test candidate

Oddzielny portable AetherSDR 26.11.1 dla przyszłych testów Windows 10 Pro x64.
Baza kodu: `4fc20d6b5c49697c29e6f6a9a5ae2ac655ab1ce3` (Stage 6.1c).
Dokładny commit paczki i adres CI znajdują się w `source.json`. To artefakt
GitHub Actions, bez instalatora, wydania ani automatycznego wdrożenia.

## Funkcje i granice

Normalny pełny desktop: `AETHER_HF5A_DESKTOP_TEST=OFF`,
`AETHER_RX_READBACK_GUI_ONLY=OFF`. Zachowuje ustawienia, QtKeychain, waterfall,
VFO, RIT/XIT, CW/SSB/DIGI/PTT, SOOMFON oraz istniejące integracje Stage 5.2.
**Pełna aplikacja ma funkcje TX; nie jest testerem RX-only.** Nowy readback
pozostaje informacyjny: SPLIT, rzeczywisty TX z `1C 03`, różnica TX−RX dial,
czerwony znacznik TX oraz **VFO NIEWYBRANE** (częstotliwość, emisja, DATA, FIL1–3).
Niewybrane VFO nie oznacza fizycznego B ani częstotliwości TX. Nieaktualne
obserwacje są wygaszane. Brak nowego sterowania, komend CI-V lub sesji LAN.

Źródła aplikacji, testy oraz istniejące helpery pakowania są identyczne z bazą.
Przepis pełnej kompilacji i pakowania pochodzi ze Stage 5.2: MSVC/Qt 6.12.0,
QtKeychain, SerialPort, FFTW, DeepFilter, hidapi, PortAudio, RTL/libusb, ASR
(whisper/Vulkan/ONNX/sherpa), RADE/D-STAR, OpenSSL i runtime MSVC. Opcjonalne
modele ASR i runtime NVIDIA pozostają pobierane na żądanie jak w Stage 5.2.
DLL QtKeychain trafia obok EXE przez prywatny zestaw wdrożeniowy Qt; globalny
katalog Qt nie jest modyfikowany. Nie wyłączono funkcji w celu uzyskania builda.

## Automatyczna kwalifikacja

Workflow `hf5a-stage6-1d-windows-candidate.yml` uruchamia się po push na
`hf5a/stage6-1d-windows-candidate`. Buduje pełny EXE, wymagane helpery i testy.
Wykonuje 37 testów CTest: wszystkie 36 regresji Stage 5.2 oraz
`icom_unselected_vfo_test`. Ten zestaw obejmuje wszystkie dziewięć wymaganych
regresji Stage 6.1c; ich osobny raport to `stage6-nine-tests.json`:

- `rx_readback_gui_test`, `icom_unselected_vfo_test`, `icom_civ_test`;
- `icom_outbound_policy_test`, `icom_rx_audit_egress_test`, `icom_rx_audit_requests_test`;
- `icom_backend_seam_test`, `transmit_model_test`, `backend_family_switch_test`.

Inwentaryzacja musi zgadzać się z listą, a JUnit zawierać 37 rzeczywiście wykonanych
PASS, bez SKIP. Dalsze próby normalnego EXE używają odziedziczonego,
zweryfikowanego helpera `hf5a_stage5_offline.py`: `--update-selftest` ładuje Qt,
plugin platformy i biblioteki oraz sprawdza wersję; `--config` sprawdza izolację
profilu, trwałość ustawień i odmowę zapisu hasła w bazie. Każde wywołanie ma
limit 45 sekund. Zapora jednorazowego runnera blokuje EXE w obu kierunkach
przed startem; profil jest pusty i tymczasowy. Test nie wchodzi do MainWindow,
nie tworzy sesji radiowej i nie odczytuje danych operatora. To **próba startu
normalnego EXE i regresje widgetów**, nie nowy test pełnego okna na Windows 10
ani test sprzętowy. Runner `windows-latest` jest środowiskiem CI, a nie komputerem HF5A.

Pakowanie i upload ZIP następują wyłącznie po wszystkich testach i kontroli
integralności źródeł. `dumpbin /DEPENDENTS`, kontrola domknięcia DLL, runtime
MSVC/OpenMP, licencje RTL/FFTW i ponowne uruchomienie spakowanego EXE z ograniczonym
PATH są wymagane. Paczka zawiera manifest źródła, README, JUnit, raport dziewięciu
testów i dowody startu. `SHA256SUMS.txt` obejmuje pliki paczki; `portable-manifest.json`
w artefakcie podaje SHA256 EXE i wewnętrznego ZIP. Plik `.zip.sha256` dotyczy tego
ZIP. Osobny `artifact-manifest.json` w artefakcie logów zawiera hash zewnętrznego
opakowania GitHub Actions. Nie są to te same trzy pliki. Logi, manifesty i wyniki
są zachowywane przez 90 dni, również gdy CI się nie powiedzie.

## Przygotowanie do przyszłego zatwierdzonego testu — nie wykonano

Nie nadpisuj katalogu produkcyjnego ani archiwum **Stage 5.2 FINAL GOOD**.
Rozpakowanie do nowego katalogu nie upoważnia do uruchomienia ani połączenia
z radiem. Portable nie izoluje automatycznie ustawień: normalny EXE bez jawnego
profilu używa trwałego profilu AetherSDR i zapisanych ustawień autopołączenia.

Przed osobno zatwierdzonym testem:

1. Zachowaj oryginalny ZIP Stage 5.2 FINAL GOOD, cały katalog produkcyjny,
   skróty i ich argumenty oraz sumy SHA256. Nie nadpisuj tych kopii.
2. Po normalnym zakończeniu produkcyjnego AetherSDR wykonaj spójną kopię całego
   profilu (SQLite, WAL/SHM, XML, motywy, pamięci, MIDI i zasoby zewnętrzne).
   Eksport ustawień nie zastępuje pełnego backupu. Poświadczenia QtKeychain
   wymagają zatwierdzonego backupu Windows Credential Manager/DPAPI dla tego
   samego konta; sama baza ich nie obejmuje. Nie zapisuj haseł w plikach tekstowych.
3. Kandydatowi przydziel nowy katalog programu i jawny, oddzielny
   `AETHER_SETTINGS_DIR`; nie kopiuj produkcyjnego profilu. Dla tego profilu
   wyłącz `AutoConnectToLastRadio` przed otwarciem GUI. Izolacja bazy nie tworzy
   osobnej usługi QtKeychain — poświadczenia wymagają osobnego uzgodnienia.
4. Nie uruchamiaj równoległej sesji LAN. Test radiowy wymaga odrębnego
   zatwierdzenia i zakończenia sesji produkcyjnej. Nie zmieniaj LAN, COM4/COM5,
   HRD, PANELU, SOOMFON, AetherKnobs ani FIX EDGE AUTO.

## Przywrócenie Stage 5.2 FINAL GOOD

Zakończ kandydata normalnie i potwierdź brak jego procesu/sesji. Zachowaj jego
logi osobno. Jeżeli produkcja pozostała nietknięta, wróć do oryginalnego skrótu
Stage 5.2. Jeżeli w późniejszym zatwierdzonym teście zmieniono produkcyjny profil,
przywróć pełną spójną kopię profilu i poprzedni katalog programu; nie mieszaj
bazy ze starszymi WAL/SHM. Zmienione poświadczenia odtwórz z zatwierdzonego
backupu Windows/DPAPI. Nie wprowadzaj zmian w pozostałych programach HF5A.

Przygotowanie tej gałęzi niczego nie instaluje i nie wykonuje tych czynności.
Wyniki kompilacji i hash paczki powstają dopiero w CI; wymagają niezależnej
weryfikacji po push. Codex nie pobiera paczki ani nie czeka na zakończenie CI.
