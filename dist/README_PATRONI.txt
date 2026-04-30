═════════════════════════════════════════════════════════════
  AVIARIUM — Stellarium z samolotami na żywo
  wersja 0.0.7 / build 30-04-2026 / dla patronów astronow.pl
═════════════════════════════════════════════════════════════


CO TO JEST?
───────────

Aviarium to specjalna wersja Stellarium 26.1 z wbudowanym pluginem
Aurora Aircraft — pokazuje samoloty na żywo na sferze niebieskiej,
w realnych pozycjach z dokładnością do paru sekund.

Dane ADS-B pobieramy bezpośrednio z opendata.adsb.fi co 15 sekund
z TWOJEGO IP (nie przez serwer astronow.pl). Wszystko lokalnie.


URUCHOMIENIE — macOS Apple Silicon (M1/M2/M3/M4)
────────────────────────────────────────────────

1. Skopiuj Aviarium.app do /Applications (albo gdzie chcesz).

2. Otwórz Aviarium.app klikając PRAWYM przyciskiem → "Otwórz".
   Standardowe podwójne kliknięcie nie zadziała za pierwszym razem
   bo aplikacja nie jest podpisana certyfikatem Apple Developer ID
   (kosztowny, pomijam dla wersji patronowej).

3. macOS pokaże ostrzeżenie typu:
   "Aviarium nie może być sprawdzona pod kątem złośliwego oprogramowania"
   → kliknij "Otwórz" / "Open Anyway".

   Powtórz: PRAWY KLIK → OTWÓRZ. Standardowe dwukliknięcie pokaże
   tylko "nie da się otworzyć" bez opcji bypass.

   Po pierwszym uruchomieniu kolejne uruchomienia działają normalnie.


PIERWSZE KROKI W APLIKACJI
──────────────────────────

1. Naciśnij F6 → ustaw swoją lokalizację (miasto + kraj).

2. Plugin Aurora Aircraft jest WŁĄCZONY DOMYŚLNIE.
   Jeśli z jakiegoś powodu nie widzisz samolotów:
     F2 (Configuration) → zakładka "Plugins" → "Aurora Aircraft"
     → zaznacz "Load at startup" → restart aplikacji.

3. W prawym górnym rogu zobaczysz dyskretny pasek statusu:
     "✈ Aurora Aircraft — N aloft / M in radius"
   N = ile samolotów aktualnie nad Twoim horyzontem,
   M = ile w promieniu 250 mil morskich (~460 km).

4. Samoloty na sferze niebieskiej:
     niebieski   — samolot pasażerski (jet, np. B738/A320)
     fioletowy   — wide-body (heavy, np. B777/A350)
     mintowy     — mały samolot (Cessna, Piper)
     pomarańczowy— helikopter
     żółty       — szybowiec
     różowy      — dron

5. KLIK na samolot → po lewej stronie zobaczysz pełen info-panel:
     callsign + kod typu ICAO (np. LOT3911 [B738])
     altitude w metrach + Flight Level
     prędkość naziemna w knots i km/h
     kurs (true track)
     vertical rate ze strzałką ↑/↓
     dystans poziomy do Ciebie


CO ZROBIĆ JEŚLI NIE DZIAŁA
──────────────────────────

▸ "Brak samolotów" — sprawdź czy lokalizacja w Stellarium jest
  ustawiona prawidłowo (F6). Jeśli jesteś na środku oceanu, nie
  zobaczysz nic. W Polsce zwykle widać 50-150 samolotów.

▸ "Pasek statusu pokazuje 0 aloft" — patrz wyżej. Jeśli jesteś
  w obszarze gdzie pora dnia/noc nie pokrywa się z aktywnością
  lotniczą, naturalnie samolotów jest mało.

▸ "Samoloty się nie ruszają" — odśwież co 15 sekund. Jeśli nigdy,
  prawdopodobnie brak internetu albo adsb.fi czasowo niedostępny.
  Otwórz log: ~/Library/Application Support/Stellarium/log.txt
  i poszukaj linii z "[AuroraAircraft]".

▸ "Klik nie pokazuje info-panel" — kliknij dokładnie na ikonkę.
  Stellarium wybiera najbliższy obiekt — gwiazdy bardzo blisko
  samolotu mogą wygrać. Obracaj kamerą i zoomuj (mouse wheel).

▸ "Zawiesza się / nie odpala" — zgłoś z plikiem log.txt
  (~/Library/Application Support/Stellarium/log.txt).


ZNANE OGRANICZENIA TEJ WERSJI
─────────────────────────────

▸ Sylwetka samolotu jest jedna (jet) — kolor zmienia się per typ,
  ale rozpoznasz heli bo będzie pomarańczowy, nie po kształcie.
  W kolejnej wersji dorobie osobne sylwetki.

▸ Pozycja ekstrapolowana liniowo między fetchami. Przy zakrętach
  samolot może chwilowo "wyjść" z prawdziwej pozycji o kilkaset
  metrów, póki nie przyjdzie kolejny fetch (max 15s).

▸ Czas na zegarze Stellarium musi być "teraz" — jeśli przewiniesz
  kalendarz w przeszłość/przyszłość, plugin nadal pokaże samoloty
  z bieżącej chwili (nie ma historycznych danych ADS-B).

▸ Aviarium używa tego samego folderu konfiguracyjnego co zwykły
  Stellarium (~/Library/Application Support/Stellarium). Twoje
  ustawienia będą synchronizowane między oboma aplikacjami.


ATRYBUCJA
─────────

Aviarium = Stellarium 26.1 + Aurora Aircraft plugin.

▸ Stellarium: GPL v2+, https://stellarium.org
▸ Aurora Aircraft plugin: GPL v2+, kod open source na życzenie
▸ Dane ADS-B: opendata.adsb.fi (community feeders worldwide)
▸ Build: astronow.pl / Kamil Zaras, 2026


KONTAKT / FEEDBACK
──────────────────

Cokolwiek nie działa, masz pytanie albo pomysł:
  email:   kamilzaras95@gmail.com
  www:     https://astronow.pl

Dzięki że jesteś. — Kamil
