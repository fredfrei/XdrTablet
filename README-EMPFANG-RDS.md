# XdrTablet – Empfang und RDS

Diese Datei beschreibt die Empfangs- und RDS-Funktionen von **XdrTablet** in Verbindung mit einem **TEF6686**-Empfänger und der **PE5PVB TEF6686_ESP32-Firmware**.

## Empfang

XdrTablet kann den Empfänger über TCP/IP oder – auf unterstützten Desktop-Systemen – über USB/serielle Schnittstelle steuern.

Die eigentliche HF-Auswertung und der automatische Sendersuchlauf werden von der PE5PVB-Firmware auf dem ESP32 ausgeführt. XdrTablet zeigt die von der Firmware gelieferten Werte an und sendet die entsprechenden Steuerbefehle.

### Angezeigte Empfangswerte

Je nach Firmware und Betriebszustand werden unter anderem folgende Werte angezeigt:

- Signalstärke
- CCI
- ACI
- Stereo / Mono
- aktuelle Filterbandbreite
- RDS-Status

Die Statusmeldungen der PE5PVB-Firmware werden von XdrTablet aus den `Ss...`- bzw. `Sm...`-Telegrammen ausgewertet.

## Sendersuchlauf

Der Suchlauf wird mit den PE5PVB-Befehlen gestartet:

```text
C1 = Suchlauf abwärts
C2 = Suchlauf aufwärts
```

XdrTablet startet nur den Suchlauf. Die weitere Suche wird vollständig von der PE5PVB-Firmware ausgeführt.

Während der Suche meldet die Firmware die jeweils getestete Frequenz als `T...`.

Beispiel:

```text
C2
T88700
C0
T88800
T88900
T89000
T89100
T89200
```

`C0` bedeutet dabei **nicht**, dass der Suchlauf beendet ist. Die Firmware kann nach `C0` selbstständig weitere Frequenzen durchsuchen.

XdrTablet beendet lediglich seine eigene Suchlaufanzeige, wenn nach dem letzten `T...`-Frequenzschritt keine weiteren Frequenzänderungen mehr eintreffen.

## Suchempfindlichkeit

Die PE5PVB-Firmware verwendet für die FM-Suche den Parameter:

```text
I1 ... I30
```

Dabei gilt in XdrTablet:

```text
1  = strenger
30 = empfindlicher
```

Mit

```text
I0
```

wird der aktuell im ESP32 gespeicherte Wert abgefragt, ohne ihn zu verändern.

Der ESP32 speichert die eingestellte Suchempfindlichkeit selbst.

## Empfangseinstellungen

XdrTablet unterstützt derzeit folgende Empfängereinstellungen der PE5PVB-Firmware:

### Stereo / Mono

```text
B0 = Stereo-Automatik
B1 = Mono erzwingen
```

### Bandbreite

Die gewünschte Bandbreite wird mit dem `W`-Befehl gesetzt.

```text
W0
```

steht für automatische Bandbreite.

Die tatsächlich verwendete Bandbreite wird zusätzlich aus den Empfangsstatusmeldungen der Firmware gelesen und in XdrTablet angezeigt.

### De-Emphasis

Die De-Emphasis wird über den `D`-Befehl eingestellt.

### AGC

Die AGC-Einstellung wird über den `A`-Befehl gesteuert.

### Channel Equalizer und Multipath Suppression

Die beiden Funktionen werden über den PE5PVB-`G`-Befehl gesteuert.

Bei PE5PVB gilt:

```text
erste Stelle:
0 = Channel Equalizer EIN
1 = Channel Equalizer AUS

zweite Stelle:
0 = Multipath Suppression / iMS EIN
1 = Multipath Suppression / iMS AUS
```

Beispiel:

```text
G00
```

aktiviert beide Funktionen.

---

# RDS

XdrTablet wertet die von der PE5PVB-Firmware gelieferten RDS-Rohdaten selbst aus.

Unterstützt werden derzeit:

- PI
- PS
- PTY
- RadioText
- RT+
- Clock Time (CT)
- RDS-Aktivitätsanzeige

## PI

Der PI-Code wird als vierstelliger Hexadezimalwert angezeigt.

Beispiel:

```text
F731
```

## PS

PS ist der maximal 8 Zeichen lange Sendername.

Ein vollständiger PS-Name besteht aus vier Segmenten zu jeweils zwei Zeichen.

Beispiel:

```text
Segment 0: AC
Segment 1: CE
Segment 2: NT
Segment 3:  4

Ergebnis:
ACCENT 4
```

### Aktueller Stand

XdrTablet sammelt die vier PS-Segmente und setzt daraus den angezeigten Sendernamen zusammen.

Bei gestörtem RDS-Empfang kann momentan noch ein einzelnes fehlerhaftes oder korrigiertes Segment einen bereits empfangenen Teil des PS-Namens überschreiben.

Eine zusätzliche Mehrfachbestätigung der einzelnen PS-Segmente ist daher als mögliche weitere Verbesserung vorgesehen.

## PTY

PTY beschreibt den Programmtyp eines Senders.

XdrTablet verwendet die europäische RDS-PTY-Tabelle.

Beispiele:

```text
0  = Kein PTY
1  = Nachrichten
4  = Sport
10 = Popmusik
11 = Rockmusik
```

Wenn ein Sender PTY-Code `0` sendet, zeigt XdrTablet:

```text
Kein PTY
```

Das ist kein Fehler der Anzeige, sondern der empfangene RDS-PTY-Wert.

## RadioText

RadioText wird aus RDS-Gruppe 2A bzw. 2B zusammengesetzt.

XdrTablet sammelt die einzelnen Segmente und berücksichtigt dabei das RadioText-A/B-Flag.

Ein Wechsel des A/B-Flags bedeutet, dass ein neuer RadioText begonnen hat und die bisherige Sammlung verworfen werden muss.

## RT+

RT+ erweitert RadioText um strukturierte Informationen, insbesondere:

- Titel
- Interpret

XdrTablet erkennt die RT+-ODA-Kennung `4BD7` und wertet die zugehörigen RT+-Daten aus.

Titel und Interpret werden nur übernommen, wenn die benötigten Daten vollständig und plausibel zum aktuell empfangenen RadioText passen.

Bei einem neuen RT+-Item oder einem STOP-Status werden alte Titel-/Interpret-Daten verworfen.

## Clock Time (CT)

Wenn der Sender eine gültige RDS-Zeit überträgt, kann XdrTablet Datum und Uhrzeit aus der RDS-Gruppe 4A auswerten.

Unplausible Datums- oder Zeitzonenwerte werden verworfen.

---

# RDS-Fehlerkorrektur

XdrTablet besitzt einen Schalter für die RDS-Fehlerkorrektur.

Dieser Schalter beeinflusst die Auswertung von:

- PS
- PTY
- RadioText
- RT+
- CT

## Fehlerkorrektur AUS

Es werden nur RDS-Blöcke mit Fehlerstatus `0` verwendet.

```text
Status 0 = fehlerfrei
```

Das ist die strengere Einstellung und kann bei schwächerem Empfang dazu führen, dass RDS-Informationen langsamer oder gar nicht erscheinen.

## Fehlerkorrektur EIN

Zusätzlich dürfen vom TEF6686 korrigierte RDS-Blöcke verwendet werden:

```text
Status 0 = fehlerfrei
Status 1 = korrigiert
Status 2 = korrigiert
Status 3 = unbrauchbar
```

Status `3` wird immer verworfen.

Die eingeschaltete Fehlerkorrektur kann die RDS-Dekodierung bei schwachem Empfang beschleunigen. Gleichzeitig steigt das Risiko, dass ein korrigierter Block einen falschen Zeichenwert enthält.

Beim Umschalten der Fehlerkorrektur werden die bereits gesammelten PS-, PTY-, RadioText-, RT+- und CT-Daten zurückgesetzt und mit der neuen Einstellung neu aufgebaut.

---

# Hinweise bei schlechtem RDS-Empfang

Wenn PS, RadioText oder RT+ unvollständig oder falsch erscheinen:

1. RDS-Fehlerkorrektur zunächst ausschalten.
2. Einige Sekunden auf derselben Frequenz warten.
3. Signalstärke sowie CCI/ACI beobachten.
4. Bei stark gestörtem Empfang eine andere Bandbreite oder Antennenausrichtung testen.
5. Bei sehr schwachen Sendern kann die aktivierte Fehlerkorrektur trotzdem hilfreich sein.

Ein starkes HF-Signal garantiert nicht automatisch fehlerfreies RDS. Mehrwegeempfang, Nachbarkanäle und Störungen können die RDS-Dekodierung beeinflussen.

---

# USB und Störmeldungen der ESP32-Firmware

Bei USB-Verbindung können Debug-Meldungen der ESP32-Firmware ebenfalls über die serielle Schnittstelle erscheinen, zum Beispiel WiFi-Fehlermeldungen.

Beispiel:

```text
[E][WiFiUdp.cpp:185] endPacket(): could not send data
```

Diese Meldungen stammen vom ESP32 und nicht vom RDS-Decoder von XdrTablet.

Sie können sich mit den normalen seriellen Statusmeldungen mischen. XdrTablet ignoriert unbekannte Zeilen weitgehend, dennoch ist eine saubere serielle Ausgabe der Firmware vorzuziehen.

---

# Aktueller Entwicklungsstand

Der derzeitige Stand umfasst:

- TCP-Verbindung
- USB/seriell auf unterstützten Desktop-Systemen
- automatische Erkennung verfügbarer serieller Ports
- PE5PVB-Sendersuchlauf über `C1` / `C2`
- Suchempfindlichkeit über `I1` bis `I30`
- automatische Abfrage der gespeicherten Suchempfindlichkeit mit `I0`
- Empfangsanzeige für Signal, CCI, ACI und Bandbreite
- Stereo-/Mono-Steuerung
- Bandbreite
- De-Emphasis
- AGC
- Channel Equalizer
- Multipath Suppression / iMS
- PI
- PS
- PTY
- RadioText
- RT+
- CT
- umschaltbare RDS-Fehlerkorrektur

## Noch mögliche Verbesserung

Die PS-Anzeige kann zukünftig noch robuster gemacht werden, indem jedes der vier 2-Zeichen-Segmente mehrfach identisch empfangen werden muss, bevor es als gültig übernommen wird.

Dies würde insbesondere bei schwachem oder stark gestörtem RDS-Empfang verhindern, dass einzelne fehlerhafte Segmente kurzzeitig einen korrekten PS-Namen überschreiben.
