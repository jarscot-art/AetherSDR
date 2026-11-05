# HF5A 4.1 — start i zamknięcie desktopu

Dowód: [CI 42039706791](https://github.com/jarscot-art/AetherSDR/actions/runs/42039706791),
artefakt `hf5a-stage4-offline-42039706791`, plik
`hf5a-desktop-stage4-JTAQsr/logs/aethersdr-20261105-144128.log`.

Desktop został utworzony i rozpoczął zamykanie. Ostatni komunikat fatalny:
`QThread: Destroyed while thread 'SpotClients' is still running`.
Log zawiera zakończenie destruktora MainWindow, ale nie zawiera wykonania
`main_window.close_event` ani `spots.thread.join`.

Przyczyną jest wywołanie `app.exit()` bezpośrednio z timera self-check w
`src/main.cpp`. Zatrzymuje ono pętlę zdarzeń bez `MainWindow::closeEvent()`,
gdzie znajdują się `m_spotThread->quit()` i `wait(3000)`. Następnie destrukcja
aktywnego QThread kończy proces. Ostrzeżenia QRhi/offscreen również występują
w logu, ale nie są dowodem przyczyny tego fatalnego zakończenia.

Poprawka wywołuje najpierw `window.close()`, a dopiero potem kończy pętlę
z wynikiem zależnym od asercji i przyjęcia zamknięcia. Nie zmienia produkcyjnej
obsługi zamykania, renderera ani funkcji radia. Workflow wymaga śladów
zakończenia closeEvent, dołączenia SpotClients i zakończenia aplikacji.

Pominięcie `app_settings_safety_explicit-profile-path-isolation` wynikało
z bezpiecznej odmowy utworzenia natywnego pliku poza QTemporaryDir: Windows
Known Folders ignoruje przekierowanie HOME. Test Windows alokuje teraz własny,
unikalny katalog pod natywną lokalizacją Qt w trybie testowym, następnie kieruje
AppConfigLocation do tego katalogu. Przed zapisem sprawdza dokładną zgodność
ścieżek. Weryfikuje rzeczywistą migrację AppSettings: zwykły plik XML pozostaje
niezmieniony i nie jest importowany do jawnego profilu. Nie zmienia kodu
produkcji ani semantyki SKIP na pozostałych platformach.

Portable ZIP wymaga wykonania i zaliczenia wszystkich 31 testów; SKIPPED,
brak wyników lub brak dowodów poprawnego zamknięcia blokuje pakowanie.
Zachowano istniejący niezależny `hf5a_desktop_profile_test`, blokadę sieci
w CI oraz publikowanie logów również przy błędzie. Lokalne kontrole są
statyczne; wykonanie poprawionych testów Windows wymaga niezależnej oceny CI.
