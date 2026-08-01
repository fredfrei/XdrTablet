# Abstimmung und Suchlauf

Diese Version ergänzt:

- wählbare kleine Schritte: 50, 100 oder 200 kHz
- wählbare große Schritte: 500 kHz, 1 MHz oder 2 MHz
- Suchlauf aufwärts und abwärts
- jederzeitige Stopptaste
- einstellbare Suchschwelle (XDR-Signalwert 0–80)
- feste FM-Grenzen von 87,500 bis 108,000 MHz
- Speicherung der Schrittweiten, Suchschwelle und zuletzt bestätigten Frequenz

Der Suchlauf arbeitet clientseitig. Er stimmt mit dem normalen XDR-Befehl
`T<frequenz-in-kHz>` in kleinen Schritten ab, wartet 650 ms und mittelt die
empfangenen Signaltelegramme. Wird die eingestellte Schwelle erreicht, bleibt
er auf der Frequenz stehen. An der Bandgrenze endet der Suchlauf automatisch.

## Bauen unter Debian 13

```bash
rm -rf build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/XdrTablet
```
