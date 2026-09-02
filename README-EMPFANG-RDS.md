# XdrTablet – RDS-Empfang

Diese Datei beschreibt ausschließlich den **RDS-Empfang und die RDS-Auswertung in XdrTablet** in Verbindung mit einem TEF6686-Empfänger und der PE5PVB-TEF6686_ESP32-Firmware.

Sie behandelt nicht den allgemeinen Sendersuchlauf, die USB-Verbindung oder andere Tunerfunktionen.

## Überblick

XdrTablet wertet die von der Firmware gelieferten RDS-Rohdaten selbst aus.

Unterstützt werden derzeit:

- PI – Program Identification
- PS – Program Service Name
- PTY – Program Type
- RadioText
- RT+
- CT – Clock Time
- RDS-Aktivitätsanzeige
- Filterung anhand der RDS-Blockfehler
- wahlweise Nutzung korrigierter RDS-Blöcke

Die Qualität der angezeigten RDS-Daten hängt nicht nur von der Signalstärke ab. Auch Mehrwegeempfang, Nachbarkanäle, Störungen und einzelne fehlerhafte RDS-Blöcke können die Dekodierung beeinflussen.

## RDS-Rohdaten und Blockfehler

Die PE5PVB-Firmware liefert RDS-Gruppen an XdrTablet weiter. XdrTablet zerlegt sie in die RDS-Blöcke A, B, C und D und berücksichtigt zusätzlich die von der Firmware gemeldeten Fehlerzustände.

Für jeden Block steht ein Fehlerstatus zur Verfügung:

```text
0 = fehlerfrei
1 = korrigiert
2 = korrigiert
3 = unbrauchbar
```

Blöcke mit Status `3` werden nicht für die Textauswertung verwendet.

## PI – Program Identification

Der PI-Code identifiziert den empfangenen Sender bzw. das Programm. XdrTablet zeigt ihn als vierstelligen Hexadezimalwert an.

Beispiel:

```text
F731
```

Der PI-Code wird aus Block A gelesen.

Bei gestörtem Empfang kann ein falsch dekodierter PI-Code kurzfristig wechseln. Eine stabile PI-Anzeige ist deshalb ein guter Hinweis auf brauchbaren RDS-Empfang.

## PS – Program Service Name

PS ist der maximal 8 Zeichen lange RDS-Sendername. Er wird in vier Segmenten mit jeweils zwei Zeichen übertragen:

```text
Segment 0: Zeichen 1–2
Segment 1: Zeichen 3–4
Segment 2: Zeichen 5–6
Segment 3: Zeichen 7–8
```

Beispiel:

```text
Segment 0: AC
Segment 1: CE
Segment 2: NT
Segment 3:  4
```

ergibt:

```text
ACCENT 4
```

### Aktueller Stand

XdrTablet sammelt die vier PS-Segmente und setzt daraus den Sendernamen zusammen.

Bei gestörtem Empfang kann jedoch ein einzelnes fehlerhaftes oder korrigiertes Segment einen bereits korrekt empfangenen Teil des PS-Namens überschreiben. Dadurch können kurzzeitig Mischungen aus richtigen und falschen Zeichen entstehen.

### Geplante Verbesserung

Viel zu machen

## PTY – Program Type

PTY beschreibt die Programmart eines Senders. XdrTablet liest den PTY-Code aus Block B und verwendet die europäische RDS-PTY-Tabelle.

Beispiele:

```text
0  = Kein PTY
1  = Nachrichten
4  = Sport
10 = Popmusik
11 = Rockmusik
```

Wenn ein Sender PTY-Code `0` überträgt, zeigt XdrTablet:

```text
Kein PTY
```

Das ist dann kein Fehler der Anzeige, sondern der tatsächlich empfangene Wert.

## RadioText

RadioText wird über RDS-Gruppe 2A bzw. 2B übertragen. XdrTablet sammelt die einzelnen Segmente und setzt sie zu einem vollständigen Text zusammen.

Wichtig ist das RadioText-A/B-Flag. Ändert es sich, beginnt normalerweise ein neuer RadioText. XdrTablet verwirft dann die bisher gesammelten Segmente und beginnt neu, damit Textteile verschiedener Nachrichten nicht vermischt werden.

## RT+

RT+ erweitert RadioText um strukturierte Informationen. XdrTablet unterstützt insbesondere:

- Titel
- Interpret

Die RT+-Informationen beziehen sich auf Positionen innerhalb des aktuell empfangenen RadioTextes. Titel und Interpret können daher nur sinnvoll ausgewertet werden, wenn der dazugehörige RadioText vollständig und plausibel vorliegt.

XdrTablet erkennt die RT+-ODA-Kennung:

```text
4BD7
```

Bei einem neuen RT+-Item oder einem STOP-Status werden alte Titel-/Interpret-Daten verworfen.

## CT – Clock Time

RDS kann Datum und Uhrzeit übertragen. Diese Information befindet sich in RDS-Gruppe 4A.

XdrTablet kann daraus die lokale Uhrzeit einschließlich des übertragenen Zeitzonenversatzes berechnen. Unplausible Werte werden verworfen.

## RDS-Fehlerkorrektur

XdrTablet besitzt einen Schalter, der festlegt, welche RDS-Blöcke für die Auswertung verwendet werden dürfen.

Der Schalter betrifft insbesondere:

- PS
- PTY
- RadioText
- RT+
- CT

### Fehlerkorrektur AUS

Es werden nur Blöcke mit Fehlerstatus `0` verwendet:

```text
0 = fehlerfrei
```

Korrigierte Blöcke werden verworfen.

Diese Einstellung ist strenger und kann bei schwachem Empfang dazu führen, dass PS, PTY oder RadioText langsamer erscheinen. Dafür ist die Gefahr geringer, dass ein falsch korrigierter Block ein falsches Zeichen erzeugt.

### Fehlerkorrektur EIN

Zusätzlich werden korrigierte Blöcke akzeptiert:

```text
0 = fehlerfrei
1 = korrigiert
2 = korrigiert
3 = unbrauchbar
```

Status `3` wird weiterhin verworfen.

Diese Einstellung kann bei schwachem oder wechselhaftem Empfang mehr RDS-Daten liefern. Gleichzeitig steigt das Risiko, dass ein korrigierter Block trotzdem ein falsches Zeichen enthält.

Das ist besonders bei PS sichtbar, weil bereits ein einzelnes falsches 2-Zeichen-Segment den angezeigten Namen verändern kann.

## Umschalten der Fehlerkorrektur

Beim Umschalten sollen bereits gesammelte Daten nicht mit Daten der alten Filtereinstellung vermischt werden. Deshalb werden die betroffenen RDS-Daten neu aufgebaut.

Dazu gehören:

- PS
- PTY
- RadioText
- RT+
- CT

Nach dem Umschalten kann es einige Sekunden dauern, bis wieder vollständige RDS-Informationen angezeigt werden.

## Hinweise zum Testen

Wenn PS, PTY oder RadioText falsch erscheinen:

1. RDS-Fehlerkorrektur ausschalten.
2. Einige Sekunden auf derselben Frequenz warten.
3. Beobachten, ob sich PS und RadioText stabilisieren.
4. Danach die Fehlerkorrektur testweise wieder einschalten.
5. Vergleichen, ob mehr RDS-Daten erscheinen oder ob sich einzelne Zeichen verschlechtern.

Für die Fehlersuche ist ein Log hilfreich, in dem sowohl die RDS-Rohdaten als auch der Zeitpunkt des Umschaltens der Fehlerkorrektur enthalten sind.

## Aktueller Entwicklungsstand


Der RDS-Teil von XdrTablet unterstützt derzeit:
- Bei klick auf die RDS LED neuer RDSMonitor
- Bei klick auf die RDTMC LED neuer TMCMonitor
- PI
- PS
- PTY
- RadioText
- RadioText A/B
- RT+
- Titel und Interpret aus RT+
- CT
- RDS-Gruppenzähler
- RDS-Aktivitätsanzeige
- Filterung anhand der RDS-Blockfehler
- umschaltbare Verwendung korrigierter Blöcke
- TMC Anzeige

### Noch offen

viel zu viel
### Der Text wurde mit KI erstellt
