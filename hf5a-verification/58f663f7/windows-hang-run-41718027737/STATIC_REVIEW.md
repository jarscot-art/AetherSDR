# Kontrola zakresu diagnostyki HF5A

- Źródła testowane: `58f663f7bcda25c54e8f4c158929a6e18ce10011`.
- Niezmieniona baza: `33975b2354972a83bf7a6378ab2786e17cb49767`.
- Narzędzia diagnostyczne: `a0be0bc3ded557eda3dad79e5bdb006c93acbce4`, podpis SSH zweryfikowany lokalnie i przez GitHub.
- Gałąź: `hf5a/stage1-windows-hang-diagnostics`, wyłącznie fork `jarscot-art/AetherSDR`.
- Przebieg: https://github.com/jarscot-art/AetherSDR/actions/runs/41718027737
- Raport wejściowy zachowany na gałęzi `hf5a/stage1-windows-five-tests`, commit `7686f78241e3a2144839013df4ac888225d99a6f`.

Kontrole wykonane przed uruchomieniem:

- `git diff --cached --check`: PASS.
- `python tools/check_ci_test_gate.py`: PASS; 7 pozycji zamrożonej listy, bez zmian.
- `python tools/check_test_registration.py --strict`: PASS.
- Walidacja struktury YAML i ograniczeń workflowu w Python/PyYAML: PASS.
- `git diff --exit-code 58f663f7 HEAD -- src tests CMakeLists.txt .github/ci-test-gate.txt .github/workflows/ci.yml`: PASS, brak zmian.
- `git diff --name-only 33975b23 58f663f7 -- third_party`: pusty wynik; brak różnic zależności między bazą i poprawką.
- Zdalny `main` nadal `33975b23…`, gałąź poprawki nadal `58f663f7…`, PR #1 nadal Draft i bez zmiany SHA.

Nowy workflow buduje pięć celów testowych i ich zależności oraz osobne narzędzie Windows SDK. Nie buduje AetherSDR ani instalatora. Odtwarza konfigurację zależności `ci.yml:check-windows`; dodaje wyłącznie symbole linkera EXE dla stosów (`/DEBUG:FULL /INCREMENTAL:NO /OPT:REF /OPT:ICF`). Nie instaluje nic na lokalnym Windows HF5A ani w produkcji.

Potwierdzone w źródłach: kopiowanie DLL FFTW (`CMakeLists.txt:3308`), HID (`:3179`), FFTW float (`:3451`) i DeepFilter (`:3510`) jest przypięte do POST_BUILD celu AetherSDR. Wcześniejszy jednorazowy job nie budował tego celu. Jest to hipoteza przyczyny poprzednich timeoutów, wymagająca pomiarów DLL/stosów i porównania przed/po przygotowaniu runtime.

Ograniczenia kontroli lokalnej: brak PowerShell, MSVC i Qt w środowisku Codex. Nie wykonano lokalnej kompilacji, parsowania przez PowerShell ani uruchomienia testów Qt. Wyniki rzeczywistych testów Windows i narzędzia diagnostycznego muszą pochodzić z logów Actions.
