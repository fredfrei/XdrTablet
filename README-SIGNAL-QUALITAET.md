# Signal- und Qualitätsanzeige

Diese Version wertet die regelmäßig vom Tuner gesendeten Signaltelegramme aus.
Ein Beispiel aus dem ESP32 lautet:

```text
Sm40.85,97,84,84000
```

Die Anzeige übernimmt daraus:

- `Sm` / `Ss` / `SM` / `SS`: Mono, Stereo und erzwungenes Mono
- `40.85`: XDR-Signalwert
- `97`: CCI in Prozent
- `84`: ACI in Prozent
- `84000`: aktuell verwendete Bandbreite in Hz

CCI und ACI werden wie in XDR-GTK als Werte von 0 bis 100 angezeigt. Der
Signalbalken verwendet die gleiche Grundskala bis 80, die XDR-GTK für die
Balkenanzeige benutzt.

## Neu bauen

```bash
rm -rf build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/XdrTablet
```
