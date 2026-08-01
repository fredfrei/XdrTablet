# Empfangseinstellungen und RDS

Diese Erweiterung ergänzt den Linux-Prototypen um:

- Stereo-Automatik / erzwungenes Mono (`B0` / `B1`)
- automatische und manuelle Bandbreite (`W...`)
- De-Emphasis 50 µs, 75 µs und 0 µs (`D0` bis `D2`)
- AGC-Schwelle (`A0` bis `A3`)
- RF- und IF-Zusatzverstärkung (`G00`, `G01`, `G10`, `G11`)
- PI-Code aus `P...`
- RDS-Gruppendecoder aus `R...`
- Sendername/PS (Gruppe 0A/0B)
- Radiotext (Gruppe 2A/2B)
- PTY und RDS-Empfangsanzeige

Die zuletzt gewählten Empfangseinstellungen werden mit QSettings gespeichert
und nach erfolgreichem Verbindungsaufbau erneut an den Tuner gesendet.

## Bauen

```bash
rm -rf build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/XdrTablet
```
