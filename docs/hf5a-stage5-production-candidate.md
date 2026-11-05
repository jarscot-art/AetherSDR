# HF5A — AetherSDR 26.11.1: kandydat produkcyjny

To oddzielna paczka portable RC, **bez instalatora i bez wykonania wdrożenia**.
Zawiera normalny `AetherSDR.exe`, helper D-STAR, manifest AetherDV oraz standardowy
helper aktualizacji. Żaden helper ani instalator nie jest automatycznie uruchamiany
przez workflow pakujący. Nie utworzono wydania GitHub Release.

## Zachowanie programu

Obie opcje `AETHER_HF5A_DESKTOP_TEST` i `AETHER_RX_READBACK_GUI_ONLY` są OFF.
Wszystkie źródła produkcyjne są identyczne z kwalifikowaną bazą
`82019c813cf90708aa2a1abcecdc4c34d72cbf02`. Nie ma nowego mechanizmu readback,
sesji LAN ani nowej komendy CI-V. SPLIT, TX z `1C 03`, RX dial, różnica TX−RX
dial (bez RIT), wygaszanie i znacznik TX zachowują istniejącą implementację.
TX nie oznacza automatycznie VFO B. Pełny desktop zachowuje normalne funkcje TX.

**Portable nie oznacza izolacji ustawień.** Uruchomienie bez argumentów używa
normalnego, trwałego profilu AetherSDR i jego poświadczeń QtKeychain. Obowiązują
zapisane ustawienia autopołączenia i standardowe migracje aplikacji. Dlatego nie
uruchamiaj RC na stanowisku HF5A przed odrębną zgodą na wdrożenie i backupem.
Etap 5 niczego nie uruchamia na tym stanowisku i nie zmienia jego profilu.

## Kwalifikacja Windows w CI

Workflow `hf5a-stage5-production-candidate.yml` działa po pushu tylko na gałąź
`hf5a/stage5-production-candidate`. Buduje MSVC/Qt 6.12.0 z QtKeychain, SerialPort,
hidapi, PortAudio, FFTW, DeepFilterNet, RTL/libusb, RADE/D-STAR oraz ASR
(whisper/Vulkan, ONNX Runtime, sherpa-onnx). ASR nie jest wyłączony na potrzeby
testów. Zależności i wymagania wydania pochodzą z istniejącego workflow Windows
Installer; zachowano również MQTT TLS i jego biblioteki OpenSSL. Brak wymaganej
zależności powoduje błąd, a nie cichą redukcję funkcji. Wagi ASR oraz opcjonalny
runtime NVIDIA są pobierane przez normalny program na żądanie, jak dotychczas.

Wymagane są wszystkie 36 wybranych testów CTest bez SKIP: readback, GUI,
backendy, kontrakty TX/PTT, audyt, ustawienia oraz obsługa poświadczeń. Testy
współbieżnego odczytu poświadczeń Icoma oraz peryferiów używają istniejących
atrap. `icom_settings_test` zachowuje oryginalny test zapisu syntetycznego
hasła przez normalny backend, wyłącznie w pustym koncie jednorazowego runnera
(standardowa usługa AetherSDR, żadnych danych operatora). Dodatkowy opt-in
`hf5a_keychain_runtime_test` zapisuje, odczytuje, usuwa i potwierdza brak własnego
losowego wpisu w rzeczywistym backendzie QtKeychain. Używa osobnej nazwy usługi
`HF5A-CI-<UUID>`, nigdy usługi `AetherSDR`; fallback do niezaszyfrowanych ustawień
jest zabroniony. Działa wyłącznie na jednorazowym koncie runnera.

Normalny EXE jest sprawdzany inną metodą niż desktop testowy etapu 4.1:

- `--update-selftest` sprawdza ładowanie Qt i numer wersji przed MainWindow;
- `--config path/set/get/export` potwierdza odizolowaną bazę, trwałość ustawień
  pomiędzy procesami oraz odmowę zapisu poświadczeń w bazie;
- każde wywołanie ma limit czasu i własny `AETHER_SETTINGS_DIR` pod raportem CI;
- zapora Windows blokuje EXE w obu kierunkach przed pierwszym uruchomieniem;
- program nie wchodzi do MainWindow, nie tworzy sesji radiowej ani nie wczytuje
  profilu lub poświadczeń operatora.

To nie jest nowy test pełnego okna normalnego EXE ani test sprzętowy. Kwalifikacja
pełnego GUI pochodzi z etapu 4.1; CI etapu 5 dodatkowo sprawdza identyczność
źródeł produkcyjnych, build normalnego wariantu oraz regresje widgetów/modeli.

Pakowanie następuje wyłącznie po zaliczeniu testów i prób normalnego EXE.
Paczka otrzymuje Qt, QtKeychain, biblioteki ASR, Vulkan, RTL, OpenSSL i MSVC.
`dumpbin /DEPENDENTS`, walidator bibliotek oraz ponowna próba EXE przy
ograniczonym PATH kontrolują kompletność. `SHA256SUMS.txt` obejmuje zawartość;
`portable-manifest.json` zawiera SHA256 EXE i wewnętrznego ZIP. Artefakt GitHuba
jest dodatkowym opakowaniem i ma osobny hash. Logi/JUnit/CSV/JSON oraz ZIP są
zachowywane przez 90 dni. Wyniki CI wymagają niezależnego odczytania po pushu.

## Plan backupu przed przyszłym wdrożeniem — nie wykonywano

1. Uzgodnij okno testowe i wyłącz produkcyjny AetherSDR w normalny sposób.
   Potwierdź zakończenie jego procesu. Nie uruchamiaj dwóch klientów LAN.
   Nie zmieniaj HRD/COM4, CI-V/COM5, SOOMFON, AetherKnobs, FIX EDGE ani PANELU.
2. Zachowaj cały katalog obecnego programu 26.11.1, skróty, argumenty startowe
   i numery/hash EXE. Nie nadpisuj archiwów FINAL GOOD.
3. Skopiuj cały katalog profilu wskazywany przez używaną instalację, zwykle
   `%LOCALAPPDATA%/AetherSDR`, po zakończeniu procesu. Uwzględnij bazę SQLite,
   istniejące pliki WAL/SHM, legacy XML, backupy, motywy, MIDI i pamięci.
   Eksport ustawień jest dodatkiem, nie zastępuje kopii katalogu.
4. Zachowaj wskazane w ustawieniach własne katalogi nagrań, VoiceKeyer,
   presetów i zasobów oraz pamięć aplikacji/cache, jeśli mają znaczenie dla
   odtworzenia stanowiska. Zanotuj aktualny wybór profilu i autopołączenia.
5. Poświadczenia QtKeychain należą do Windows Credential Manager i ochrony
   DPAPI użytkownika. Sama kopia SQLite ich nie zawiera. Wykonaj zatwierdzony
   pełny backup systemu/profilu Windows obejmujący Credential Manager i DPAPI,
   z możliwością odtworzenia na tym samym koncie. Nie eksportuj haseł do tekstu.
6. Zweryfikuj kompletność i SHA256 kopii oraz możliwość jej odczytu. RC rozpakuj
   później do nowego katalogu; samo rozpakowanie nie jest zgodą na uruchomienie.

## Plan wycofania — nie wykonywano

Zakończ RC normalnie i potwierdź brak procesu/sesji LAN. Zachowaj osobno jego
logi do diagnostyki. Przywróć cały poprzedni katalog programu oraz spójną kopię
całego profilu, bez mieszania starej bazy z nowymi WAL/SHM. Jeśli zmieniły się
poświadczenia, odtwórz je z zatwierdzonego backupu Windows/DPAPI. Następnie
przywróć poprzedni skrót i uruchom tylko wcześniejszą wersję, w zatwierdzonym
oknie testowym. Przy problemie z odtworzeniem użyj zweryfikowanego obrazu
systemu zamiast ręcznie modyfikować bazę. Konfiguracje COM/LAN i pozostałych
programów pozostają bez zmian.

## Stan przygotowania

Przed push wykonano kontrole statyczne YAML, rejestracji testów, zamrożonej listy
CI, integralności źródeł, składni skryptów Python i `git diff --check`. Nie
kompilowano lokalnie Qt/Windows ani nie wykonywano testów sprzętowych.
Wyniki kompilacji, testów i SHA256 paczki powstaną dopiero w GitHub Actions.
