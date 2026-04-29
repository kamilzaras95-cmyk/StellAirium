# aurora-stellarium-aircraft

Stellarium plugin pokazujący samoloty na żywo na sferze niebieskiej.
Side-loadowy plugin dla patronów astronow.pl — bez weryfikacji w oficjalnym plugin manager Stellarium.

## Status

MVP — w trakcie projektowania.

## Architektura w skrócie

- C++/Qt plugin ładowany przez Stellarium ze ścieżki `~/.stellarium/modules/AuroraAircraft/`
- Fetch ADS-B z `https://opendata.adsb.fi/api/v2/lat/{lat}/lon/{lon}/dist/{dist}` co 2 s (z IP użytkownika, nie przez astronow.pl)
- Konwersja lat/lon/alt → AltAz przez `StelCore` Stellarium
- Render: ikonki + etykiety (callsign + ICAO type), klik dla szczegółów
- Dead-reckoning klatka po klatce (Stellarium update tick) między próbkami fetchu

## Roadmap

- [ ] Toolchain: Qt 6 + Stellarium source (SDK headers)
- [ ] Skeleton plugin (loads, pojawia się w "Configuration → Plugins")
- [ ] Background QTimer fetcher → adsb.fi → parsed `AircraftSnapshot`
- [ ] Coord transform via `StelCore::altAzToJ2000` (lub odwrotnie)
- [ ] Renderer: `StelPainter` z prostymi markerami
- [ ] GUI: panel z liczbą samolotów, sliderem zasięgu, source picker (adsb.fi/airplanes.live/custom)
- [ ] Dead-reckoning między próbkami (`gs`, `track`, `vert_rate`)
- [ ] Click handler → `StelObjectMgr` zwraca info o samolocie
- [ ] Build pipeline: macOS pierwszy, potem Linux, potem Windows
- [ ] README dla patronów: jak side-loadować

## Instalacja toolchain (macOS)

```bash
brew install qt
brew install --cask stellarium
git clone --depth 1 https://github.com/Stellarium/stellarium.git reference/stellarium-source
```

## Layout

```
aurora-stellarium-aircraft/
├── src/                        # plugin source (.cpp, .h)
├── cmake/                      # CMake helpers
├── reference/                  # klon Stellarium (gitignored) jako referencja + build base
│   └── stellarium-source/
├── docs/                       # notatki techniczne
└── CMakeLists.txt              # build pluginu
```

## Licencja

GPL v2 (Stellarium plugin = pochodna pracy Stellarium = ten sam licencjonowanie).
