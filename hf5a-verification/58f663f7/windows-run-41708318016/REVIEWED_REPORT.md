# HF5A — zweryfikowany raport jednorazowych testów Windows

Testowane źródła aplikacji i testów: `58f663f7bcda25c54e8f4c158929a6e18ce10011`.
Definicja workflowu: `ec085b04bacef21077e4831e86f7de18527dd974`, tylko gałąź `hf5a/stage1-windows-five-tests`.

Przebieg: https://github.com/jarscot-art/AetherSDR/actions/runs/41708318016
Job: https://github.com/jarscot-art/AetherSDR/actions/runs/41708318016/job/126169462439

Windows, MSVC 19.51.36260.0, Qt 6.12.0. Konfiguracja i budowanie pięciu celów oraz ich zależności: SUCCESS, kod budowania 0. Nie budowano celu AetherSDR ani instalatora.

| Test | Wynik | Czas / diagnostyka |
|---|---|---|
| icom_civ_test | PASS | 0,03 s, potwierdzone przez CTest |
| icom_control_profile_test | FAIL — timeout | 300,01 s; CTest zakończył test po limicie 300 s |
| icom_backend_seam_test | PRZERWANY | uruchomiony, następnie zabity przez limit 10 minut całego kroku; bez końcowego wyniku CTest |
| transmit_model_test | NIEURUCHOMIONY | nie osiągnięto przed przerwaniem kroku |
| backend_family_switch_test | NIEURUCHOMIONY | nie osiągnięto przed przerwaniem kroku |

## Potwierdzone fakty i ograniczenia

- `artifact-original/source.json` potwierdza checkout oczekiwanego SHA. `ctest-selection.json` zawiera dokładnie pięć wymaganych nazw.
- `windows-job.log` i `artifact-original/ctest.log.tmpf259e` potwierdzają PASS pierwszego testu, timeout drugiego i rozpoczęcie trzeciego.
- Testy Qt nie wypisały komunikatów przed timeoutem/przerwaniem. Nie ma zrzutu stosu ani diagnozy przyczyny blokady. Timeout nie dowodzi błędu obsługi SPLIT/TX.
- Limit 10 minut przygotowanego workflowu był za krótki na dokończenie pięciu testów, gdy pojedynczy test wykorzystuje pełne 300 sekund. Pięć takich limitów może wymagać 25 minut. Ten przebieg nie stanowi pełnej weryfikacji pięciu testów.
- JUnit nie został sfinalizowany. Oryginalne `artifact-original/REPORT.md`, `results.csv` i `results.json` automatycznie pokazują wszystkie pozycje jako NOT RUN. Są zachowane bez zmian, ale nie są miarodajną tabelą wykonania. Wiążące wyniki z tego przebiegu znajdują się w niniejszym raporcie i plikach `reviewed-results.*`, wyprowadzonych z logu CTest.
- Standardowe skrypty zależności z udanego `check-windows` zmieniły w przestrzeni roboczej runnera cztery śledzone pliki zależności: `third_party/deepfilter/COMMIT`, `third_party/deepfilter/include/deep_filter.h`, `third_party/fftw3/lib/fftw3.exp`, `third_party/fftw3/lib/fftw3.lib`. Odnotowano to w `tracked-source-changes.txt` i `execution.json`. Kod `src/`, `tests/`, istniejących workflowów oraz plik CMake nie został zmieniony. Całe drzewo zależności po setupie nie było bitowo identyczne z checkoutem.
- W kroku diagnostyki sccache wystąpił dodatkowy błąd `couldn't connect to server`; nie jest to błąd kompilacji pięciu testów.

## Hipoteza wymagająca sprawdzenia, bez zmian kodu

W zwykłym `check-windows` budowanie celu AetherSDR wykonuje kopiowanie bibliotek DLL do katalogu build. Ten jednorazowy workflow celowo budował wyłącznie testy i ich zależności, ale nie dodał osobnego przygotowania tych DLL do uruchomienia testów. Kod CMake kopiuje m.in. FFTW na `CMakeLists.txt:3308`, DeepFilter na `:3510`, HID na `:3179`, wyłącznie w POST_BUILD celu AetherSDR. Nie ma wystarczającej diagnostyki, by uznać brak DLL za udowodnioną przyczynę timeoutów.

Minimalny dalszy krok to diagnostyka zależności DLL (`dumpbin /DEPENDENTS`) i zrzut stosu zawieszonego procesu w odizolowanym runnerze, a następnie przygotowanie runtime bez budowania aplikacji i bez zmian jej źródeł. Testy warto uruchamiać osobnymi wywołaniami CTest z raportem JUnit dla każdego, by wynik ukończonego testu przetrwał timeout kolejnego. Łączny limit powinien uwzględniać wszystkie pięć limitów indywidualnych. Nie wykonano ponownego przebiegu ani poprawek źródeł.

## Zachowane dowody

- `windows-job.log`: pełny log joba, ze znacznikami czasu; odnośniki do linii są w `reviewed-results.json`.
- `artifact-original/`: oryginalne pliki pobrane z artefaktu, zachowane bez korekt.
- `artifact-original.zip`: oryginalny ZIP, SHA256 `573e4be6f5e4385e9e1222b1096e304a3860fb96bfa64c5a29e429bf72f4c023`.
- Artefakt GitHub Actions: https://github.com/jarscot-art/AetherSDR/actions/runs/41708318016/artifacts/13268322896 — automatyczna retencja do 1 lutego 2027. Ta kopia w Git nie zależy od retencji artefaktów.

Gałąź pomocnicza i wyniki nie zostały usunięte. PR #1 i gałąź źródłowa pozostają bez zmian. Nie wykonano merge, wydania, instalatora, operacji na radiu ani zmian produkcyjnych programów HF5A.
