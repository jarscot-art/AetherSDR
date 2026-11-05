# HF5A 5.1 — kontrakt zakończenia instalacji zależności

Zgłoszony przebieg `42098054632` zakończył instalację ONNX Runtime komunikatem
`ready`, ale wrapper sprawdzał `$LASTEXITCODE` po wywołaniu skryptu PowerShell
w tym samym procesie. Nowa instalacja ONNX używa wyłącznie cmdletów; nie ustawia
tej zmiennej i nie kończyła się jawnym `exit`. `$LASTEXITCODE` jest statusem
ostatniego programu natywnego, a nie ostatniego skryptu/cmdletu. W świeżym kroku
jest niezainicjalizowane (`$null`), zaś `$null -ne 0` daje True. W istniejącym
shellu może też zachować nieaktualny status. Komunikat wrappera nie dowodził
niezerowego kodu zakończenia instalatora ONNX.

Pięć wrapperów zależności uruchamia teraz `pwsh -NoProfile -NonInteractive
-File ...` jako osobny proces i sprawdza jego rzeczywisty kod zakończenia.
Wyjątki i `exit 1` nie są ignorowane. Vulkan nadal eksportuje SDK poprzez
`GITHUB_ENV`/`GITHUB_PATH`, więc kolejne kroki otrzymują jego ścieżki.

ONNX sprawdza przypięty SHA256 również dla wcześniej pobranego ZIP. Sprawdza
kompletność i niezerowy rozmiar wymaganych headers/LIB/DLL zarówno na ścieżce
cache, jak i po rozpakowaniu; dopiero potem wypisuje `ready` i zwraca zero.
Niekompletny cache kończy się błędem. Dodatkowa kontrola przed CMake sprawdza
payloady wszystkich pięciu zależności i zapisuje manifest SHA256.

Samodzielna regresja PowerShell odtwarza niezainicjalizowane LASTEXITCODE,
sprawdza sukces procesu potomnego przy pustym/starym statusie rodzica oraz
propagację wyjątku i jawnego kodu 7. Nie pobiera zależności ani nie używa radia.
Pozostaje komplet 36 wymaganych testów CTest i dotychczasowe bramki pakowania.

Kontrole lokalne są statyczne. Regresja PowerShell, kompilacja Windows, testy
i pakowanie mają zostać wykonane przez CI; ich wyniku nie zadeklarowano.
