# HF5A etap 2.3 — eliminacja zbędnych żądań RX Audit

Baza: `48bb8ee78a2229e1035f6162d06f674e09923686`.
Gałąź: `hf5a/stage2-3-audit-requests-fix`.

Odpowiedź `26 00` uruchamiała odczyt szerokości filtra (`1A 03`),
wewnętrznego i zewnętrznego PBT (`14 07`, `14 08`). Zmiana DATA dodawała
odczyt ustawienia pasma TX (`1A 05`, pozycja 17 dla MK2 DATA).
Brak odpowiedzi na zabronione odczyty powodował ponowienie przy następnym
raporcie trybu: 11 raportów USB-D po USB daje `11 × 3 + 1 = 34` odmowy.
Jest to odtworzenie mechanizmu w kodzie; bez pełnego logu sprzętowego nie
przypisujemy każdej z 34 historycznych odmów do konkretnej ramki.

Audyt zachowuje przyjęcie stanu trybu/DATA/filtra i unieważnienie starego TX,
ale pomija dodatkowy inwentarz filtra/PBT/TBW, preampu i automatyczne zapisy
scope. Nie generuje również automatycznego cleanup PTT/CW przy konflikcie
identyfikacji ani rozszerzonego odczytu XFC. Produkcja zachowuje dotychczasowe
ścieżki. Pięć miejsc zwiększających licznik odmów pozostaje bez zmian.

Lista zapytań nadal ma dokładnie dziewięć pozycji: `19 00`, `0F`, `1C 03`,
`03`, `04`, `26 00`, `21 00`, `21 01`, `21 02`. Nie zmieniono polityki,
finalnego writer guard, autoryzacji, Busy ani wyłączności LAN.

Nowy test offline obejmuje odtworzenie 34 odmów, zwykły cykl audytu,
produkcję oraz pięć jawnie zabronionych operacji. Przechwytuje rzeczywiste
bajty w ostatnim writerze bez otwierania gniazd. Workflow pomocniczy buduje
AetherSDR, tester i 16 testów pakietu HF5A, zachowuje JUnit/logi i manifest.
Cztery mutacje celowo usuwają zabezpieczenia tylko w kopii runnera, wymagają
wykrycia regresji i odtwarzają oryginalne bajty źródeł. Nie zmieniają wydania.
Nie jest to wykonanie całego zestawu testów repozytorium.

## Przenośny tester

ZIP zawiera tylko `aether-icom-rx-audit.exe`, zależności, dokumentację,
`source.json` i manifest `SHA256SUMS.txt`; nie jest instalatorem.
Offline: `aether-icom-rx-audit.exe --self-check`.
Uruchomienie bez argumentów ma odmówić połączenia.
Nie zastępować produkcyjnego EXE. Nie uruchamiano połączenia z radiem;
próba sprzętowa wymaga osobnej zgody i wyłącznego dostępu LAN.
COM4/COM5 i konfiguracja produkcyjna pozostają nienaruszone.

Rzeczywiste wyniki CI, SHA commitu, EXE i ZIP oraz linki zostaną zachowane
w `hf5a-verification/stage2-3/REPORT.md` po zakończeniu joba.
