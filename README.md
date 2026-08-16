
# XDR Tablet – Linux-Prototyp

Qt-Quick-Client für einen xdrd-kompatiblen TCP-Server. Die Anwendung verbindet
sich direkt mit dem ESP32/xdrd und benötigt keine WebSocket-Brücke.
# XdrTablet

Plattformübergreifender TCP-Client für einen XDR-kompatiblen FM-Tuner.

## Oberfläche

![XdrTablet](screenshots/Xdrtablet.png)

## Enthalten

- xdrd-Anmeldung mit SHA-1 aus Salt und Passwort
- leeres Passwort wird unterstützt
- Startsequenz `o...` → `x` → `OK`
- letzte bestätigte FM-Frequenz speichern und beim nächsten Start wiederherstellen
- Frequenzschritte von 100 kHz und 1 MHz
- Signalwert aus `S...`-Telegrammen
- CCI und ACI als Prozentwerte
- Stereo, Mono und erzwungenes Mono unterscheiden
- aktuelle dynamische Bandbreite anzeigen
- Mono-Automatik und manuelle Bandbreite steuern
- DX-Scan-Befehl senden

## Debian 13 bauen

```bash
sudo apt install build-essential cmake ninja-build \
  qt6-base-dev qt6-declarative-dev qml6-module-qtquick \
  qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  qml6-module-qtcore

rm -rf build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/XdrTablet
```

Oder:

```bash
./install-debian13-deps.sh
./build-linux.sh
./build/bin/XdrTablet
```
Oder:
 Appimage
 Über den Dateimanager (Grafisch)
 Machen Sie einen Rechtsklick auf die .AppImage-Datei.
 Wählen Sie Eigenschaften.
 Gehen Sie zum Reiter Zugriffsrechte (oder Berechtigungen).
 Setzen Sie den Haken bei „Der Datei erlauben, als Programm auszuführen“ oder setzen Sie das Häkchen bei Ausführbar / Jedermann.
 Schließen Sie das Fenster und doppelklicken Sie auf die Datei, um sie zu starten.

## Beispiel eines Signaltelegramms

```text
Sm40.85,97,84,84000
```

- `Sm`: Mono
- `40.85`: XDR-Signalwert
- `97`: CCI
- `84`: ACI
- `84000`: Bandbreite in Hz, Erweiterung deines Firmwarestands

## Neue Empfangs- und RDS-Funktionen

Siehe `README-EMPFANG-RDS.md`.
