# HF5A — ustalona przyczyna zawieszenia testów Windows

Przyczyną odtworzonego zawieszenia był brak bibliotek DLL w środowisku uruchamiania testów. Procesy zatrzymywały się w loaderze Windows przed uruchomieniem `main()`, w `ntdll!NtRaiseHardError+0x14`, z kodem `0xC0000135` (`STATUS_DLL_NOT_FOUND`). Po przygotowaniu czterech DLL te same pliki EXE zaliczyły wszystkie cztery testy Qt. Zmiany kodu SPLIT/TX ani testów nie były potrzebne.

Przebieg: https://github.com/jarscot-art/AetherSDR/actions/runs/41718027737

Job: https://github.com/jarscot-art/AetherSDR/actions/runs/41718027737/job/126203074853

Źródła aplikacji i testów: `58f663f7bcda25c54e8f4c158929a6e18ce10011`.
Narzędzia diagnostyczne: `a0be0bc3ded557eda3dad79e5bdb006c93acbce4`, tylko gałąź pomocnicza `hf5a/stage1-windows-hang-diagnostics` w forku.
Windows image `windows-2025-vs2026`, wersja `20261030.294.1`; MSVC **19.51.36260.0**, Qt **6.12.0**, x64. Wersję kompilatora potwierdza jego rzeczywisty wynik identyfikacji CMake, a nie nazwa katalogu toolsetu. Obraz runnera i wersja kompilatora odpowiadają wcześniejszemu raportowi `windows-run-41708318016`.

## Rzeczywiście wykonane testy

| Test | Pierwotny runtime | Po przygotowaniu DLL | Czas testu po przygotowaniu wg JUnit |
|---|---|---|---|
| `icom_control_profile_test` | TIMEOUT, limit 60 s | **PASS**, CTest 0 | 0,182944 s |
| `icom_backend_seam_test` | TIMEOUT, limit 60 s | **PASS**, CTest 0 | 0,129024 s |
| `transmit_model_test` | TIMEOUT, limit 60 s | **PASS**, CTest 0 | 0,0409199 s |
| `backend_family_switch_test` | TIMEOUT, limit 60 s | **PASS**, CTest 0 | 3,48963 s |
| `icom_civ_test` | Nie uruchamiano ponownie | Nie uruchamiano ponownie | Poprzedni Windows PASS: 0,03 s w run `41708318016` |

Zbudowano wszystkie pięć celów i sprawdzono ich DLL; wykonano dokładnie cztery pozostałe testy, każdy osobnym wywołaniem CTest, w dwóch wariantach runtime. Każdy przypadek ma `result.json`, `stdout.log`, `stderr.log`, `ctest.log` i `results.junit.xml`. Pierwotne TIMEOUT są zachowane jako niepowodzenia: CTest 8 po kontrolowanym zakończeniu procesu przez harness. Nie są to niezaliczone asercje SPLIT/TX. Rzeczywisty czas wykonania do timeoutu wynosił ok. 60 s, a całość z zebraniem dowodów 60,31–60,40 s. Nie zwiększano limitów testów.

## Dowody przyczyny

1. `dumpbin /DEPENDENTS` wskazuje dla **każdego z czterech testów Qt** bezpośrednie importy:
   - `hidapi.dll`,
   - `libfftw3-3.dll`,
   - `libfftw3f-3.dll`,
   - `deepfilter.dll`.

   Wszystkich czterech brakowało w katalogu testów i zarejestrowanej ścieżce wyszukiwania. `artifact-original/feature-original/dll/import-resolution.csv` zawiera 16 brakujących importów, a `missing-imports.txt` czytelną tabelę. Dla wszystkich pięciu EXE zachowano osobne wyniki `/DEPENDENTS` i `/HEADERS`. Importy DLL sprawdzono również rekurencyjnie poza katalogiem systemowym; kontrakty Windows API-set oznaczono oddzielnie. Qt było na PATH i jego DLL były załadowane w zawieszonych procesach.

2. Po 60 sekundach, **przed zakończeniem każdego procesu**, poprawnie zapisano `process.dmp` i stosy wszystkich czterech jego wątków. Każde `capture.json` potwierdza zakończenie kolektora i kod 0. Wszystkie zrzuty mają sygnaturę MDMP i konteksty AMD64. Żadne okno należące do testu nie zostało znalezione; pliki `windows.txt` są zachowane, ale nie potwierdzają widocznego dialogu błędu.

   Konkretne miejsca blokady w zapisanych stosach:

   | Test | Plik stosów, pierwsza ramka blokady | TID głównego wątku |
   |---|---|---|
   | `icom_control_profile_test` | `artifact-original/feature-original/icom_control_profile_test/capture/threads.txt:41` | 4016 |
   | `icom_backend_seam_test` | `artifact-original/feature-original/icom_backend_seam_test/capture/threads.txt:51` | 8796 |
   | `transmit_model_test` | `artifact-original/feature-original/transmit_model_test/capture/threads.txt:56` | 4520 |
   | `backend_family_switch_test` | `artifact-original/feature-original/backend_family_switch_test/capture/threads.txt:51` | 876 |

   Każdy główny stos zawiera `ntdll!NtRaiseHardError+0x14` i `ntdll!LdrInitializeThunk+0xe`, bez ramek aplikacji. Dodatkowy, tylko odczytowy parser `analyze_minidumps.py` odczytał z kontekstów zrzutów argumenty `RCX=R10=0xC0000135` we wszystkich **16 wątkach**. Wynik: `minidump-register-analysis.json`. Tym samym wskazano błąd ładowania DLL na etapie inicjalizacji procesu, przed kodem testu.

3. W tym samym jobie skopiowano dokładnie te cztery DLL do prywatnego `build/hf5a-runtime` i dodano wyłącznie ten katalog na początku PATH. Porównanie zapisanych PATH potwierdza identyczną pozostałą część ścieżki. `feature-prepared/staged-dlls.csv` zawiera źródła, cele i SHA256. Nie przebudowywano EXE między obiema seriami. Ponowny audyt zależności nie wykazał brakujących DLL; wszystkie cztery testy zakończyły się PASS. To kontrolowane porównanie izoluje brak runtime DLL jako przyczynę odtworzonego zawieszenia.

## Dlaczego standardowy job Windows nie ujawnił problemu

W źródłach z `58f663f7` kopiowanie bibliotek do katalogu wykonywalnego jest podpięte do **POST_BUILD celu AetherSDR**:

- `CMakeLists.txt:3179` — HID;
- `CMakeLists.txt:3308` — FFTW double;
- `CMakeLists.txt:3451` — FFTW float;
- `CMakeLists.txt:3510` — DeepFilter.

Standardowy `ci.yml:check-windows` buduje aplikację i wykonuje te operacje. Poprzedni jednorazowy job budował wyłącznie testy i ich zależności, więc DLL nie zostały przygotowane do ich uruchomienia. Niezależny `icom_civ_test` nie importuje Qt ani tych czterech bibliotek, co wyjaśnia jego wcześniejszy PASS. Zawieszenie z poprzedniego raportu zostało odtworzone, a następnie usunięte samą korektą runtime w izolowanej diagnostyce.

## Baza i ograniczenia wnioskowania

- Kroki budowania i uruchamiania bazy `33975b2354972a83bf7a6378ab2786e17cb49767` były warunkowe. Zostały **SKIPPED**, ponieważ po przygotowaniu runtime żaden test poprawki już się nie zawieszał. Nie deklarujemy wyników testów bazowego commitu. Warunek użytkownika dotyczący porównania bazy przy utrzymującym się zawieszeniu nie wystąpił.
- Wyniki potwierdzają wykonanie czterech testów na wskazanych źródłach w Windows/MSVC/Qt 6.12.0. Nie są dowodem zachowania rzeczywistego IC-7300MK2, transmisji, całej aplikacji ani pozostałych testów repozytorium. Pierwszej udanej kompilacji aplikacji Windows nie powtarzano.
- Testy były zbudowane jak w standardowym jobie Release, z Embedded debug info i dodatkowymi symbolami linkera EXE (`/DEBUG:FULL /INCREMENTAL:NO /OPT:REF /OPT:ICF`) dla diagnostyki. Optymalizacja Release została zachowana.
- Stosy systemowe rozwiązano offline według eksportów DLL; nazwy pośrednich prywatnych funkcji i duże przesunięcia eksportów nie są pełnymi symbolami Microsoft PDB. Wniosek opiera się na dokładnej ramce `NtRaiseHardError`, wartości statusu z kontekstów, importach i kontrolowanym wyniku przed/po. Nie odtwarzano tekstu nazwy brakującej DLL z pamięci sterty; zwykły minidump jej nie zawierał. Komplet czterech nazw wynika z tabel importów i audytu PATH.
- Standardowy setup zależności na runnerze zmodyfikował cztery śledzone pliki: `third_party/deepfilter/COMMIT`, `third_party/deepfilter/include/deep_filter.h`, `third_party/fftw3/lib/fftw3.exp`, `third_party/fftw3/lib/fftw3.lib`. To ten sam zakres co poprzednio; log `final-tracked-source-changes.txt` go ujawnia. Kod `src/`, `tests/`, CMake i istniejące workflowy pozostały bez zmian. Całe drzewo zależności po setupie nie było bitowo identyczne z checkoutem.

## Zachowane wyniki

- `artifact-original.zip`: oryginalny artefakt, **219541 B**, SHA256 `2bc3c5a0570f95b69812e5d87fad80fc5ec10c4b1205b951891b21bcd19cfd07` — zgodny z digestem GitHub.
- `artifact-original/`: wszystkie 181 oryginalnych plików artefaktu, bez korekt, w tym cztery zrzuty, stosy, osiem wyników/JUnit, PATH i pełne logi przypadków. Dodatkowo zachowano zrzut i stosy próby kontrolnej kolektora.
- `windows-job.log`: pełny log joba; timeouty w liniach 3229–3232, cztery PASS w liniach 3314–3317.
- `github-jobs.json`, `github-artifacts.json`: metadane ukończonego przebiegu i artefaktów.
- `reviewed-results.json`, `reviewed-results.csv`: zbiorczy status z czasem pojedynczego testu z JUnit i odrębnym czasem harnessu.
- `evidence-sha256.csv`: sumy wszystkich oryginalnych plików oraz pełnego loga.
- `static-check-results.json`: kontrole statyczne i potwierdzenie, że Git zachowuje wszystkie 181 oryginalnych plików artefaktu bajt w bajt. Kontrola `git diff --check` narzędzi i napisanych raportów: PASS; strict test registration i frozen CI gate: PASS. Pełna kontrola dołączonych surowych logów zgłasza ich oryginalne CRLF i końcowe białe znaki; zachowano je celowo bez normalizacji. Wyjście tej kontroli: `raw-evidence-diff-check.log.gz`. Nie są to błędy kodu narzędzi ani poprawki SPLIT/TX.

Artefakty Actions, z automatyczną retencją do 1 lutego 2027:

- Logi/zrzuty: https://github.com/jarscot-art/AetherSDR/actions/runs/41718027737/artifacts/13272822437
- Feature EXE/PDB: https://github.com/jarscot-art/AetherSDR/actions/runs/41718027737/artifacts/13273451479
- Kolektor oraz EXE/PDB: https://github.com/jarscot-art/AetherSDR/actions/runs/41718027737/artifacts/13273121982

Logi i zrzuty są dodatkowo zapisane w Git na gałęzi pomocniczej, niezależnie od retencji artefaktów. Duże paczki EXE/PDB pozostają w artefaktach Actions. Żadnej gałęzi, raportu ani artefaktu nie usunięto. Dane wejściowe z run `41708318016` pozostają na dotychczasowej gałęzi pomocniczej.

## Wniosek i dalszy krok

Przyczyna zawieszenia jest ustalona: nieprzygotowane DLL w jednorazowym środowisku testowym Windows. Cztery brakujące wcześniej testy Qt są teraz **PASS**; protokół ma wcześniejszy Windows PASS. W tym zakresie nie ujawniono regresji SPLIT/TX. Przed kolejnymi testami Windows należy przygotowywać runtime DLL także przy budowaniu samych testów. Nie wymaga to zmiany implementacji etapu 1.

Nie wprowadzono poprawek do kodu produkcyjnego, testów źródłowych ani istniejących workflowów. Nie zmieniono PR #1, gałęzi `hf5a/stage1-split-tx-readback` ani `main`; nie wykonano merge, wydania, instalatora, lokalnej kompilacji Qt, operacji na radiu lub na Windows HF5A.
