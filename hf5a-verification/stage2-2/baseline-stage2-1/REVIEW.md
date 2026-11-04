# Etap 2.1 — przegląd zakresu poprawki

Kod: `f069490fa2c9e1a2153a7490be1ef7fca58196ea`.
Baza: `11b1480d6270cd2942c767c11046543735a8fbfe`.
Wyniki Windows zostaną zapisane osobno w raporcie końcowym wraz z surowymi logami.

## Potwierdzony błąd

W bazowym `IcomSession.cpp:430` wynik `parseConnectionUsage(...).kind == Busy`
kończył sesję audytu przed korelacją SID i przed oceną grantu. Publiczny przykład
kappanhang pokazuje, że przydział własnych strumieni ma ten sam kształt:
`0x90`, inner length `0x80`, reply/type `03 00`, busy LE32 `1`, właściciel i IPv4.
Test `icom_protocol_test` już w bazie potwierdzał nakładanie się obu parserów.

Nie mamy pełnej ramki z pierwszej próby HF5A. Naprawiamy wykazaną wadę klasyfikacji,
ale nie przedstawiamy odtworzenia syntetycznego jako zapisu tamtej próby sprzętowej.

## Zachowanie poprawki

1. `IcomProtocol.cpp:577`: wspólny klasyfikator audytu najpierw sprawdza SID,
   następnie ścisły format, AuthId i RadioId. Nie przejmuje tokenu z `0x90`.
2. `IcomSession.cpp:489`: audyt nie przechodzi już do heurystyki grantu używanej
   przez produkcję. Obsługuje osobno Ignore, Available, Grant, OwnSession, Busy,
   Ambiguous. Numeracja w logu odpowiada tej kolejności, od 0 do 5.
3. `IcomSession.cpp:533`: przed żądaniem strumieni wymagane jest skorelowane
   potwierdzenie braku właściciela. Własne busy=1 wymaga oczekiwania na grant,
   wcześniejszej dostępności, nazwy właściciela z tego logowania i lokalnego IPv4.
4. `IcomSession.cpp:269`: nazwa logowania audytu jest nowa dla każdej próby;
   klient produkcyjny nadal wysyła `icom-pc`. Równoległe klienty na tym samym
   komputerze nie są rozpoznawane wyłącznie na podstawie wspólnego IP.
5. `IcomStream.cpp:287`: dodano wyłącznie odczyt lokalnego adresu połączonego
   gniazda. Żadna ścieżka zapisu UDP ani polityka wyjściowa nie została zmieniona.
6. `stop()` zeruje dane korelacji. Duplikat własnego statusu nie otwiera strumieni
   ponownie, a późniejsze potwierdzenie zwolnienia radia kończy sesję.

## Testy i ich granice

Nowe testy klasyfikatora i dispatchera obejmują poprawny grant, innego właściciela
na tym samym IP, zmieniony IP, stare oba SID, obie części AuthId, inny RadioId,
brak właściciela/IP, błędne długości i pola, niespodziewany grant, duplikat oraz
zwolnienie sesji. Test integracyjny obserwuje bajty na końcowym writerze i sprawdza,
że gniazda pozostają nieutworzone. Prawdziwe Busy nie otwiera strumieni; kolejny
pakiet nie może obejść stanu odmowy.

Dotychczasowy test egress zachowuje wszystkie asercje blokady; zmieniono tylko
ustawienie stanu gotowości w seamie oraz AuthId ramki wejściowej. Osobny nowy
test sprawdza rzeczywistą drogę Available → żądanie RX → grant.

To nie jest test interoperacyjności firmware. Odpowiedź z ukrytym właścicielem,
inną nazwą, NAT lub niezgodnym AuthId nie uzyska grantu. Powiadomienie Available
dostarczone przed uwierzytelnieniem/wyborem radia zostaje zignorowane; brak
późniejszego potwierdzenia powoduje upłynięcie ograniczonego czasu obserwacji.
Nie dodano obchodzenia tego warunku ani automatycznego przejmowania sesji.

RS-BA1 nie zapewnia tutaj kryptograficznego dowodu właściciela ani atomowej
rezerwacji między powiadomieniem Available i żądaniem strumieni. Nadal potrzebne
jest osobne zatwierdzenie testu sprzętowego i rzeczywista wyłączność LAN.
Samo logowanie może zakłócić istniejącą sesję — poprawka nie daje zgody na
uruchomienie obok produkcyjnego AetherSDR.
