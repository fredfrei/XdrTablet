# Android-APK

Die Android-Version wird für **arm64-v8a** gebaut, verwendet Qt 6.8.2 und ist für Tablets im Querformat eingerichtet.

## Automatisch mit GitHub Actions

1. Die neuen Dateien committen und zu GitHub pushen.
2. Auf GitHub `Actions` öffnen.
3. Den Workflow `Android APK` öffnen.
4. Nach erfolgreichem Lauf unten das Artefakt `XdrTablet-Android-arm64` herunterladen.
5. Das ZIP entpacken; enthalten ist `XdrTablet-arm64-v8a-debug.apk`.

Der Workflow läuft außerdem automatisch, wenn relevante Quellcodedateien auf `main` geändert werden.

## Lokal bauen

Voraussetzungen:

- Qt 6.8.2 `android_arm64_v8a`
- Android SDK Platform 36
- Android Build Tools 36.0.0
- Android NDK 26.1.10909125
- JDK 17 oder neuer

Dann:

```bash
./build-android-local.sh
```

Die APK wird unterhalb von `build-android/` erzeugt.
