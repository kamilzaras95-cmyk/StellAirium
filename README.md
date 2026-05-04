# StellAirium

Stellarium plugin pokazujący samoloty na żywo na sferze niebieskiej.

Dane ADS-B pobierane bezpośrednio od publicznych providerów (adsb.fi, airplanes.live) — bez pośrednika.

## Funkcje

- Sylwetki samolotów na mapie nieba, obracane zgodnie z kursem
- Kolory per typ (odrzutowiec, śmigłowiec, lekki, cargo…)
- Dead-reckoning między kolejnymi fetchami — płynny ruch
- Kliknięcie → panel ze szczegółami (callsign, ICAO type, altitude, speed, squawk…)
- Konfigurowalny zasięg (50–500 nm), źródło danych i interwał odświeżania

## Instalacja

### macOS

1. Pobierz `StellAirium-macOS-arm64.zip` z [Releases](../../releases/latest)
2. Wypakuj i skopiuj do katalogu pluginów Stellarium:

```bash
mkdir -p ~/Library/Application\ Support/Stellarium/modules/StellAirium
cp StellAirium/libStellAirium.dylib ~/Library/Application\ Support/Stellarium/modules/StellAirium/
cp StellAirium/module.ini ~/Library/Application\ Support/Stellarium/modules/StellAirium/
codesign --sign - --force --timestamp=none \
  ~/Library/Application\ Support/Stellarium/modules/StellAirium/libStellAirium.dylib
```

3. Uruchom Stellarium → **Konfiguracja → Wtyczki** → `StellAirium` → zaznacz **Załaduj przy starcie** → restart

### Windows

1. Pobierz `StellAirium-Windows-x64.zip` z [Releases](../../releases/latest)
2. Wypakuj i skopiuj do katalogu pluginów Stellarium:

```
%APPDATA%\Stellarium\modules\StellAirium\
```

3. Uruchom Stellarium → **Konfiguracja → Wtyczki** → `StellAirium` → zaznacz **Załaduj przy starcie** → restart

## Wymagania

- Stellarium 0.21.0 lub nowszy
- Połączenie z internetem (dane ADS-B)

## Licencja

GPL v2 — plugin jest pochodną Stellarium.

---

Autor: [astronow.pl](https://astronow.pl) / Kamil Zaraś
