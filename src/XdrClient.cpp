#include <QDate>
#include "XdrClient.h"
#include "TmcTablesGenerated.h"  // XDRTABLET_TMC_ECL_LCL_V1
// XDRTABLET_TMC_LCL_NAMES_V1: Meldungs-Key unabhängig vom Zeitpunkt des LTN-Empfangs.

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>

#include <QDate>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>
#include <QVector>
#include <QXmlStreamReader>

#include <algorithm>

#include <QCryptographicHash>
#include <QDebug>
#include <QSettings>
#include <QSerialPortInfo>
#include <QStringList>
#include <QtGlobal>

namespace {
constexpr qsizetype XdrSaltLength = 16;
constexpr int MinimumFmFrequencyKhz = 87500;
constexpr int MaximumFmFrequencyKhz = 108000;
constexpr int RdsTimeoutMs = 2600;
// XDRTABLET_TMC_ACTIVE_MESSAGES_V1
constexpr qint64 TmcMessageStaleMs = 15LL * 60LL * 1000LL;
constexpr int TmcCleanupIntervalMs = 60 * 1000;

int boundedBandwidth(int hz)
{
    return qBound(0, hz, 400000);
}

// XDRTABLET_TMC_MULTI_V1
struct TmcOptionalLabel {
    int type = -1;
    int value = -1;
    bool hasValue = false;
    bool truncated = false;
};

// XDRTABLET_TMC_ECL_CORRECTIONS_V1
bool tmcEventIsCancellation(int eventCode)
{
    const XdrTmcTables::EventInfo *eventInfo =
        XdrTmcTables::findEvent(eventCode);

    if (!eventInfo || !eventInfo->text[0])
        return false;

    const QString text =
        QString::fromUtf8(eventInfo->text).trimmed();

    return text.compare(
               QStringLiteral("Meldung aufgehoben"),
               Qt::CaseInsensitive) == 0 ||
           text.compare(
               QStringLiteral("message cancelled"),
               Qt::CaseInsensitive) == 0;
}

QString tmcEventText(int eventCode)
{
    const XdrTmcTables::EventInfo *eventInfo =
        XdrTmcTables::findEvent(eventCode);

    if (eventInfo && eventInfo->text[0])
        return QString::fromUtf8(eventInfo->text);

    return QStringLiteral("Unbekanntes Ereignis (Code %1)")
        .arg(eventCode);
}

QString tmcDurationText(int level, int eventCode)
{
    level &= 0x07;
    if (level == 0)
        return QStringLiteral("keine explizite Dauer (Code 0)");

    const XdrTmcTables::EventInfo *eventInfo =
        XdrTmcTables::findEvent(eventCode);

    const QString dtype =
        eventInfo && eventInfo->durationType[0]
            ? QString::fromUtf8(eventInfo->durationType)
            : QString();

    QString dtypeName;
    if (dtype == QStringLiteral("D"))
        dtypeName = QStringLiteral("dynamisch/kurz");
    else if (dtype == QStringLiteral("L"))
        dtypeName = QStringLiteral("länger andauernd");
    else if (dtype == QStringLiteral("(D)"))
        dtypeName = QStringLiteral("dynamisch, nur Persistenz/Verwaltung");
    else if (dtype == QStringLiteral("(L)"))
        dtypeName = QStringLiteral("länger andauernd, nur Persistenz/Verwaltung");

    if (!dtype.isEmpty()) {
        return QStringLiteral("Stufe %1/7, Typ %2 (%3)")
            .arg(level)
            .arg(dtype)
            .arg(dtypeName.isEmpty() ? dtype : dtypeName);
    }

    return QStringLiteral("Stufe %1/7 (Dauertyp unbekannt)")
        .arg(level);
}

QString tmcFormatLocation(const XdrTmcTables::LocationInfo *loc)
{
    if (!loc)
        return QString();

    QStringList parts;
    const QString road = QString::fromUtf8(loc->road);
    const QString name = QString::fromUtf8(loc->name);

    if (!road.isEmpty())
        parts << road;
    if (!name.isEmpty() && !parts.contains(name))
        parts << name;

    if (loc->lat >= 46.0 && loc->lat <= 56.5 &&
        loc->lon >= 4.0 && loc->lon <= 16.5) {
        parts << QStringLiteral("%1,%2")
                     .arg(loc->lat, 0, 'f', 5)
                     .arg(loc->lon, 0, 'f', 5);
    }

    return parts.join(QStringLiteral(" | "));
}

QString tmcLocationLine(int locationCode, int ltn)
{
    if (ltn == 1) {
        const auto *loc =
            XdrTmcTables::findLocation(locationCode);
        const QString value = tmcFormatLocation(loc);
        if (!value.isEmpty())
            return QStringLiteral("Ort (LTN 1): %1").arg(value);
        return QStringLiteral("Ort (LTN 1): Location %1 nicht gefunden")
            .arg(locationCode);
    }

    if (ltn >= 0) {
        return QStringLiteral(
            "Location %1 – LTN %2 ist nicht die eingebaute deutsche LTN-1-Tabelle")
            .arg(locationCode)
            .arg(ltn);
    }

    return QStringLiteral("Location %1 – LTN noch nicht empfangen")
        .arg(locationCode);
}

QStringList tmcSectionLines(int locationCode,
                            int direction,
                            int extent,
                            int ltn)
{
    QStringList lines;

    if (ltn != 1 || extent <= 0)
        return lines;

    const XdrTmcTables::LocationInfo *start =
        XdrTmcTables::findLocation(locationCode);
    if (!start) {
        lines << QStringLiteral("Abschnitt: nicht auflösbar");
        return lines;
    }

    QStringList codes;
    const XdrTmcTables::LocationInfo *current = start;
    const XdrTmcTables::LocationInfo *last = start;
    QString problem;

    for (int step = 0; step <= extent; ++step) {
        if (!current) {
            problem = QStringLiteral("Location fehlt");
            break;
        }

        codes << QString::number(current->code);
        last = current;

        if (step == extent)
            break;

        const int nextCode =
            direction >= 0
                ? current->posOffset
                : current->negOffset;

        if (nextCode <= 0) {
            problem =
                QStringLiteral("%1 bei Location %2 fehlt")
                    .arg(direction >= 0
                             ? QStringLiteral("pos_offset")
                             : QStringLiteral("neg_offset"))
                    .arg(current->code);
            break;
        }

        current = XdrTmcTables::findLocation(nextCode);
        if (!current) {
            problem =
                QStringLiteral("Location %1 fehlt in der LCL")
                    .arg(nextCode);
            break;
        }
    }

    lines << QStringLiteral("Abschnitt: %1 (%2)")
                 .arg(codes.join(QStringLiteral(" → ")))
                 .arg(direction >= 0
                          ? QStringLiteral("positive LCL-Richtung")
                          : QStringLiteral("negative LCL-Richtung"));

    const QString startText = tmcFormatLocation(start);
    const QString endText = tmcFormatLocation(last);

    if (!startText.isEmpty())
        lines << QStringLiteral("  Start: %1").arg(startText);
    if (last != start && !endText.isEmpty())
        lines << QStringLiteral("  Ende : %1").arg(endText);
    if (!problem.isEmpty()) {
        lines << QStringLiteral(
                     "  Hinweis: nur %1 von %2 Extent-Schritten auflösbar (%3)")
                     .arg(qMax(0, codes.size() - 1))
                     .arg(extent)
                     .arg(problem);
    }

    return lines;
}

int tmcLabelValueBits(int type)
{
    switch (type) {
    case 0x0:
    case 0x1:
        return 3;
    case 0x2:
    case 0x3:
    case 0x4:
        return 5;
    case 0x5:
    case 0x6:
    case 0x7:
    case 0x8:
        return 8;
    case 0x9:
        return 11;
    case 0xA:
    case 0xB:
    case 0xC:
    case 0xD:
        return 16;
    case 0xE:
        return 0;
    default:
        return -1;
    }
}

QVector<TmcOptionalLabel> tmcParseOptionalLabels(
    const QMap<int, quint32> &parts,
    int expectedMax)
{
    QVector<TmcOptionalLabel> labels;
    QString bits;

    for (int gsi = expectedMax; gsi >= 0; --gsi) {
        const quint32 part =
            parts.value(gsi) & 0x0FFFFFFFU;

        for (int bit = 27; bit >= 0; --bit) {
            bits.append(
                (part & (quint32(1) << bit))
                    ? QLatin1Char('1')
                    : QLatin1Char('0'));
        }
    }

    int pos = 0;

    while (pos + 4 <= bits.size()) {
        bool ok = false;
        const int type =
            bits.mid(pos, 4).toInt(&ok, 2);
        if (!ok)
            break;
        pos += 4;

        if (type == 0xF) {
            labels.push_back({type, -1, false, false});
            break;
        }

        const int nbits =
            tmcLabelValueBits(type);
        if (nbits < 0)
            break;

        if (nbits == 0) {
            labels.push_back({type, -1, false, false});
            continue;
        }

        if (pos + nbits > bits.size()) {
            const QString tail = bits.mid(pos);
            if (tail.contains(QLatin1Char('1')))
                labels.push_back({type, -1, false, true});
            break;
        }

        const int value =
            bits.mid(pos, nbits).toInt(&ok, 2);
        if (!ok)
            break;
        pos += nbits;

        // ALERT-C Null-Padding / Ende.
        if (type == 0x0 && value == 0)
            break;

        labels.push_back({type, value, true, false});
    }

    return labels;
}

QPair<int, int> tmcEffectiveDirectionExtent(
    int direction,
    int extent,
    const QVector<TmcOptionalLabel> &labels)
{
    int effectiveDirection = direction;
    int effectiveExtent = extent;

    for (const TmcOptionalLabel &label : labels) {
        if (label.truncated ||
            label.type != 0x1 ||
            !label.hasValue) {
            continue;
        }

        if (label.value == 2)
            effectiveDirection *= -1;
        else if (label.value == 6)
            effectiveExtent += 8;
        else if (label.value == 7)
            effectiveExtent += 16;
    }

    return {effectiveDirection, effectiveExtent};
}

QString tmcDistanceText(int code)
{
    if (code == 0)
        return QStringLiteral(">100 km");
    if (code >= 1 && code <= 10)
        return QStringLiteral("%1 km").arg(code);
    if (code >= 11 && code <= 15)
        return QStringLiteral("%1 km").arg(10 + (code - 10) * 2);
    return QStringLiteral("%1 km").arg(20 + (code - 15) * 5);
}

QString tmcControlText(int code)
{
    switch (code) {
    case 0: return QStringLiteral("Dringlichkeit erhöhen");
    case 1: return QStringLiteral("Dringlichkeit verringern");
    case 2: return QStringLiteral("Richtung umkehren");
    case 3: return QStringLiteral("Dauer invertieren");
    case 4: return QStringLiteral("Sprachausgabe invertieren");
    case 5: return QStringLiteral("Umleitung empfohlen/vorhanden");
    case 6: return QStringLiteral("Extent um 8 erhöhen");
    case 7: return QStringLiteral("Extent um 16 erhöhen");
    default: return QStringLiteral("Code %1").arg(code);
    }
}

QString tmcSupplementaryText(int code)
{
    switch (code) {
    case 1: return QStringLiteral("LKW wird empfohlen, das Gebiet weiträumig zu umfahren");
    case 2: return QStringLiteral("folgen Sie den Schildern");
    case 3: return QStringLiteral("der Umleitungsbeschilderung folgen");
    case 4: return QStringLiteral("eine Umleitung ist eingerichtet");
    case 5: return QStringLiteral("keine Umleitungsempfehlung");
    case 6: return QStringLiteral("Umleitung wird nicht mehr empfohlen");
    case 7: return QStringLiteral("für Großraum- und Schwertransporte");
    case 8: return QStringLiteral("für LKW");
    case 9: return QStringLiteral("für PKW und leichte LKW");
    case 10: return QStringLiteral("Schneeräumfahrzeuge im Einsatz");
    case 11: return QStringLiteral("Streufahrzeuge im Einsatz");
    case 12: return QStringLiteral("bitte vorsichtig fahren");
    case 13: return QStringLiteral("Abstand halten");
    case 14: return QStringLiteral("auf Signalregelung achten");
    case 15: return QStringLiteral("Vorsicht beim Überholen");
    case 16: return QStringLiteral("Vorsicht an der Kreuzung");
    case 17: return QStringLiteral("Überholen verboten");
    case 18: return QStringLiteral("Licht einschalten");
    case 19: return QStringLiteral("Nebelscheinwerfer einschalten");
    case 20: return QStringLiteral("bitte das Fahrzeug nicht verlassen");
    case 21: return QStringLiteral("Motor abstellen");
    case 22: return QStringLiteral("Fenster schließen und Lüftung abstellen");
    case 23: return QStringLiteral("Bremsen überprüfen");
    case 24: return QStringLiteral("Winterausrüstung empfohlen");
    case 25: return QStringLiteral("Schneeketten empfohlen");
    case 26: return QStringLiteral("Schneekettenpflicht");
    case 27: return QStringLiteral("links fahren");
    case 28: return QStringLiteral("rechts fahren");
    case 29: return QStringLiteral("Standstreifen mitbenutzen");
    case 30: return QStringLiteral("Standstreifen nicht befahren");
    case 31: return QStringLiteral("in der Gegenrichtung");
    case 32: return QStringLiteral("auf der Gegenfahrspur");
    case 33: return QStringLiteral("auf dem Überholstreifen");
    case 34: return QStringLiteral("auf der Kriechspur");
    case 35: return QStringLiteral("auf dem rechten Fahrstreifen");
    case 36: return QStringLiteral("auf dem mittleren Fahrstreifen");
    case 37: return QStringLiteral("auf dem linken Fahrstreifen");
    case 38: return QStringLiteral("auf der Fahrbahn");
    case 39: return QStringLiteral("Polizei regelt den Verkehr");
    case 40: return QStringLiteral("Unfallaufnahme");
    case 41: return QStringLiteral("Polizei fährt verstärkt Streife");
    case 42: return QStringLiteral("wegen Bergungsarbeiten");
    case 43: return QStringLiteral("auf Sicherungsposten achten");
    case 44: return QStringLiteral("P+R anfahren und öffentliche Verkehrsmittel benutzen");
    case 45: return QStringLiteral("nutzen Sie die öffentlichen Verkehrsmittel");
    case 46: return QStringLiteral("nur im Notfall fahren");
    case 47: return QStringLiteral("erhöhte Unfallgefahr");
    case 48: return QStringLiteral("auf Brücken");
    case 49: return QStringLiteral("in schattigen Bereichen");
    case 50: return QStringLiteral("an Tunnelein- oder -ausgängen");
    case 51: return QStringLiteral("Wintersperre");
    case 52: return QStringLiteral("Blockabfertigung");
    case 53: return QStringLiteral("in nördlicher Richtung");
    case 54: return QStringLiteral("in nordöstlicher Richtung");
    case 55: return QStringLiteral("in östlicher Richtung");
    case 56: return QStringLiteral("in südöstlicher Richtung");
    case 57: return QStringLiteral("in südlicher Richtung");
    case 58: return QStringLiteral("in südwestlicher Richtung");
    case 59: return QStringLiteral("in westlicher Richtung");
    case 60: return QStringLiteral("in nordwestlicher Richtung");
    case 61: return QStringLiteral("folgen Sie den Schildern mit den orangen Pfeilen");
    case 62: return QStringLiteral("nicht der Umleitungsbeschilderung folgen");
    case 63: return QStringLiteral("eine örtliche Umleitung ist eingerichtet");
    case 64: return QStringLiteral("obligatorische Umleitung eingerichtet");
    case 65: return QStringLiteral("für Fahrzeuge mit Verbrennungsmotoren");
    case 66: return QStringLiteral("für Fahrzeuge mit Dieselmotoren");
    case 67: return QStringLiteral("für Fahrzeuge mit Gasmotoren");
    case 68: return QStringLiteral("ortskundige Autofahrer werden gebeten, das Gebiet weiträumig zu umfahren");
    case 69: return QStringLiteral("für Allradfahrzeuge mit Winterreifen oder Schneeketten");
    case 70: return QStringLiteral("am Mittelstreifen");
    case 71: return QStringLiteral("für PKW");
    case 72: return QStringLiteral("für leichte LKW");
    case 73: return QStringLiteral("für Fahrzeuge mit Katalysatoren");
    case 74: return QStringLiteral("für Fahrzeuge ohne Katalysatoren");
    case 75: return QStringLiteral("für PKW mit Anhänger");
    case 76: return QStringLiteral("für Fahrzeuge mit Wohnanhänger");
    case 77: return QStringLiteral("für Fahrzeuge mit Anhänger");
    case 78: return QStringLiteral("für Schwerlastverkehr");
    case 79: return QStringLiteral("für Busse");
    case 80: return QStringLiteral("für Spezialfahrzeuge");
    case 81: return QStringLiteral("für Fahrzeuge mit hohen Aufbauten");
    case 82: return QStringLiteral("für Gefahrguttransporte");
    case 83: return QStringLiteral("für Großraum-, Schwer- und Gefahrguttransporte");
    case 84: return QStringLiteral("für KFZ mit geradzahligen Kennzeichen");
    case 85: return QStringLiteral("für KFZ mit ungeradzahligen Kennzeichen");
    case 86: return QStringLiteral("für alle Fahrzeuge");
    case 88: return QStringLiteral("für Durchgangsverkehr");
    case 89: return QStringLiteral("für Schienenverkehr");
    case 90: return QStringLiteral("im U-Bahn-Verkehr");
    case 91: return QStringLiteral("Gefahr");
    case 92: return QStringLiteral("Reparaturarbeiten");
    case 93: return QStringLiteral("Lotsendienst im Einsatz");
    case 94: return QStringLiteral("Rettungsfahrzeuge im Einsatz");
    case 95: return QStringLiteral("Verkehr wird an der Unfallstelle vorbeigeführt");
    case 96: return QStringLiteral("Explosionsgefahr");
    case 97: return QStringLiteral("Brandgefahr");
    case 98: return QStringLiteral("Strahlungsgefahr");
    case 99: return QStringLiteral("Gasaustritt");
    case 100: return QStringLiteral("Feuerwehr regelt den Verkehr");
    case 101: return QStringLiteral("Geschwindigkeitskontrollen");
    case 102: return QStringLiteral("die Geschwindigkeit ist begrenzt");
    case 103: return QStringLiteral("Geschwindigkeitsbeschränkung für LKW");
    case 104: return QStringLiteral("langsam fahren");
    case 105: return QStringLiteral("auf Geschwindigkeitsbeschränkung achten");
    case 106: return QStringLiteral("die empfohlene Geschwindigkeit beachten");
    case 107: return QStringLiteral("Schneeketten erforderlich");
    case 108: return QStringLiteral("Winterreifen oder Schneeketten erforderlich");
    case 109: return QStringLiteral("Rettungshubschrauber im Einsatz");
    case 110: return QStringLiteral("es haben sich bereits mehrere Unfälle ereignet");
    case 111: return QStringLiteral("fahren Sie bitte besonders vorsichtig");
    case 112: return QStringLiteral("vorsichtig an das Stauende heranfahren");
    case 113: return QStringLiteral("Sicherheitsabstand vergrößern");
    case 114: return QStringLiteral("keine unnötigen Lücken lassen");
    case 115: return QStringLiteral("fahren Sie zügig an der Unfallstelle vorbei");
    case 116: return QStringLiteral("nicht unnötig abbremsen");
    case 117: return QStringLiteral("auf Beschilderung achten");
    case 118: return QStringLiteral("Warnblinkanlage einschalten");
    case 119: return QStringLiteral("nicht rauchen");
    case 120: return QStringLiteral("kein offenes Feuer");
    case 121: return QStringLiteral("Mobiltelefone und Funkgeräte ausschalten");
    case 122: return QStringLiteral("Rettungsfahrzeuge überholen lassen");
    case 123: return QStringLiteral("Gasse für Rettungsfahrzeuge bilden");
    case 124: return QStringLiteral("an den Rand der Straße heranfahren");
    case 125: return QStringLiteral("warten Sie auf Führungsfahrzeug");
    case 126: return QStringLiteral("auf Lautsprecherdurchsagen der Polizei und der Rettungsdienste achten");
    case 128: return QStringLiteral("an der nächsten sicheren Stelle halten");
    case 129: return QStringLiteral("Gasse für Räum- und Streufahrzeuge frei halten");
    case 131: return QStringLiteral("benutzen Sie den rechten Fahrstreifen");
    case 132: return QStringLiteral("benutzen Sie den linken Fahrstreifen");
    case 133: return QStringLiteral("LKW den rechten Fahrstreifen benutzen");
    case 134: return QStringLiteral("LKW den linken Fahrstreifen benutzen");
    case 136: return QStringLiteral("nächsten Rast- oder Parkplatz anfahren");
    case 141: return QStringLiteral("auf der rechten Seite");
    case 142: return QStringLiteral("in der Mitte");
    case 143: return QStringLiteral("auf der linken Seite");
    case 144: return QStringLiteral("auf der Busspur");
    case 145: return QStringLiteral("auf der Spur für Fahrgemeinschaften");
    case 146: return QStringLiteral("auf dem Standstreifen");
    case 147: return QStringLiteral("auf der Notfallspur");
    case 148: return QStringLiteral("auf der Gegenfahrbahn");
    case 149: return QStringLiteral("auf der Schwerverkehrsspur");
    case 150: return QStringLiteral("auf der Nahverkehrsspur");
    case 151: return QStringLiteral("wegen Hitze");
    case 152: return QStringLiteral("wegen Frost");
    case 153: return QStringLiteral("für Fährbetrieb");
    case 154: return QStringLiteral("für Straßen in");
    case 155: return QStringLiteral("auf der Fernverkehrsspur");
    case 156: return QStringLiteral("auf der Schnellspur");
    case 157: return QStringLiteral("auf der Verbindungsfahrbahn");
    case 158: return QStringLiteral("auf der Parallelfahrbahn");
    case 159: return QStringLiteral("auf der rechten Parallelfahrbahn");
    case 160: return QStringLiteral("auf der linken Parallelfahrbahn");
    case 161: return QStringLiteral("in Tunnels");
    case 162: return QStringLiteral("in Anschlussstellen");
    case 163: return QStringLiteral("in Höhenlagen");
    case 164: return QStringLiteral("in tiefer gelegenen Gebieten");
    case 165: return QStringLiteral("im Kurvenbereich");
    case 166: return QStringLiteral("hinter einer Kuppe");
    case 167: return QStringLiteral("in der Innenstadt");
    case 168: return QStringLiteral("im Innenstadtbereich");
    case 169: return QStringLiteral("auf dem Langsamfahrstreifen");
    case 170: return QStringLiteral("auf der Wendespur");
    case 171: return QStringLiteral("wegen eines vorausgegangenen Unfalls");
    case 172: return QStringLiteral("wegen zu hoher Verkehrsbelastung");
    case 173: return QStringLiteral("wegen hoher Besucherzahlen");
    case 174: return QStringLiteral("wegen Urlaubsverkehrs");
    case 175: return QStringLiteral("wegen technischer Probleme");
    case 176: return QStringLiteral("stadteinwärts");
    case 177: return QStringLiteral("stadtauswärts");
    case 178: return QStringLiteral("bei der Einreise");
    case 179: return QStringLiteral("bei der Ausreise");
    case 180: return QStringLiteral("nichts Brennendes aus dem Fahrzeug werfen");
    case 181: return QStringLiteral("Verkehrshelfer regeln den Verkehr");
    case 185: return QStringLiteral("aufgrund von herabfallendem Eis");
    case 191: return QStringLiteral("Winterreifen empfohlen");
    case 192: return QStringLiteral("Winterreifen vorgeschrieben");
    case 195: return QStringLiteral("benutzen Sie die Fernverkehrsspur");
    case 196: return QStringLiteral("benutzen Sie die Nahverkehrsspur");
    case 197: return QStringLiteral("benutzen Sie die linke Parallelfahrbahn");
    case 198: return QStringLiteral("benutzen Sie die rechte Parallelfahrbahn");
    case 199: return QStringLiteral("benutzen Sie die LKW-Spur");
    case 200: return QStringLiteral("wegen eines  vorausgegangenen Vorfalls");
    case 201: return QStringLiteral("bilden Sie bitte Fahrgemeinschaften");
    case 202: return QStringLiteral("Ist heute Ihr autofreier Tag?");
    case 203: return QStringLiteral("wegen eines vorausgegangenen Ereignisses");
    case 204: return QStringLiteral("bitte die Bahn benutzen");
    case 205: return QStringLiteral("bitte die U-Bahn benutzen");
    case 206: return QStringLiteral("bitte die Straßenbahn benutzen");
    case 207: return QStringLiteral("bitte den Bus benutzen");
    case 208: return QStringLiteral("vorübergehend");
    case 209: return QStringLiteral("Fahrpreis für öffentliche Verkehrsmittel ist im Parkticket enthalten");
    case 210: return QStringLiteral("vermeiden Sie die Hauptverkehrszeiten");
    case 211: return QStringLiteral("für Urlaubsverkehr");
    case 212: return QStringLiteral("für Anwohner");
    case 213: return QStringLiteral("mehrfach");
    case 214: return QStringLiteral("am Tage");
    case 215: return QStringLiteral("in verkehrsarmen Zeiten");
    case 216: return QStringLiteral("in der Nacht");
    case 217: return QStringLiteral("bis auf weiteres");
    case 218: return QStringLiteral("für Ankunft");
    case 219: return QStringLiteral("für Abfahrt");
    case 220: return QStringLiteral("nur Zugang");
    case 221: return QStringLiteral("nur");
    case 222: return QStringLiteral("außer");
    case 223: return QStringLiteral("für Fernverkehr");
    case 224: return QStringLiteral("für Nahverkehr");
    case 225: return QStringLiteral("für Regionalverkehr");
    case 226: return QStringLiteral("unbestätigter Bericht");
    case 227: return QStringLiteral("auf Mautspuren mit Schalterbetrieb");
    case 231: return QStringLiteral("entschuldigen Sie etwaige Verzögerungen");
    case 232: return QStringLiteral("für Straßen nach");
    case 233: return QStringLiteral("für Straßen aus");
    case 234: return QStringLiteral("wir danken Ihnen für Ihre Mitarbeit");
    case 235: return QStringLiteral("Staulänge sehr langsam abnehmend");
    case 236: return QStringLiteral("Staulänge langsam abnehmend");
    case 237: return QStringLiteral("Staulänge schnell abnehmend");
    case 238: return QStringLiteral("Staulänge sehr schnell abnehmend");
    case 239: return QStringLiteral("bitte das Fahrzeug verlassen und den nächsten sicheren Platz aufsuchen");
    case 240: return QStringLiteral("für Besucher");
    case 241: return QStringLiteral("im Baustellenbereich");
    case 242: return QStringLiteral("aus");
    case 243: return QStringLiteral("nach");
    case 244: return QStringLiteral("in");
    case 245: return QStringLiteral("Staulänge zunehmend");
    case 246: return QStringLiteral("Staulänge sehr langsam zunehmend");
    case 247: return QStringLiteral("Staulänge langsam zunehmend");
    case 248: return QStringLiteral("Staulänge schnell zunehmend");
    case 249: return QStringLiteral("Staulänge sehr schnell zunehmend");
    case 250: return QStringLiteral("sehr kurzer Fahrtakt");
    case 251: return QStringLiteral("regelmäßiger Fahrtakt");
    case 252: return QStringLiteral("recht kurzer Fahrtakt");
    case 253: return QStringLiteral("fahrplanmäßiger Fahrtakt");
    case 255: return QStringLiteral("Staulänge abnehmend");
    default: return QString();
    }
}

QString tmcQuantifier5Text(int code)
{
    switch (code & 0x1F) {
    case 0: return QStringLiteral("bis zu 72 Stunden");
    case 1: return QStringLiteral("bis zu 5 Minuten");
    case 2: return QStringLiteral("bis zu 10 Minuten");
    case 3: return QStringLiteral("bis zu 15 Minuten");
    case 4: return QStringLiteral("bis zu 20 Minuten");
    case 5: return QStringLiteral("bis zu 25 Minuten");
    case 6: return QStringLiteral("bis zu 30 Minuten");
    case 7: return QStringLiteral("bis zu 35 Minuten");
    case 8: return QStringLiteral("bis zu 40 Minuten");
    case 9: return QStringLiteral("bis zu 45 Minuten");
    case 10: return QStringLiteral("bis zu 50 Minuten");
    case 11: return QStringLiteral("bis zu 1 Stunden");
    case 12: return QStringLiteral("bis zu 2 Stunden");
    case 13: return QStringLiteral("bis zu 3 Stunden");
    case 14: return QStringLiteral("bis zu 4 Stunden");
    case 15: return QStringLiteral("bis zu 5 Stunden");
    case 16: return QStringLiteral("bis zu 6 Stunden");
    case 17: return QStringLiteral("bis zu 7 Stunden");
    case 18: return QStringLiteral("bis zu 8 Stunden");
    case 19: return QStringLiteral("bis zu 9 Stunden");
    case 20: return QStringLiteral("bis zu 10 Stunden");
    case 21: return QStringLiteral("bis zu 11 Stunden");
    case 22: return QStringLiteral("bis zu 12 Stunden");
    case 23: return QStringLiteral("bis zu 18 Stunden");
    case 24: return QStringLiteral("bis zu 24 Stunden");
    case 25: return QStringLiteral("bis zu 30 Stunden");
    case 26: return QStringLiteral("bis zu 36 Stunden");
    case 27: return QStringLiteral("bis zu 42 Stunden");
    case 28: return QStringLiteral("bis zu 48 Stunden");
    case 29: return QStringLiteral("bis zu 54 Stunden");
    case 30: return QStringLiteral("bis zu 60 Stunden");
    case 31: return QStringLiteral("bis zu 66 Stunden");
    default: return QString();
    }
}

QString tmcOptionalLabelText(const TmcOptionalLabel &label,
                             int mainEventCode)
{
    if (label.truncated)
        return QStringLiteral("unvollständiges Label 0x%1")
            .arg(label.type, 0, 16)
            .toUpper();

    switch (label.type) {
    case 0x0:
        return label.hasValue
            ? QStringLiteral("Dauer/Persistenz: %1")
                  .arg(tmcDurationText(label.value, mainEventCode))
            : QStringLiteral("Dauer");
    case 0x1:
        return QStringLiteral("Steuerung: %1")
            .arg(tmcControlText(label.value));
    case 0x2:
        return QStringLiteral("Streckenlänge: %1 (Code %2)")
            .arg(tmcDistanceText(label.value))
            .arg(label.value);
    case 0x3:
        return QStringLiteral("Geschwindigkeit: Code %1")
            .arg(label.value);
    case 0x4: {
        const QString value =
            tmcQuantifier5Text(label.value);

        return value.isEmpty()
            ? QStringLiteral("Quantifier-5: Code %1")
                  .arg(label.value)
            : QStringLiteral("Dauer/Persistenz: %1 (Quantifier-5)")
                  .arg(value);
    }
    case 0x5:
        return QStringLiteral("Quantifier-8: %1").arg(label.value);
    case 0x6: {
        const QString value = tmcSupplementaryText(label.value);
        return value.isEmpty()
            ? QStringLiteral("Zusatzinformation: unbekannter Code %1")
                  .arg(label.value)
            : QStringLiteral("Zusatzinformation: %1 (Code %2)")
                  .arg(value)
                  .arg(label.value);
    }
    case 0x7:
        return QStringLiteral("Startzeit: Code %1").arg(label.value);
    case 0x8:
        return QStringLiteral("Endzeit: Code %1").arg(label.value);
    case 0x9:
        return QStringLiteral("Zusatzereignis: %1 - %2")
            .arg(label.value)
            .arg(tmcEventText(label.value));
    case 0xA:
    case 0xB:
    case 0xC:
    case 0xD: {
        QString name;
        if (label.type == 0xA)
            name = QStringLiteral("Umleitungsziel");
        else if (label.type == 0xB)
            name = QStringLiteral("Ziel");
        else if (label.type == 0xC)
            name = QStringLiteral("Präzise Location");
        else
            name = QStringLiteral("Cross-Link");

        const QString loc =
            tmcFormatLocation(
                XdrTmcTables::findLocation(label.value));

        return QStringLiteral("%1: Location %2%3")
            .arg(name)
            .arg(label.value)
            .arg(loc.isEmpty()
                     ? QString()
                     : QStringLiteral(" - %1").arg(loc));
    }
    case 0xE:
        return QStringLiteral("Separator");
    case 0xF:
        return QStringLiteral("Reserviert/Ende");
    default:
        return QStringLiteral("Label 0x%1: %2")
            .arg(label.type, 0, 16)
            .arg(label.value)
            .toUpper();
    }
}
}

XdrClient::XdrClient(QObject *parent) : QObject(parent)
{
    epgNetwork_ = new QNetworkAccessManager(this);

    epgRefreshTimer_.setInterval(30000);
    connect(&epgRefreshTimer_, &QTimer::timeout,
            this, &XdrClient::refreshEpg);
    epgRefreshTimer_.start();

    QSettings settings;
    const int savedFrequency =
        settings.value(QStringLiteral("radio/lastFrequencyKhz"),
                       MinimumFmFrequencyKhz).toInt();
    frequencyKhz_ = qBound(MinimumFmFrequencyKhz,
                           savedFrequency,
                           MaximumFmFrequencyKhz);

    smallStepKhz_ = qBound(
        10, settings.value(QStringLiteral("radio/smallStepKhz"), 100).toInt(), 1000);
    largeStepKhz_ = qBound(
        100, settings.value(QStringLiteral("radio/largeStepKhz"), 1000).toInt(), 5000);
    // PE5PVB Standardwert für FM scan sensitivity.
    seekThreshold_ = 4;

    forcedMono_ = settings.value(QStringLiteral("receiver/forcedMono"), false).toBool();
    bandwidthSettingHz_ = boundedBandwidth(
        settings.value(QStringLiteral("receiver/bandwidthHz"), 0).toInt());
    deemphasis_ = qBound(
        0, settings.value(QStringLiteral("receiver/deemphasis"), 0).toInt(), 2);
    agc_ = qBound(0, settings.value(QStringLiteral("receiver/agc"), 2).toInt(), 3);
    // PE5PVB: G erste Stelle = Channel EQ AUS, zweite Stelle = iMS AUS.
    // Alte XdrTablet-Einstellungen werden einmalig logisch invertiert übernommen.
    channelEqualizerEnabled_ = settings.contains(
        QStringLiteral("receiver/channelEqualizer"))
        ? settings.value(QStringLiteral("receiver/channelEqualizer")).toBool()
        : !settings.value(QStringLiteral("receiver/rfGain"), false).toBool();

    multipathSuppressionEnabled_ = settings.contains(
        QStringLiteral("receiver/multipathSuppression"))
        ? settings.value(QStringLiteral("receiver/multipathSuppression")).toBool()
        : !settings.value(QStringLiteral("receiver/ifGain"), false).toBool();

    residualTimer_.setSingleShot(true);
    residualTimer_.setInterval(100);

    authenticationTimer_.setSingleShot(true);
    authenticationTimer_.setInterval(3000);

    seekEvaluationTimer_.setSingleShot(true);
    seekEvaluationTimer_.setInterval(650);

    rdsTimeoutTimer_.setSingleShot(true);
    rdsTimeoutTimer_.setInterval(RdsTimeoutMs);

    tmcCleanupTimer_.setInterval(TmcCleanupIntervalMs);

    connect(&socket_, &QTcpSocket::connected, this, &XdrClient::onConnected);
    connect(&socket_, &QTcpSocket::disconnected, this, &XdrClient::onDisconnected);
    connect(&socket_, &QTcpSocket::readyRead, this, &XdrClient::onReadyRead);
    connect(&socket_, &QTcpSocket::errorOccurred, this, &XdrClient::onError);

    connect(&serial_, &QSerialPort::readyRead,
            this, &XdrClient::onSerialReadyRead);
    connect(&serial_, &QSerialPort::errorOccurred,
            this, &XdrClient::onSerialError);

    connect(&residualTimer_, &QTimer::timeout,
            this, &XdrClient::processResidualBuffer);
    connect(&authenticationTimer_, &QTimer::timeout,
            this, &XdrClient::onAuthenticationTimeout);
    connect(&seekEvaluationTimer_, &QTimer::timeout,
            this, &XdrClient::evaluateSeekStep);
    connect(&rdsTimeoutTimer_, &QTimer::timeout,
            this, &XdrClient::onRdsTimeout);
    connect(&tmcCleanupTimer_, &QTimer::timeout,
            this, &XdrClient::pruneStaleTmcMessages);
    tmcCleanupTimer_.start();
}

bool XdrClient::connected() const
{
    if (usbMode_)
        return serial_.isOpen();

    return socket_.state() == QAbstractSocket::ConnectedState;
}

QString XdrClient::connectionType() const
{
    return usbMode_
        ? QStringLiteral("usb")
        : QStringLiteral("tcp");
}

bool XdrClient::ready() const { return ready_; }
QString XdrClient::statusText() const { return statusText_; }
int XdrClient::frequencyKhz() const { return frequencyKhz_; }
bool XdrClient::signalAvailable() const { return signalAvailable_; }
double XdrClient::signalLevel() const { return signalLevel_; }
int XdrClient::cci() const { return cci_; }
int XdrClient::aci() const { return aci_; }
bool XdrClient::stereo() const { return stereo_; }
bool XdrClient::forcedMono() const { return forcedMono_; }
int XdrClient::bandwidthHz() const { return bandwidthHz_; }
int XdrClient::bandwidthSettingHz() const { return bandwidthSettingHz_; }
int XdrClient::deemphasis() const { return deemphasis_; }
int XdrClient::agc() const { return agc_; }
bool XdrClient::channelEqualizer() const
{
    return channelEqualizerEnabled_;
}

bool XdrClient::multipathSuppression() const
{
    return multipathSuppressionEnabled_;
}
bool XdrClient::rdsActive() const { return rdsActive_; }
QString XdrClient::piCode() const { return piCode_; }
QString XdrClient::eccCode() const { return eccCode_; }
QString XdrClient::pinText() const { return pinText_; }

bool XdrClient::epgAvailable() const { return epgAvailable_; }
QString XdrClient::epgNow() const { return epgNow_; }
QString XdrClient::epgNext() const { return epgNext_; }
QString XdrClient::psText() const { return psText_; }
QString XdrClient::radioText() const { return radioText_; }
int XdrClient::ptyCode() const { return ptyCode_; }
QString XdrClient::ptyText() const { return ptyText_; }
QString XdrClient::rtPlusTitle() const { return rtPlusTitle_; }
QString XdrClient::rtPlusArtist() const { return rtPlusArtist_; }
bool XdrClient::rtPlusItemRunning() const { return rtPlusItemRunning_; }
bool XdrClient::rtPlusItemRunningKnown() const { return rtPlusItemRunningKnown_; }
QString XdrClient::ctText() const { return ctText_; }
bool XdrClient::rdsErrorCorrectionEnabled() const
{
    return rdsErrorCorrectionEnabled_;
}
int XdrClient::rdsGroupCount() const { return rdsGroupCount_; }

// XDRTABLET_TMC_LED_TEST_V1
// XDRTABLET_TMC_SINGLE_V1
bool XdrClient::tmcActive() const { return tmcActive_; }
int XdrClient::tmcGroupCount() const { return tmcGroupCount_; }
int XdrClient::tmcLocationTableNumber() const { return tmcLocationTableNumber_; }
int XdrClient::tmcServiceId() const { return tmcServiceId_; }
int XdrClient::tmcSingleCount() const { return tmcSingleCount_; }
int XdrClient::tmcMultiCount() const { return tmcMultiCount_; }
int XdrClient::tmcMultiOrphanCount() const { return tmcMultiOrphanCount_; }
int XdrClient::tmcMessageCount() const { return tmcMessages_.size(); }
QString XdrClient::tmcMessagesText() const
{
    return tmcMessages_.join(QStringLiteral("\n\n"));
}

// XDRTABLET_TMC_ACTIVE_MESSAGES_V1
void XdrClient::rebuildTmcMessageList()
{
    tmcMessages_.clear();

    for (const QString &key : tmcMessageOrder_) {
        const auto it =
            tmcMessageTextByKey_.constFind(key);

        if (it != tmcMessageTextByKey_.cend())
            tmcMessages_.append(it.value());
    }
}

void XdrClient::removeTmcMessagesForLocation(int locationCode)
{
    const QString token =
        QStringLiteral("/%1/").arg(locationCode);

    bool changed = false;

    const QSet<QString> snapshot =
        tmcMessageKeys_;

    for (const QString &key : snapshot) {
        if (!key.contains(token))
            continue;

        tmcMessageKeys_.remove(key);
        tmcMessageLastSeenMs_.remove(key);
        tmcMessageTextByKey_.remove(key);
        tmcMessageOrder_.removeAll(key);
        changed = true;
    }

    if (changed)
        rebuildTmcMessageList();
}

void XdrClient::upsertTmcMessage(const QString &key,
                                 int eventCode,
                                 int locationCode,
                                 const QString &message)
{
    /*
     * "Meldung aufgehoben" kommt in der offiziellen ECL unter
     * mehreren Event-Codes vor. Deshalb nicht auf einen festen Code prüfen.
     */
    if (tmcEventIsCancellation(eventCode)) {
        removeTmcMessagesForLocation(locationCode);
        return;
    }

    const qint64 now =
        QDateTime::currentMSecsSinceEpoch();

    const bool known =
        tmcMessageKeys_.contains(key);

    tmcMessageKeys_.insert(key);
    tmcMessageLastSeenMs_[key] = now;
    tmcMessageTextByKey_[key] = message;

    /*
     * Wiederholungen aktualisieren nur lastSeen.
     * So springt die Liste durch die zyklische TMC-Ausstrahlung nicht ständig.
     */
    if (!known)
        tmcMessageOrder_.prepend(key);

    while (tmcMessageOrder_.size() > 50) {
        const QString oldestKey =
            tmcMessageOrder_.takeLast();

        tmcMessageKeys_.remove(oldestKey);
        tmcMessageLastSeenMs_.remove(oldestKey);
        tmcMessageTextByKey_.remove(oldestKey);
    }

    rebuildTmcMessageList();
}

void XdrClient::pruneStaleTmcMessages()
{
    if (tmcMessageLastSeenMs_.isEmpty())
        return;

    const qint64 cutoff =
        QDateTime::currentMSecsSinceEpoch() -
        TmcMessageStaleMs;

    bool changed = false;

    const QMap<QString, qint64> snapshot =
        tmcMessageLastSeenMs_;

    for (auto it = snapshot.cbegin();
         it != snapshot.cend();
         ++it) {

        if (it.value() >= cutoff)
            continue;

        const QString key =
            it.key();

        tmcMessageKeys_.remove(key);
        tmcMessageLastSeenMs_.remove(key);
        tmcMessageTextByKey_.remove(key);
        tmcMessageOrder_.removeAll(key);
        changed = true;
    }

    if (!changed)
        return;

    rebuildTmcMessageList();
    emit tmcChanged();
}
QString XdrClient::tmcLastRaw() const { return tmcLastRaw_; }


QString XdrClient::rdsFlagsText() const
{
    const auto flagText = [](int value) -> QString {
        if (value < 0)
            return QStringLiteral("--");
        return value
            ? QStringLiteral("1 / EIN")
            : QStringLiteral("0 / AUS");
    };

    QStringList lines;

    lines << QStringLiteral("TP: %1").arg(flagText(rdsTp_));
    lines << QStringLiteral("TA: %1").arg(flagText(rdsTa_));
    lines << QStringLiteral("MS: %1").arg(flagText(rdsMs_));

    QString di;

    for (int bit = 3; bit >= 0; --bit) {
        const int mask = 1 << bit;

        if (!(rdsDiSeenMask_ & mask))
            di += QLatin1Char('?');
        else
            di += (rdsDiMask_ & mask)
                ? QLatin1Char('1')
                : QLatin1Char('0');
    }

    lines << QStringLiteral("DI: %1  (d3..d0)").arg(di);

    return lines.join(QLatin1Char('\n'));
}

QString XdrClient::rdsAfText() const
{
    if (rdsAfLists_.isEmpty() &&
        rdsAfOtherCodes_.isEmpty()) {
        return QStringLiteral("--");
    }

    const auto frequencyList =
        [](const QSet<int> &values) -> QString {

        QList<int> list = values.values();
        std::sort(list.begin(), list.end());

        QStringList result;

        for (int khz : list) {
            result << QStringLiteral("%1")
                          .arg(
                              khz / 1000.0,
                              0, 'f', 1);
        }

        return result.join(
            QStringLiteral(", "));
    };

    QStringList lines;

    for (auto it = rdsAfLists_.cbegin();
         it != rdsAfLists_.cend();
         ++it) {

        const int baseKhz = it.key();
        const RdsAfListInfo &info = it.value();

        QString method;

        if (info.method == 1)
            method = QStringLiteral("A");
        else if (info.method == 2)
            method = QStringLiteral("B");
        else
            method = QStringLiteral("?");

        QString header =
            QStringLiteral("%1 MHz   Methode %2")
                .arg(
                    baseKhz / 1000.0,
                    0, 'f', 1)
                .arg(method);

        if (info.expectedCount >= 0) {
            header +=
                QStringLiteral("   N=%1")
                    .arg(info.expectedCount);
        }

        lines << header;

        if (info.method == 1) {

            const QString freqs =
                frequencyList(info.frequencies);

            if (!freqs.isEmpty()) {
                lines <<
                    QStringLiteral(
                        "  Liste: %1 MHz")
                        .arg(freqs);
            }
        }
        else if (info.method == 2) {

            const QString same =
                frequencyList(info.sameProgramme);

            if (!same.isEmpty()) {
                lines <<
                    QStringLiteral(
                        "  gleiches Programm: %1 MHz")
                        .arg(same);
            }

            const QString regional =
                frequencyList(
                    info.regionalProgramme);

            if (!regional.isEmpty()) {
                lines <<
                    QStringLiteral(
                        "  regional/ggf. abweichend: %1 MHz")
                        .arg(regional);
            }
        }

        lines << QString();
    }

    if (!rdsAfOtherCodes_.isEmpty()) {

        QList<int> codes =
            rdsAfOtherCodes_.values();

        std::sort(
            codes.begin(),
            codes.end());

        QStringList raw;

        for (int code : codes) {
            raw << QStringLiteral("0x%1")
                       .arg(
                           code,
                           2, 16,
                           QLatin1Char('0'))
                       .toUpper();
        }

        lines <<
            QStringLiteral(
                "Andere AF-Codes: %1")
                .arg(
                    raw.join(
                        QStringLiteral(", ")));
    }

    while (!lines.isEmpty() &&
           lines.last().isEmpty()) {
        lines.removeLast();
    }

    return lines.join(QLatin1Char('\n'));
}

QString XdrClient::rdsPtynText() const
{
    if (!rdsPtynSegments_[0] &&
        !rdsPtynSegments_[1]) {
        return QStringLiteral("--");
    }

    const QString result =
        rdsPtynBuffer_.trimmed();

    return result.isEmpty()
        ? QStringLiteral("--")
        : result;
}

QString XdrClient::rdsLanguageName(int code)
{
    static const QMap<int, QString> names = {
        {0x00, QStringLiteral("Unbekannt / nicht anwendbar")},
        {0x01, QStringLiteral("Albanisch")},
        {0x02, QStringLiteral("Bretonisch")},
        {0x03, QStringLiteral("Katalanisch")},
        {0x04, QStringLiteral("Kroatisch")},
        {0x05, QStringLiteral("Walisisch")},
        {0x06, QStringLiteral("Tschechisch")},
        {0x07, QStringLiteral("Dänisch")},
        {0x08, QStringLiteral("Deutsch")},
        {0x09, QStringLiteral("Englisch")},
        {0x0A, QStringLiteral("Spanisch")},
        {0x0B, QStringLiteral("Esperanto")},
        {0x0C, QStringLiteral("Estnisch")},
        {0x0D, QStringLiteral("Baskisch")},
        {0x0E, QStringLiteral("Färöisch")},
        {0x0F, QStringLiteral("Französisch")},
        {0x10, QStringLiteral("Friesisch")},
        {0x11, QStringLiteral("Irisch")},
        {0x12, QStringLiteral("Gälisch")},
        {0x13, QStringLiteral("Galicisch")},
        {0x14, QStringLiteral("Isländisch")},
        {0x15, QStringLiteral("Italienisch")},
        {0x16, QStringLiteral("Samisch")},
        {0x17, QStringLiteral("Latein")},
        {0x18, QStringLiteral("Lettisch")},
        {0x19, QStringLiteral("Luxemburgisch")},
        {0x1A, QStringLiteral("Litauisch")},
        {0x1B, QStringLiteral("Ungarisch")},
        {0x1C, QStringLiteral("Maltesisch")},
        {0x1D, QStringLiteral("Niederländisch")},
        {0x1E, QStringLiteral("Norwegisch")},
        {0x1F, QStringLiteral("Okzitanisch")},
        {0x20, QStringLiteral("Polnisch")},
        {0x21, QStringLiteral("Portugiesisch")},
        {0x22, QStringLiteral("Rumänisch")},
        {0x23, QStringLiteral("Rätoromanisch")},
        {0x24, QStringLiteral("Serbisch")},
        {0x25, QStringLiteral("Slowakisch")},
        {0x26, QStringLiteral("Slowenisch")},
        {0x27, QStringLiteral("Finnisch")},
        {0x28, QStringLiteral("Schwedisch")},
        {0x29, QStringLiteral("Türkisch")},
        {0x2A, QStringLiteral("Flämisch")},
        {0x2B, QStringLiteral("Wallonisch")}
    };

    return names.value(
        code,
        QStringLiteral("Code nicht in Tabelle"));
}

QString XdrClient::rdsLanguageText() const
{
    if (rdsLanguageCode_ < 0)
        return QStringLiteral("--");

    return QStringLiteral("0x%1  %2")
        .arg(rdsLanguageCode_,
             2, 16,
             QLatin1Char('0'))
        .arg(rdsLanguageName(rdsLanguageCode_))
        .toUpper()
        .replace(
            rdsLanguageName(rdsLanguageCode_).toUpper(),
            rdsLanguageName(rdsLanguageCode_));
}

QString XdrClient::rdsOdaText() const
{
    if (rdsOdaGroupCode_.isEmpty())
        return QStringLiteral("--");

    QStringList lines;

    for (auto it = rdsOdaGroupCode_.cbegin();
         it != rdsOdaGroupCode_.cend();
         ++it) {

        const quint16 aid = it.key();
        const int code = it.value();

        const int group = code >> 1;
        const QChar version =
            (code & 1)
                ? QLatin1Char('B')
                : QLatin1Char('A');

        QString known;

        if (aid == 0x4BD7)
            known = QStringLiteral("RT+");
        else if (aid == 0xCD46)
            known = QStringLiteral("TMC / ALERT-C");
        else if (aid == 0x0093)
            known = QStringLiteral("FM -> DAB");

        QString line =
            QStringLiteral(
                "AID %1  -> %2%3   %4x")
                .arg(
                    static_cast<int>(aid),
                    4, 16,
                    QLatin1Char('0'))
                .arg(group)
                .arg(version)
                .arg(rdsOdaCount_.value(aid))
                .toUpper();

        if (!known.isEmpty())
            line += QStringLiteral("  [%1]").arg(known);

        if (rdsOdaLastData_.contains(aid)) {
            line += QStringLiteral("  C=%1")
                .arg(
                    static_cast<int>(
                        rdsOdaLastData_.value(aid)),
                    4, 16,
                    QLatin1Char('0'))
                .toUpper();
        }

        lines << line;
    }

    return lines.join(QLatin1Char('\n'));
}

QString XdrClient::rdsEonText() const
{
    if (rdsEon_.isEmpty())
        return QStringLiteral("--");

    QStringList all;

    for (auto it = rdsEon_.cbegin();
         it != rdsEon_.cend();
         ++it) {

        const quint16 pi = it.key();
        const RdsEonInfo &info = it.value();

        QString ps = info.ps.trimmed();

        QString title =
            QStringLiteral("PI %1")
                .arg(
                    static_cast<int>(pi),
                    4, 16,
                    QLatin1Char('0'))
                .toUpper();

        if (!ps.isEmpty())
            title += QStringLiteral("  %1").arg(ps);

        all << title;

        all << QStringLiteral("  Gruppen: %1")
                   .arg(info.groups);

        if (info.tp >= 0) {
            all << QStringLiteral("  TP(ON): %1")
                       .arg(info.tp);
        }

        if (info.ta >= 0) {
            all << QStringLiteral("  TA(ON): %1")
                       .arg(info.ta);
        }

        if (info.pty >= 0) {
            all << QStringLiteral("  PTY(ON): %1  %2")
                       .arg(info.pty)
                       .arg(ptyName(info.pty));
        }

        QList<int> af = info.afKhz.values();
        std::sort(af.begin(), af.end());

        if (!af.isEmpty()) {
            QStringList f;

            for (int khz : af) {
                f << QStringLiteral("%1")
                         .arg(
                             khz / 1000.0,
                             0, 'f', 1);
            }

            all << QStringLiteral("  AF(ON): %1 MHz")
                       .arg(
                           f.join(
                               QStringLiteral(", ")));
        }

        if (!info.mappedAf.isEmpty()) {
            QStringList mapped =
                info.mappedAf.values();

            std::sort(
                mapped.begin(),
                mapped.end());

            for (const QString &m : mapped)
                all << QStringLiteral("  Mapped AF: %1").arg(m);
        }

        if (!info.linkage.isEmpty())
            all << QStringLiteral("  Linkage: %1")
                       .arg(info.linkage);

        if (!info.pin.isEmpty())
            all << QStringLiteral("  PIN(ON): %1")
                       .arg(info.pin);

        if (info.taBursts > 0)
            all << QStringLiteral("  14B/TA-Bursts: %1")
                       .arg(info.taBursts);

        QStringList variants;

        for (int v = 0; v < 16; ++v) {
            if (info.variantCounts[
                    static_cast<std::size_t>(v)] > 0) {

                variants << QStringLiteral("%1=%2x")
                    .arg(v)
                    .arg(
                        info.variantCounts[
                            static_cast<std::size_t>(v)]);
            }
        }

        if (!variants.isEmpty()) {
            all << QStringLiteral("  Varianten: %1")
                       .arg(
                           variants.join(
                               QStringLiteral(", ")));
        }

        for (auto raw =
                 info.lastVariantData.cbegin();
             raw !=
                 info.lastVariantData.cend();
             ++raw) {

            all << QStringLiteral(
                       "  V%1 zuletzt: C=%2")
                       .arg(raw.key())
                       .arg(
                           static_cast<int>(
                               raw.value()),
                           4, 16,
                           QLatin1Char('0'))
                       .toUpper();
        }

        all << QString();
    }

    while (!all.isEmpty() &&
           all.last().isEmpty()) {
        all.removeLast();
    }

    return all.join(QLatin1Char('\n'));
}

QString XdrClient::rdsGroupStats() const
{
    QStringList lines;

    for (int group = 0; group < 16; ++group) {
        const quint64 countA =
            rdsGroupTypeCounts_[static_cast<std::size_t>(group * 2)];

        const quint64 countB =
            rdsGroupTypeCounts_[static_cast<std::size_t>(group * 2 + 1)];

        lines << QStringLiteral("%1A  %2    %1B  %3")
                     .arg(group)
                     .arg(countA)
                     .arg(countB);
    }

    if (rdsUnusableBCount_ > 0) {
        lines << QString();
        lines << QStringLiteral("Block B unbrauchbar: %1")
                     .arg(rdsUnusableBCount_);
    }

    return lines.join(QLatin1Char('\n'));
}

QString XdrClient::rdsErrorStats() const
{
    QStringList lines;

    for (int block = 0; block < 3; ++block) {
        QString name;

        if (block == 0)
            name = QStringLiteral("B");
        else if (block == 1)
            name = QStringLiteral("C");
        else
            name = QStringLiteral("D");

        const auto &v =
            rdsBlockErrorCounts_[static_cast<std::size_t>(block)];

        lines << QStringLiteral(
                     "Block %1:  0=%2   1=%3   2=%4   3=%5")
                     .arg(name)
                     .arg(v[0])
                     .arg(v[1])
                     .arg(v[2])
                     .arg(v[3]);
    }

    return lines.join(QLatin1Char('\n'));
}

QString XdrClient::rdsRawGroups() const
{
    return rdsRawLines_.join(QLatin1Char('\n'));
}

void XdrClient::clearRdsMonitor()
{
    rdsGroupTypeCounts_.fill(0);

    for (auto &block : rdsBlockErrorCounts_)
        block.fill(0);

    rdsUnusableBCount_ = 0;
    rdsRawLines_.clear();

    rdsTp_ = -1;
    rdsTa_ = -1;
    rdsMs_ = -1;

    rdsDiMask_ = 0;
    rdsDiSeenMask_ = 0;

    rdsAfLists_.clear();

    rdsAfCurrentBaseKhz_ = -1;
    rdsAfCurrentExpectedCount_ = -1;
    rdsAfCurrentMethod_ = 0;

    rdsAfOtherCodes_.clear();

    rdsPtynBuffer_.fill(
        QLatin1Char(' '),
        8);

    rdsPtynSegments_.fill(false);
    rdsPtynAbKnown_ = false;
    rdsPtynAb_ = false;

    rdsLanguageCode_ = -1;

    rdsOdaGroupCode_.clear();
    rdsOdaLastData_.clear();
    rdsOdaCount_.clear();

    rdsEon_.clear();

    emit rdsChanged();
}

QString XdrClient::saveRdsMonitor() const
{
    QString base =
        QStandardPaths::writableLocation(
            QStandardPaths::DocumentsLocation);

    if (base.isEmpty())
        base = QDir::homePath();

    const QString directory =
        QDir(base).filePath(
            QStringLiteral("XdrTablet/RDS-Logs"));

    if (!QDir().mkpath(directory))
        return QStringLiteral("FEHLER: Ordner konnte nicht erstellt werden");

    QString pi = piCode_;

    if (pi.isEmpty() || pi == QStringLiteral("----"))
        pi = QStringLiteral("UNBEKANNT");

    const QString stamp =
        QDateTime::currentDateTime()
            .toString(QStringLiteral("yyyyMMdd_HHmmss"));

    const QString fileName =
        QStringLiteral("RDS_%1_%2.txt")
            .arg(pi, stamp);

    const QString path =
        QDir(directory).filePath(fileName);

    QFile file(path);

    if (!file.open(
            QIODevice::WriteOnly |
            QIODevice::Text)) {
        return QStringLiteral("FEHLER: Datei konnte nicht geöffnet werden");
    }

    QTextStream out(&file);

    out << "XdrTablet RDS-Monitor\n";
    out << "=====================\n\n";

    out << "Zeit: "
        << QDateTime::currentDateTime()
               .toString(Qt::ISODate)
        << "\n";

    out << "PI:  " << piCode_ << "\n";
    out << "PS:  "
        << (psText_.isEmpty()
                ? QStringLiteral("--")
                : psText_)
        << "\n";

    out << "PTY: ";

    if (ptyCode_ >= 0)
        out << ptyCode_ << "  " << ptyText_;
    else
        out << "--";

    out << "\n";

    out << "ECC: "
        << (eccCode_ == QStringLiteral("--")
                ? QStringLiteral("--")
                : eccCode_)
        << "\n";

    out << "PIN: "
        << (pinText_.isEmpty()
                ? QStringLiteral("--")
                : pinText_)
        << "\n";

    out << "CT:  "
        << (ctText_.isEmpty()
                ? QStringLiteral("--")
                : ctText_)
        << "\n";

    out << "\nRadiotext\n";
    out << "---------\n";
    out << (radioText_.isEmpty()
                ? QStringLiteral("--")
                : radioText_)
        << "\n";

    out << "\nRT+\n";
    out << "---\n";
    out << "Titel: "
        << (rtPlusTitle_.isEmpty()
                ? QStringLiteral("--")
                : rtPlusTitle_)
        << "\n";

    out << "Interpret: "
        << (rtPlusArtist_.isEmpty()
                ? QStringLiteral("--")
                : rtPlusArtist_)
        << "\n";

    out << "\nFlags / Decoder Information\n";
    out << "---------------------------\n";
    out << rdsFlagsText() << "\n";

    out << "\nAlternative Frequencies (AF)\n";
    out << "----------------------------\n";
    out << rdsAfText() << "\n";

    out << "\nPTYN\n";
    out << "----\n";
    out << rdsPtynText() << "\n";

    out << "\nSprache\n";
    out << "-------\n";
    out << rdsLanguageText() << "\n";

    out << "\nODA\n";
    out << "---\n";
    out << rdsOdaText() << "\n";

    out << "\nEON\n";
    out << "---\n";
    out << rdsEonText() << "\n";

    out << "\nRDS-Gruppen\n";
    out << "-----------\n";
    out << rdsGroupStats() << "\n";

    out << "\nFehlerstatistik\n";
    out << "---------------\n";
    out << rdsErrorStats() << "\n";

    out << "\nLetzte Rohgruppen\n";
    out << "-----------------\n";
    out << rdsRawGroups() << "\n";

    file.close();

    return path;
}


QString XdrClient::lastLine() const { return lastLine_; }
int XdrClient::minimumFmFrequencyKhz() const { return MinimumFmFrequencyKhz; }
int XdrClient::maximumFmFrequencyKhz() const { return MaximumFmFrequencyKhz; }
int XdrClient::smallStepKhz() const { return smallStepKhz_; }
int XdrClient::largeStepKhz() const { return largeStepKhz_; }
int XdrClient::seekThreshold() const { return seekThreshold_; }
bool XdrClient::seeking() const { return seeking_; }
int XdrClient::seekDirection() const { return seekDirection_; }

QString XdrClient::receptionModeText() const
{
    if (forcedMono_)
        return stereo_ ? QStringLiteral("MONO erzwungen · Stereosignal")
                       : QStringLiteral("MONO erzwungen");
    return stereo_ ? QStringLiteral("STEREO") : QStringLiteral("MONO");
}

void XdrClient::connectToServer(const QString &host, int port,
                                const QString &password)
{
    if (serial_.isOpen())
        serial_.close();

    if (usbMode_) {
        usbMode_ = false;
        emit connectionTypeChanged();
        emit connectedChanged();
    }

    residualTimer_.stop();
    authenticationTimer_.stop();
    rdsTimeoutTimer_.stop();
    cancelSeekSilently();
    socket_.abort();
    buffer_.clear();
    pendingSalt_.clear();
    authenticationSent_ = false;
    startupSent_ = false;
    password_ = password;
    setReady(false);
    clearSignalData();
    clearRdsData();

    const QString cleanHost = host.trimmed();
    if (cleanHost.isEmpty() || port < 1 || port > 65535) {
        setStatusText(QStringLiteral("Ungültige IP-Adresse oder Portnummer"));
        return;
    }

    setStatusText(QStringLiteral("Verbinde mit %1:%2 …").arg(cleanHost).arg(port));
    qInfo().noquote() << "TCP CONNECT" << cleanHost << port;
    socket_.connectToHost(cleanHost, static_cast<quint16>(port));
}

void XdrClient::connectToUsb(const QString &portName, int baudRate)
{
    residualTimer_.stop();
    authenticationTimer_.stop();
    rdsTimeoutTimer_.stop();
    cancelSeekSilently();

    // Eine eventuell bestehende TCP-Verbindung beenden.
    socket_.abort();

    const bool modeChanged = !usbMode_;
    usbMode_ = true;

    if (modeChanged)
        emit connectionTypeChanged();

    if (serial_.isOpen())
        serial_.close();

    buffer_.clear();
    pendingSalt_.clear();

    // USB benötigt keine TCP-Authentifizierung.
    authenticationSent_ = true;
    startupSent_ = true;

    setReady(false);
    clearSignalData();
    clearRdsData();

    const QString cleanPort = portName.trimmed();

    if (cleanPort.isEmpty()) {
        setStatusText(QStringLiteral("Kein USB-Anschluss ausgewählt"));
        emit connectedChanged();
        return;
    }

    serial_.setPortName(cleanPort);
    serial_.setBaudRate(baudRate);
    serial_.setDataBits(QSerialPort::Data8);
    serial_.setParity(QSerialPort::NoParity);
    serial_.setStopBits(QSerialPort::OneStop);
    serial_.setFlowControl(QSerialPort::NoFlowControl);

    setStatusText(
        QStringLiteral("Öffne USB %1 mit %2 Baud …")
            .arg(cleanPort)
            .arg(baudRate));

    if (!serial_.open(QIODevice::ReadWrite)) {
        setStatusText(
            QStringLiteral("USB-Fehler: %1")
                .arg(serial_.errorString()));

        emit connectedChanged();
        return;
    }

    qInfo().noquote()
        << "USB CONNECT"
        << cleanPort
        << baudRate;

    emit connectedChanged();

    setStatusText(QStringLiteral("USB verbunden – starte Tuner"));

    // Der ESP32/USB-Wandler braucht nach dem Öffnen kurz Zeit.
    // Ein sofort gesendetes "x" kann insbesondere nach einem
    // Kaltstart verloren gehen.
    QTimer::singleShot(400, this, [this]() {
        if (usbMode_ && serial_.isOpen() && !ready_)
            sendLine(QStringLiteral("x"), false);
    });

    // Falls der erste Handshake trotzdem verloren ging,
    // einmal automatisch wiederholen.
    QTimer::singleShot(1200, this, [this]() {
        if (usbMode_ && serial_.isOpen() && !ready_)
            sendLine(QStringLiteral("x"), false);
    });
}

QStringList XdrClient::availableSerialPorts() const
{
    QStringList ports;

    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
        ports.append(info.systemLocation());

    return ports;
}

void XdrClient::disconnectFromServer()
{
    residualTimer_.stop();
    authenticationTimer_.stop();
    rdsTimeoutTimer_.stop();
    cancelSeekSilently();

    if (usbMode_) {
        if (serial_.isOpen())
            serial_.close();

        buffer_.clear();
        pendingSalt_.clear();
        authenticationSent_ = false;
        startupSent_ = false;

        setReady(false);
        clearSignalData();
        clearRdsData();

        emit connectedChanged();
        setStatusText(QStringLiteral("Nicht verbunden"));

        qInfo() << "USB DISCONNECTED";
        return;
    }

    socket_.disconnectFromHost();
}

void XdrClient::setFrequencyKhz(int khz)
{
    if (seeking_)
        finishSeek(QStringLiteral("Suchlauf durch manuelle Abstimmung beendet"));

    const int bounded = qBound(MinimumFmFrequencyKhz, khz, MaximumFmFrequencyKhz);
    if (bounded == frequencyKhz_) {
        if (khz < MinimumFmFrequencyKhz)
            setStatusText(QStringLiteral("Untere Bandgrenze: 87,500 MHz"));
        else if (khz > MaximumFmFrequencyKhz)
            setStatusText(QStringLiteral("Obere Bandgrenze: 108,000 MHz"));
        return;
    }
    sendFrequencyCommand(bounded);
}

void XdrClient::stepFrequency(int deltaKhz)
{
    if (deltaKhz != 0)
        setFrequencyKhz(frequencyKhz_ + deltaKhz);
}

void XdrClient::stepSmall(int direction)
{
    stepFrequency((direction < 0 ? -1 : 1) * smallStepKhz_);
}

void XdrClient::stepLarge(int direction)
{
    stepFrequency((direction < 0 ? -1 : 1) * largeStepKhz_);
}

void XdrClient::setSmallStepKhz(int khz)
{
    const int value = qBound(10, khz, 1000);
    if (smallStepKhz_ == value)
        return;
    smallStepKhz_ = value;
    QSettings().setValue(QStringLiteral("radio/smallStepKhz"), value);
    emit tuningSettingsChanged();
}

void XdrClient::setLargeStepKhz(int khz)
{
    const int value = qBound(100, khz, 5000);
    if (largeStepKhz_ == value)
        return;
    largeStepKhz_ = value;
    QSettings().setValue(QStringLiteral("radio/largeStepKhz"), value);
    emit tuningSettingsChanged();
}

void XdrClient::setSeekThreshold(int value)
{
    value = qBound(1, value, 30);

    if (seekThreshold_ != value) {
        seekThreshold_ = value;
        emit tuningSettingsChanged();
    }

    // PE5PVB: I1..I30 = FM scan sensitivity.
    // Die Firmware speichert den Wert selbst im EEPROM.
    sendLine(QStringLiteral("I%1").arg(value));
}

void XdrClient::startSeek(int direction)
{
    if (!ready_) {
        setStatusText(QStringLiteral("Tuner ist noch nicht bereit"));
        return;
    }

    if (direction != -1 && direction != 1)
        return;

    if (seeking_)
        return;

    seekEvaluationTimer_.stop();
    seeking_ = true;
    seekDirection_ = direction;
    seekSignalSum_ = 0.0;
    seekSignalSamples_ = 0;
    emit seekingChanged();

    setStatusText(direction > 0
                      ? QStringLiteral("Suchlauf aufwärts …")
                      : QStringLiteral("Suchlauf abwärts …"));

    // PE5PVB: C1 = Suchlauf abwärts, C2 = Suchlauf aufwärts.
    sendLine(direction > 0
                 ? QStringLiteral("C2")
                 : QStringLiteral("C1"));
}

void XdrClient::stopSeek()
{
    if (!seeking_)
        return;

    // Ein T-Befehl beendet den Suchlauf in der PE5PVB-Firmware.
    sendFrequencyCommand(frequencyKhz_);
    finishSeek(QStringLiteral("Suchlauf gestoppt"));
}

void XdrClient::setForcedMono(bool enabled)
{
    if (forcedMono_ != enabled) {
        forcedMono_ = enabled;
        QSettings().setValue(QStringLiteral("receiver/forcedMono"), enabled);
        emit receptionModeChanged();
    }
    // XDR-GTK: B1 = Mono erzwingen, B0 = Stereo-Automatik.
    sendLine(enabled ? QStringLiteral("B1") : QStringLiteral("B0"));
}

void XdrClient::setStereoAuto(bool enabled)
{
    setForcedMono(!enabled);
}

void XdrClient::setBandwidth(int hz)
{
    const int value = boundedBandwidth(hz);
    if (bandwidthSettingHz_ != value) {
        bandwidthSettingHz_ = value;
        QSettings().setValue(QStringLiteral("receiver/bandwidthHz"), value);
        emit receiverSettingsChanged();
    }
    sendLine(QStringLiteral("W%1").arg(value));
}

void XdrClient::setDeemphasis(int mode)
{
    const int value = qBound(0, mode, 2);
    if (deemphasis_ != value) {
        deemphasis_ = value;
        QSettings().setValue(QStringLiteral("receiver/deemphasis"), value);
        emit receiverSettingsChanged();
    }
    sendLine(QStringLiteral("D%1").arg(value));
}

void XdrClient::setAgc(int mode)
{
    const int value = qBound(0, mode, 3);
    if (agc_ != value) {
        agc_ = value;
        QSettings().setValue(QStringLiteral("receiver/agc"), value);
        emit receiverSettingsChanged();
    }
    sendLine(QStringLiteral("A%1").arg(value));
}

void XdrClient::setChannelEqualizer(bool enabled)
{
    if (channelEqualizerEnabled_ == enabled)
        return;

    channelEqualizerEnabled_ = enabled;
    QSettings().setValue(
        QStringLiteral("receiver/channelEqualizer"), enabled);
    emit receiverSettingsChanged();
    sendDspCommand();
}

void XdrClient::setMultipathSuppression(bool enabled)
{
    if (multipathSuppressionEnabled_ == enabled)
        return;

    multipathSuppressionEnabled_ = enabled;
    QSettings().setValue(
        QStringLiteral("receiver/multipathSuppression"), enabled);
    emit receiverSettingsChanged();
    sendDspCommand();
}

void XdrClient::onConnected()
{
    emit connectedChanged();
    setStatusText(QStringLiteral("TCP verbunden – warte auf Anmeldekennung"));
    authenticationTimer_.start();
    qInfo() << "TCP CONNECTED";
}

void XdrClient::onDisconnected()
{
    residualTimer_.stop();
    authenticationTimer_.stop();
    rdsTimeoutTimer_.stop();
    buffer_.clear();
    pendingSalt_.clear();
    authenticationSent_ = false;
    startupSent_ = false;
    cancelSeekSilently();
    const bool wasReady = ready_;
    setReady(false);
    clearSignalData();
    clearRdsData();
    emit connectedChanged();

    if (socket_.error() == QAbstractSocket::RemoteHostClosedError && !wasReady)
        setStatusText(QStringLiteral("Anmeldung abgelehnt – Passwort prüfen"));
    else
        setStatusText(QStringLiteral("Nicht verbunden"));

    qInfo() << "TCP DISCONNECTED";
}

void XdrClient::onReadyRead()
{
    const QByteArray data = socket_.readAll();
    if (data.isEmpty())
        return;

    qInfo().noquote() << "RX RAW:"
                      << QString::fromUtf8(data)
                             .replace('\r', "\\r")
                             .replace('\n', "\\n");
    buffer_ += data;

    if (!authenticationSent_ && buffer_.size() >= XdrSaltLength) {
        pendingSalt_ = buffer_.left(XdrSaltLength);
        buffer_.remove(0, XdrSaltLength);
        if (buffer_.startsWith("\r\n"))
            buffer_.remove(0, 2);
        else if (buffer_.startsWith('\n'))
            buffer_.remove(0, 1);
        sendAuthentication(pendingSalt_);
    }

    processCompleteLines();

    if (!buffer_.isEmpty())
        residualTimer_.start();
}

void XdrClient::onSerialReadyRead()
{
    if (!usbMode_)
        return;

    const QByteArray data = serial_.readAll();

    if (data.isEmpty())
        return;

    qInfo().noquote()
        << "USB RX RAW:"
        << QString::fromUtf8(data)
               .replace('\r', "\\r")
               .replace('\n', "\\n");

    buffer_ += data;

    processCompleteLines();

    if (!buffer_.isEmpty())
        residualTimer_.start();
}

void XdrClient::onSerialError(QSerialPort::SerialPortError error)
{
    if (!usbMode_ || error == QSerialPort::NoError)
        return;

    qWarning().noquote()
        << "USB ERROR:"
        << serial_.errorString();

    setStatusText(
        QStringLiteral("USB-Fehler: %1")
            .arg(serial_.errorString()));

    if (error == QSerialPort::ResourceError ||
        error == QSerialPort::DeviceNotFoundError ||
        error == QSerialPort::PermissionError) {

        setReady(false);

        if (serial_.isOpen())
            serial_.close();

        emit connectedChanged();
    }
}

void XdrClient::processCompleteLines()
{
    while (true) {
        const qsizetype newline = buffer_.indexOf('\n');
        if (newline < 0)
            return;

        QByteArray raw = buffer_.left(newline);
        buffer_.remove(0, newline + 1);
        raw.replace("\r", "");

        // Gelegentlich kommt beim Öffnen der ESP32-USB-Verbindung
        // ein einzelnes ungültiges Startbyte vor dem ersten "OK".
        // Das eigentliche XDR-Protokoll ist ASCII.
        while (!raw.isEmpty()) {
            const unsigned char c =
                static_cast<unsigned char>(raw.at(0));

            if (c >= 0x20 && c <= 0x7e)
                break;

            raw.remove(0, 1);
        }

        const QString line = QString::fromLatin1(raw).trimmed();
        if (!line.isEmpty())
            processLine(line);
    }
}

void XdrClient::processResidualBuffer()
{
    if (buffer_.isEmpty())
        return;

    if (!authenticationSent_) {
        if (buffer_.size() >= XdrSaltLength) {
            pendingSalt_ = buffer_.left(XdrSaltLength);
            buffer_.remove(0, XdrSaltLength);
            sendAuthentication(pendingSalt_);
        }
        return;
    }

    QByteArray raw = buffer_;
    buffer_.clear();
    raw.replace("\r", "");
    raw.replace("\n", "");

    const QString line = QString::fromUtf8(raw).trimmed();
    if (!line.isEmpty())
        processLine(line);
}

void XdrClient::onAuthenticationTimeout()
{
    if (!authenticationSent_) {
        setStatusText(QStringLiteral("Keine Anmeldekennung vom Server empfangen"));
        socket_.disconnectFromHost();
    }
}

void XdrClient::onError(QAbstractSocket::SocketError error)
{
    cancelSeekSilently();
    setReady(false);

    if (error == QAbstractSocket::RemoteHostClosedError && authenticationSent_)
        setStatusText(QStringLiteral("Anmeldung abgelehnt – Passwort prüfen"));
    else
        setStatusText(QStringLiteral("TCP-Fehler: %1").arg(socket_.errorString()));

    emit connectedChanged();
    qWarning().noquote() << "TCP ERROR:" << socket_.errorString();
}

void XdrClient::sendAuthentication(const QByteArray &salt)
{
    if (authenticationSent_ || salt.size() != XdrSaltLength)
        return;

    authenticationTimer_.stop();

    QByteArray input = salt;
    input += password_.toUtf8();
    const QByteArray digest =
        QCryptographicHash::hash(input, QCryptographicHash::Sha1).toHex();

    authenticationSent_ = true;
    qInfo().noquote() << "AUTH SALT:" << QString::fromLatin1(salt);
    qInfo().noquote() << "AUTH SHA1:" << QString::fromLatin1(digest);
    socket_.write(digest + '\n');
    socket_.flush();

    setStatusText(QStringLiteral("Anmeldung gesendet – warte auf Server"));
}

void XdrClient::sendLine(const QString &line, bool requireReady)
{
    if (!connected()) {
        setStatusText(
            usbMode_
                ? QStringLiteral("Keine USB-Verbindung")
                : QStringLiteral("Keine TCP-Verbindung"));
        return;
    }

    if (requireReady && !ready_) {
        setStatusText(QStringLiteral("Tuner ist noch nicht bereit"));
        return;
    }

    const QByteArray packet = line.toUtf8() + '\n';

    if (usbMode_) {
        qInfo().noquote() << "USB TX:" << line;
        serial_.write(packet);
        serial_.flush();
    } else {
        qInfo().noquote() << "TCP TX:" << line;
        socket_.write(packet);
        socket_.flush();
    }
}

void XdrClient::processLine(const QString &line)
{
    lastLine_ = line;
    emit lastLineChanged();
    qInfo().noquote() << "RX:" << line;

    if (line == QStringLiteral("a0")) {
        setReady(false);
        setStatusText(QStringLiteral("Anmeldung abgelehnt – Passwort prüfen"));
        return;
    }

    if (line == QStringLiteral("a1")) {
        setReady(false);
        setStatusText(QStringLiteral("Nur Gastzugang – Steuerung nicht erlaubt"));
        return;
    }

    if (line.startsWith('o')) {
        if (!startupSent_) {
            startupSent_ = true;
            setStatusText(QStringLiteral("Anmeldung erfolgreich – starte Tuner"));
            sendLine(QStringLiteral("x"), false);
        }
        return;
    }

    if (line == QStringLiteral("OK")) {
        setReady(true);

        if (usbMode_) {
            setStatusText(QStringLiteral("Tuner bereit"));
            applySavedReceiverSettings();
            return;
        }

        setStatusText(
            QStringLiteral("Tuner bereit – stelle %1 MHz ein")
                .arg(frequencyKhz_ / 1000.0, 0, 'f', 3));
        sendFrequencyCommand(frequencyKhz_);
        applySavedReceiverSettings();
        return;
    }

        if (line.startsWith('T')) {
        const QString frequencyField =
            line.mid(1).section(',', 0, 0).trimmed();
        bool ok = false;
        const int value = frequencyField.toInt(&ok);

        if (ok &&
            value >= MinimumFmFrequencyKhz &&
            value <= MaximumFmFrequencyKhz) {
            const bool changed = value != frequencyKhz_;
            frequencyKhz_ = value;

            if (changed) {
                clearRdsData();
                clearRdsMonitor();
                emit frequencyChanged();
            }

            saveFrequency(value);

            if (seeking_) {
                // PE5PVB sendet bei jedem Suchschritt eine neue T-Frequenz.
                // Solange T-Meldungen kommen, läuft der Suchlauf weiter.
                seekEvaluationTimer_.start();
            } else {
                setStatusText(QStringLiteral("Tuner bereit"));
            }
        }
        return;
    }

    if (line.startsWith('I')) {
        bool ok = false;
        const int value = line.mid(1).toInt(&ok);

        if (ok && value >= 1 && value <= 30) {
            if (seekThreshold_ != value) {
                seekThreshold_ = value;
                emit tuningSettingsChanged();
            }

            qInfo().noquote()
                << "PE5PVB FM-Suchempfindlichkeit:"
                << value;
        }

        return;
    }

    if (line.startsWith('C')) {
        bool ok = false;
        const int value = line.mid(1).toInt(&ok);

        if (!ok)
            return;

        if (value == 1 || value == 2) {
            const int direction = value == 2 ? 1 : -1;
            const bool changed =
                !seeking_ || seekDirection_ != direction;

            seekEvaluationTimer_.stop();
            seeking_ = true;
            seekDirection_ = direction;
            seekSignalSum_ = 0.0;
            seekSignalSamples_ = 0;

            if (changed)
                emit seekingChanged();

            setStatusText(direction > 0
                              ? QStringLiteral("Suchlauf aufwärts …")
                              : QStringLiteral("Suchlauf abwärts …"));
        } else if (value == 0) {
            // PE5PVB sendet C0 bereits nach der Bearbeitung von C1/C2.
            // Der interne Suchlauf der Firmware kann danach weiterlaufen.
            // Deshalb C0 nicht als Suchende behandeln.
        }

        return;
    }

    if (line.startsWith('W')) {
        bool ok = false;
        const int value = line.mid(1).toInt(&ok);
        if (ok) {
            const int normalized = boundedBandwidth(value);
            if (normalized != bandwidthSettingHz_) {
                bandwidthSettingHz_ = normalized;
                QSettings().setValue(QStringLiteral("receiver/bandwidthHz"), normalized);
                emit receiverSettingsChanged();
            }
        }
        return;
    }

    if (line.startsWith('B')) {
        bool ok = false;
        const int value = line.mid(1).toInt(&ok);
        if (ok)
            updateReceptionMode(stereo_, value == 1);
        return;
    }

    if (line.startsWith('D')) {
        bool ok = false;
        const int value = line.mid(1).toInt(&ok);
        if (ok && value >= 0 && value <= 2 && value != deemphasis_) {
            deemphasis_ = value;
            QSettings().setValue(QStringLiteral("receiver/deemphasis"), value);
            emit receiverSettingsChanged();
        }
        return;
    }

    if (line.startsWith('A')) {
        bool ok = false;
        const int value = line.mid(1).toInt(&ok);
        if (ok && value >= 0 && value <= 3 && value != agc_) {
            agc_ = value;
            QSettings().setValue(QStringLiteral("receiver/agc"), value);
            emit receiverSettingsChanged();
        }
        return;
    }

    if (line.startsWith('G')) {
        const QString value = line.mid(1).rightJustified(
            2, QLatin1Char('0'));

        if (value.size() == 2 &&
            (value.at(0) == QLatin1Char('0') ||
             value.at(0) == QLatin1Char('1')) &&
            (value.at(1) == QLatin1Char('0') ||
             value.at(1) == QLatin1Char('1'))) {

            // PE5PVB: 0 bedeutet Funktion EIN, 1 bedeutet Funktion AUS.
            const bool newEqualizer =
                value.at(0) == QLatin1Char('0');
            const bool newMultipathSuppression =
                value.at(1) == QLatin1Char('0');

            if (newEqualizer != channelEqualizerEnabled_ ||
                newMultipathSuppression !=
                    multipathSuppressionEnabled_) {

                channelEqualizerEnabled_ = newEqualizer;
                multipathSuppressionEnabled_ =
                    newMultipathSuppression;

                QSettings settings;
                settings.setValue(
                    QStringLiteral("receiver/channelEqualizer"),
                    channelEqualizerEnabled_);
                settings.setValue(
                    QStringLiteral("receiver/multipathSuppression"),
                    multipathSuppressionEnabled_);
                emit receiverSettingsChanged();
            }
        }
        return;
    }

    if (line.startsWith('P')) {
        processPiLine(line);
        return;
    }

    if (line.startsWith('R')) {
        processRdsLine(line);
        return;
    }

    if (line.startsWith(QStringLiteral("Ss")) ||
        line.startsWith(QStringLiteral("Sm")) ||
        line.startsWith(QStringLiteral("SS")) ||
        line.startsWith(QStringLiteral("SM"))) {
        const QChar mode = line.at(1);
        const bool receivedStereo = mode == QLatin1Char('s') || mode == QLatin1Char('S');
        const bool forcedMono = mode == QLatin1Char('S') || mode == QLatin1Char('M');
        updateReceptionMode(receivedStereo, forcedMono);

        const QStringList fields = line.mid(2).split(',');
        if (!fields.isEmpty()) {
            bool ok = false;
            const double level = fields.at(0).toDouble(&ok);
            if (ok) {
                const bool changed = !signalAvailable_ ||
                    !qFuzzyCompare(level + 1.0, signalLevel_ + 1.0);
                signalAvailable_ = true;
                signalLevel_ = level;
                if (changed)
                    emit signalChanged();
            }
        }

        bool qualityValuesChanged = false;
        if (fields.size() >= 2) {
            bool ok = false;
            const int value = fields.at(1).toInt(&ok);
            if (ok) {
                const int normalized = qBound(0, value, 100);
                if (normalized != cci_) {
                    cci_ = normalized;
                    qualityValuesChanged = true;
                }
            }
        }
        if (fields.size() >= 3) {
            bool ok = false;
            const int value = fields.at(2).toInt(&ok);
            if (ok) {
                const int normalized = qBound(0, value, 100);
                if (normalized != aci_) {
                    aci_ = normalized;
                    qualityValuesChanged = true;
                }
            }
        }
        if (qualityValuesChanged)
            emit qualityChanged();

        if (fields.size() >= 4) {
            bool ok = false;
            int bw = fields.at(3).toInt(&ok);

            if (ok && bw >= 0) {
                if (bw < 1000)
                    bw *= 1000;

                if (bw != bandwidthHz_) {
                    bandwidthHz_ = bw;
                    emit bandwidthChanged();
                }
            }
        }

        if (seeking_ && signalAvailable_) {
            seekSignalSum_ += signalLevel_;
            seekSignalSamples_++;
        }
        return;
    }
}

void XdrClient::applySavedReceiverSettings()
{
    sendLine(forcedMono_ ? QStringLiteral("B1") : QStringLiteral("B0"));
    sendLine(QStringLiteral("W%1").arg(bandwidthSettingHz_));
    sendLine(QStringLiteral("D%1").arg(deemphasis_));
    sendLine(QStringLiteral("A%1").arg(agc_));
    sendDspCommand();

    // I0 ändert den PE5PVB-Wert nicht, liefert aber I<aktueller Wert>.
    sendLine(QStringLiteral("I0"));
}

void XdrClient::sendDspCommand()
{
    // PE5PVB:
    // erste Stelle 0 = Channel EQ EIN, 1 = AUS
    // zweite Stelle 0 = iMS EIN,       1 = AUS
    const QChar equalizer =
        channelEqualizerEnabled_
            ? QLatin1Char('0')
            : QLatin1Char('1');

    const QChar multipath =
        multipathSuppressionEnabled_
            ? QLatin1Char('0')
            : QLatin1Char('1');

    sendLine(QStringLiteral("G%1%2")
                 .arg(equalizer)
                 .arg(multipath));
}


void XdrClient::setRdsErrorCorrectionEnabled(bool enabled)
{
    if (rdsErrorCorrectionEnabled_ == enabled)
        return;

    rdsErrorCorrectionEnabled_ = enabled;

    /*
     * Bereits gesammelte RT-Segmente stammen möglicherweise
     * noch von der vorherigen Filtereinstellung.
     * Deshalb Textdaten neu aufbauen.
     */
    rtBuffer_.fill(QLatin1Char(' '), 64);
    rtSegments_.fill(false);

    // Auch PS und PTY mit der neuen Fehlerfilter-Einstellung
    // vollständig neu empfangen.
    psBuffer_.fill(QLatin1Char(' '), 8);
    psSegments_.fill(false);
    psText_.clear();

    ptyCode_ = -1;
    ptyText_.clear();

    radioText_.clear();
    rtPlusTitle_.clear();
    rtPlusArtist_.clear();
    ctText_.clear();

    rtPlusGroupCode_ = -1;
    rtPlusItemToggle_ = -1;
    rtPlusItemRunning_ = false;
    rtPlusItemRunningKnown_ = false;

    emit rdsErrorCorrectionChanged();
    emit rdsChanged();

    qInfo().noquote()
        << "RDS-Fehlerkorrektur:"
        << (enabled
            ? "korrigierte Blöcke erlaubt"
            : "nur fehlerfreie Blöcke");
}


void XdrClient::clearEpg()
{
    if (epgReply_) {
        epgReply_->abort();
        epgReply_ = nullptr;
    }

    const bool changed =
        epgAvailable_ ||
        !epgNow_.isEmpty() ||
        !epgNext_.isEmpty();

    epgXml_.clear();
    epgDateKey_.clear();
    epgNow_.clear();
    epgNext_.clear();
    epgAvailable_ = false;

    if (changed)
        emit epgChanged();
}

int XdrClient::isoDurationSeconds(const QString &value)
{
    static const QRegularExpression rx(
        QStringLiteral(
            "^P(?:(\\d+)D)?T?"
            "(?:(\\d+)H)?"
            "(?:(\\d+)M)?"
            "(?:(\\d+)S)?$"));

    const QRegularExpressionMatch m = rx.match(value);

    if (!m.hasMatch())
        return 0;

    const int days =
        m.captured(1).isEmpty() ? 0 : m.captured(1).toInt();
    const int hours =
        m.captured(2).isEmpty() ? 0 : m.captured(2).toInt();
    const int minutes =
        m.captured(3).isEmpty() ? 0 : m.captured(3).toInt();
    const int seconds =
        m.captured(4).isEmpty() ? 0 : m.captured(4).toInt();

    return days * 86400 +
           hours * 3600 +
           minutes * 60 +
           seconds;
}

void XdrClient::refreshEpg()
{
    /*
     * Erster Test nur für Deutschlandfunk.
     *
     * UKW:
     *   PI D210
     *
     * RadioDNS-Fallback über den bekannten DAB-Bearer:
     *   dab:de0.10bc.d210.0
     */
    if (rdsPi_ != 0xD210) {
        clearEpg();
        return;
    }

    const QString today =
        QDate::currentDate().toString(QStringLiteral("yyyyMMdd"));

    if (epgXml_.isEmpty() ||
        epgDateKey_ != today) {

        if (!epgReply_)
            fetchDlfEpg();

        return;
    }

    updateEpgFromXml();
}

void XdrClient::fetchDlfEpg()
{
    if (!epgNetwork_ || epgReply_)
        return;

    const QString dateKey =
        QDate::currentDate().toString(QStringLiteral("yyyyMMdd"));

    const QString url =
        QStringLiteral(
            "http://rdns.deutschlandradio.de/"
            "radiodns/spi/3.1/"
            "dab/de0/10bc/d210/0/%1_PI.xml")
            .arg(dateKey);

    qInfo().noquote()
        << "EPG DLF Download:"
        << url;

    QNetworkRequest request{QUrl(url)};

    QNetworkReply *reply =
        epgNetwork_->get(request);

    epgReply_ = reply;

    connect(reply, &QNetworkReply::finished,
            this,
            [this, reply, dateKey]() {

        if (reply != epgReply_) {
            reply->deleteLater();
            return;
        }

        epgReply_ = nullptr;

        if (rdsPi_ != 0xD210) {
            reply->deleteLater();
            return;
        }

        if (reply->error() != QNetworkReply::NoError) {
            qWarning().noquote()
                << "EPG DLF Fehler:"
                << reply->errorString();

            reply->deleteLater();
            return;
        }

        const QByteArray data =
            reply->readAll();

        reply->deleteLater();

        if (data.isEmpty()) {
            qWarning()
                << "EPG DLF: leere PI.xml";
            return;
        }

        epgXml_ = data;
        epgDateKey_ = dateKey;

        qInfo()
            << "EPG DLF:"
            << epgXml_.size()
            << "Bytes geladen";

        updateEpgFromXml();
    });
}

void XdrClient::updateEpgFromXml()
{
    struct ProgrammeEntry {
        QDateTime start;
        QDateTime end;
        QString name;
    };

    QVector<ProgrammeEntry> entries;

    QXmlStreamReader xml(epgXml_);

    while (!xml.atEnd()) {
        xml.readNext();

        if (!xml.isStartElement() ||
            xml.name().toString() !=
                QStringLiteral("programme")) {
            continue;
        }

        QString longName;
        QString mediumName;
        QString shortName;

        QDateTime startTime;
        int durationSeconds = 0;

        while (!xml.atEnd()) {
            xml.readNext();

            if (xml.isEndElement() &&
                xml.name().toString() ==
                    QStringLiteral("programme")) {
                break;
            }

            if (!xml.isStartElement())
                continue;

            const QString element =
                xml.name().toString();

            if (element == QStringLiteral("longName") &&
                longName.isEmpty()) {

                longName =
                    xml.readElementText(
                        QXmlStreamReader::IncludeChildElements)
                        .trimmed();
            }
            else if (element ==
                         QStringLiteral("mediumName") &&
                     mediumName.isEmpty()) {

                mediumName =
                    xml.readElementText(
                        QXmlStreamReader::IncludeChildElements)
                        .trimmed();
            }
            else if (element ==
                         QStringLiteral("shortName") &&
                     shortName.isEmpty()) {

                shortName =
                    xml.readElementText(
                        QXmlStreamReader::IncludeChildElements)
                        .trimmed();
            }
            else if (element ==
                         QStringLiteral("time") &&
                     !startTime.isValid()) {

                const auto attrs =
                    xml.attributes();

                const QString timeText =
                    attrs.value(
                        QStringLiteral("time"))
                        .toString();

                const QString durationText =
                    attrs.value(
                        QStringLiteral("duration"))
                        .toString();

                startTime =
                    QDateTime::fromString(
                        timeText,
                        Qt::ISODate);

                durationSeconds =
                    isoDurationSeconds(
                        durationText);
            }
        }

        QString name = longName;

        if (name.isEmpty())
            name = mediumName;

        if (name.isEmpty())
            name = shortName;

        if (!startTime.isValid() ||
            durationSeconds <= 0 ||
            name.isEmpty()) {
            continue;
        }

        ProgrammeEntry e;
        e.start = startTime;
        e.end =
            startTime.addSecs(durationSeconds);
        e.name = name;

        entries.append(e);
    }

    if (xml.hasError()) {
        qWarning().noquote()
            << "EPG DLF XML-Fehler:"
            << xml.errorString();
        return;
    }

    std::sort(
        entries.begin(),
        entries.end(),
        [](const ProgrammeEntry &a,
           const ProgrammeEntry &b) {
            return a.start < b.start;
        });

    const QDateTime now =
        QDateTime::currentDateTime();

    int currentIndex = -1;

    for (int i = 0;
         i < entries.size();
         ++i) {

        if (entries.at(i).start <= now &&
            now < entries.at(i).end) {

            currentIndex = i;
            break;
        }
    }

    QString newNow;
    QString newNext;

    QDateTime currentEnd;

    if (currentIndex >= 0) {
        const ProgrammeEntry &e =
            entries.at(currentIndex);

        currentEnd = e.end;

        newNow =
            QStringLiteral("%1-%2  %3")
                .arg(
                    e.start.toLocalTime()
                        .toString(QStringLiteral("HH:mm")),
                    e.end.toLocalTime()
                        .toString(QStringLiteral("HH:mm")),
                    e.name);
    }

    if (currentIndex >= 0) {
        for (const ProgrammeEntry &e : entries) {
            if (e.start >= currentEnd) {
                newNext =
                    QStringLiteral("%1-%2  %3")
                        .arg(
                            e.start.toLocalTime()
                                .toString(
                                    QStringLiteral("HH:mm")),
                            e.end.toLocalTime()
                                .toString(
                                    QStringLiteral("HH:mm")),
                            e.name);
                break;
            }
        }
    }
    else {
        for (const ProgrammeEntry &e : entries) {
            if (e.start > now) {
                newNext =
                    QStringLiteral("%1-%2  %3")
                        .arg(
                            e.start.toLocalTime()
                                .toString(
                                    QStringLiteral("HH:mm")),
                            e.end.toLocalTime()
                                .toString(
                                    QStringLiteral("HH:mm")),
                            e.name);
                break;
            }
        }
    }

    const bool newAvailable =
        !newNow.isEmpty();

    if (newAvailable == epgAvailable_ &&
        newNow == epgNow_ &&
        newNext == epgNext_) {
        return;
    }

    epgAvailable_ = newAvailable;
    epgNow_ = newNow;
    epgNext_ = newNext;

    emit epgChanged();

    qInfo().noquote()
        << "EPG DLF Jetzt:"
        << (epgNow_.isEmpty()
                ? QStringLiteral("--")
                : epgNow_)
        << "| Danach:"
        << (epgNext_.isEmpty()
                ? QStringLiteral("--")
                : epgNext_);
}

void XdrClient::clearRdsData()
{
    rdsTimeoutTimer_.stop();
    const bool hadData = rdsActive_ || rdsPi_ >= 0 || !psText_.isEmpty() ||
                         !radioText_.isEmpty() || ptyCode_ >= 0 ||
                         !rtPlusTitle_.isEmpty() || !rtPlusArtist_.isEmpty() ||
                         !ctText_.isEmpty() || rdsGroupCount_ > 0;

    const bool hadTmcData =
        tmcActive_ ||
        tmcGroupCount_ > 0 ||
        tmcSingleCount_ > 0 ||
        tmcMultiCount_ > 0 ||
        tmcMultiOrphanCount_ > 0 ||
        !tmcMessages_.isEmpty() ||
        !tmcLastRaw_.isEmpty();

    rdsActive_ = false;
    rdsPi_ = -1;
    clearEpg();
    piCode_ = QStringLiteral("----");
    rdsEcc_ = -1;
    eccCode_ = QStringLiteral("--");
    pinText_.clear();
    psText_.clear();
    radioText_.clear();
    ptyCode_ = -1;
    ptyText_.clear();

    rtPlusTitle_.clear();
    rtPlusArtist_.clear();
    ctText_.clear();
    rtPlusGroupCode_ = -1;
    rtPlusItemToggle_ = -1;
    rtPlusItemRunning_ = false;
    rtPlusItemRunningKnown_ = false;

    tmcActive_ = false;
    tmcGroupCount_ = 0;
    tmcSingleCount_ = 0;
    tmcMultiCount_ = 0;
    tmcMultiOrphanCount_ = 0;
    tmcLocationTableNumber_ = -1;
    tmcServiceId_ = -1;
    tmcMessages_.clear();
    tmcMessageKeys_.clear();
    tmcMessageLastSeenMs_.clear();
    tmcMessageTextByKey_.clear();
    tmcMessageOrder_.clear();
    tmcMultiStates_.clear();
    tmcMultiLastSignature_.clear();
    tmcLastRaw_.clear();

    rdsGroupCount_ = 0;
    psBuffer_.fill(QLatin1Char(' '), 8);
    psSegments_.fill(false);
    rtBuffer_.fill(QLatin1Char(' '), 64);
    rtSegments_.fill(false);
    rtAbFlagKnown_ = false;
    rtAbFlag_ = false;
    rtVersionB_ = false;

    if (hadData)
        emit rdsChanged();

    if (hadTmcData)
        emit tmcChanged();
}

void XdrClient::markRdsActivity()
{
    const bool changed = !rdsActive_;
    rdsActive_ = true;
    rdsTimeoutTimer_.start();
    if (changed)
        emit rdsChanged();
}

void XdrClient::onRdsTimeout()
{
    if (!rdsActive_)
        return;
    rdsActive_ = false;
    emit rdsChanged();
}

void XdrClient::processPiLine(const QString &line)
{
    if (line.size() < 5)
        return;

    markRdsActivity();
    const QString raw = line.mid(1, 4).toUpper();
    bool ok = false;
    const int value = raw.toInt(&ok, 16);
    const QString display = raw;

    bool changed = false;
    if (display != piCode_) {
        piCode_ = display;
        changed = true;
    }
    if (ok && value != rdsPi_) {
        if (rdsPi_ >= 0) {
            rdsEcc_ = -1;
            eccCode_ = QStringLiteral("--");
            pinText_.clear();
        }

        rdsPi_ = value;
        refreshEpg();
        changed = true;
    }
    if (changed)
        emit rdsChanged();
}


void XdrClient::recordRdsMonitorLine(const QString &line,
                                     quint16 blockB,
                                     quint8 errors)
{
    /*
     * Beim PE5PVB-Legacyformat stammen B/C/D direkt aus
     * der RDS-Zeile. Block A/PI kommt separat über Pxxxx.
     *
     * Deshalb zeigt der Monitor bewusst die Fehlerstatistik
     * für B, C und D und erfindet keinen Block-A-Status.
     */
    for (int i = 0; i < 3; ++i) {
        const int status =
            rdsBlockError(errors, i + 1);

        if (status >= 0 && status <= 3) {
            ++rdsBlockErrorCounts_
                [static_cast<std::size_t>(i)]
                [static_cast<std::size_t>(status)];
        }
    }

    const int blockBError =
        rdsBlockError(errors, 1);

    if (blockBError < 3) {
        const int groupType =
            (blockB >> 12) & 0x0F;

        const bool versionB =
            ((blockB >> 11) & 0x01) != 0;

        const int index =
            groupType * 2 +
            (versionB ? 1 : 0);

        ++rdsGroupTypeCounts_
            [static_cast<std::size_t>(index)];
    }
    else {
        ++rdsUnusableBCount_;
    }

    const QString raw =
        line.trimmed();

    if (!raw.isEmpty()) {
        rdsRawLines_.append(raw);

        while (rdsRawLines_.size() > 30)
            rdsRawLines_.removeFirst();
    }
}

void XdrClient::processRdsLine(const QString &line)
{
    const QString payload = line.mid(1).trimmed();
    quint16 blocks[4] = {0, 0, 0, 0};
    quint8 errors = 0;
    bool ok = false;

    if (payload.size() == 18) {
        for (int i = 0; i < 4; ++i) {
            blocks[i] = static_cast<quint16>(payload.mid(i * 4, 4).toUInt(&ok, 16));
            if (!ok)
                return;
        }
        errors = static_cast<quint8>(payload.mid(16, 2).toUInt(&ok, 16));
        if (!ok)
            return;
    } else if (payload.size() == 14) {
        blocks[0] = rdsPi_ >= 0 ? static_cast<quint16>(rdsPi_) : 0;
        for (int i = 1; i < 4; ++i) {
            blocks[i] = static_cast<quint16>(payload.mid((i - 1) * 4, 4).toUInt(&ok, 16));
            if (!ok)
                return;
        }
        const quint8 legacyErrors =
            static_cast<quint8>(payload.mid(12, 2).toUInt(&ok, 16));
        if (!ok)
            return;
        if (!rdsActive_)
            errors |= static_cast<quint8>(0x03U << 6U);
        errors |= static_cast<quint8>((legacyErrors & 0x03U) << 4U);
        errors |= static_cast<quint8>(legacyErrors & 0x0CU);
        errors |= static_cast<quint8>((legacyErrors & 0x30U) >> 4U);
    } else {
        return;
    }

    recordRdsMonitorLine(line, blocks[1], errors);

    markRdsActivity();
    ++rdsGroupCount_;
    processRdsGroup(blocks[0], blocks[1], blocks[2], blocks[3], errors);
    emit rdsChanged();
}


void XdrClient::updateRdsExtendedMonitor(
    quint16 blockB,
    quint16 blockC,
    quint16 blockD,
    quint8 errors)
{
    const int errB =
        rdsBlockError(errors, 1);

    const int errC =
        rdsBlockError(errors, 2);

    const int errD =
        rdsBlockError(errors, 3);

    if (errB != 0)
        return;

    const int groupType =
        (blockB >> 12) & 0x0F;

    const bool versionB =
        ((blockB >> 11) & 1) != 0;

    /*
     * TP befindet sich in jeder RDS-Gruppe
     * an derselben Position.
     */
    rdsTp_ =
        (blockB >> 10) & 1;

    /*
     * Gruppe 0: TA, MS und DI
     */
    if (groupType == 0) {
        rdsTa_ =
            (blockB >> 4) & 1;

        rdsMs_ =
            (blockB >> 3) & 1;

        const int segment =
            blockB & 0x03;

        const int di =
            (blockB >> 2) & 1;

        const int bit =
            3 - segment;

        const int mask =
            1 << bit;

        rdsDiSeenMask_ |= mask;

        if (di)
            rdsDiMask_ |= mask;
        else
            rdsDiMask_ &= ~mask;
    }

    /*
     * ============================================================
     * AF - Alternative Frequencies
     * ============================================================
     *
     * Gruppe 0A, Block C.
     *
     * Methode A:
     *   #N + erste Frequenz
     *   danach Frequenzpaare
     *
     * Methode B:
     *   #N + Bezugsfrequenz
     *   danach:
     *       Bezugsfrequenz + AF
     *
     * Die Methode ist nicht separat signalisiert.
     * Wiederholung der Bezugsfrequenz kennzeichnet Methode B.
     */
    if (groupType == 0 &&
        !versionB &&
        errC == 0) {

        const int a =
            (blockC >> 8) & 0xFF;

        const int b =
            blockC & 0xFF;

        const auto isFmCode =
            [](int code) -> bool {
                return code >= 1 &&
                       code <= 204;
            };

        const auto toKhz =
            [](int code) -> int {
                return 87500 +
                       code * 100;
            };

        const auto isCountCode =
            [](int code) -> bool {
                return code >= 224 &&
                       code <= 249;
            };

        /*
         * Beginn einer neuen AF-Liste.
         *
         * Normalerweise steht der Anzahlcode
         * im ersten Byte. Das zweite wird aus
         * Robustheitsgründen ebenfalls akzeptiert.
         */
        int countCode = -1;
        int baseCode = -1;

        if (isCountCode(a) &&
            isFmCode(b)) {

            countCode = a;
            baseCode = b;
        }
        else if (isCountCode(b) &&
                 isFmCode(a)) {

            countCode = b;
            baseCode = a;
        }

        if (countCode >= 0 &&
            baseCode >= 0) {

            const int baseKhz =
                toKhz(baseCode);

            const int expected =
                countCode - 224;

            rdsAfCurrentBaseKhz_ =
                baseKhz;

            rdsAfCurrentExpectedCount_ =
                expected;

            rdsAfCurrentMethod_ = 0;

            RdsAfListInfo &info =
                rdsAfLists_[baseKhz];

            info.expectedCount =
                expected;

            info.frequencies.insert(
                baseKhz);
        }
        else if (isFmCode(a) &&
                 isFmCode(b)) {

            const int f1 =
                toKhz(a);

            const int f2 =
                toKhz(b);

            bool handledAsMethodB =
                false;

            /*
             * Falls bereits eine Methode-B-Liste
             * für eine der beiden Frequenzen bekannt
             * ist, kann das Paar auch nach einem
             * verlorenen Header noch zugeordnet werden.
             */
            const auto addMethodBPair =
                [&](int baseKhz,
                    int otherKhz) {

                    if (!rdsAfLists_
                            .contains(baseKhz)) {
                        return false;
                    }

                    RdsAfListInfo &info =
                        rdsAfLists_[baseKhz];

                    if (info.method != 2)
                        return false;

                    /*
                     * RDS-Konvention:
                     * aufsteigendes Paar =
                     * gleiches Programm;
                     *
                     * absteigendes Paar =
                     * regional bzw. zeitweise
                     * abweichendes Programm.
                     */
                    if (f1 < f2) {
                        info.sameProgramme.insert(
                            otherKhz);
                    }
                    else if (f1 > f2) {
                        info.regionalProgramme.insert(
                            otherKhz);
                    }

                    info.frequencies.insert(
                        otherKhz);

                    return true;
                };

            if (addMethodBPair(f1, f2))
                handledAsMethodB = true;

            if (addMethodBPair(f2, f1))
                handledAsMethodB = true;

            /*
             * Normale Auswertung anhand der zuletzt
             * angekündigten Liste.
             */
            if (!handledAsMethodB &&
                rdsAfCurrentBaseKhz_ > 0) {

                RdsAfListInfo &info =
                    rdsAfLists_[
                        rdsAfCurrentBaseKhz_];

                const bool containsBase =
                    f1 ==
                        rdsAfCurrentBaseKhz_ ||
                    f2 ==
                        rdsAfCurrentBaseKhz_;

                /*
                 * Wird die Bezugsfrequenz wiederholt,
                 * ist es Methode B.
                 */
                if (containsBase) {

                    rdsAfCurrentMethod_ = 2;
                    info.method = 2;

                    const int other =
                        f1 ==
                            rdsAfCurrentBaseKhz_
                        ? f2
                        : f1;

                    info.frequencies.insert(
                        other);

                    if (f1 < f2) {
                        info.sameProgramme.insert(
                            other);
                    }
                    else if (f1 > f2) {
                        info.regionalProgramme.insert(
                            other);
                    }
                }

                /*
                 * Keine Wiederholung der
                 * Bezugsfrequenz -> Methode A.
                 */
                else {

                    if (rdsAfCurrentMethod_ == 0) {
                        rdsAfCurrentMethod_ = 1;
                        info.method = 1;
                    }

                    if (rdsAfCurrentMethod_ == 1) {

                        info.frequencies.insert(f1);
                        info.frequencies.insert(f2);

                        /*
                         * Bei Methode A ist die
                         * angekündigte Zahl die
                         * Länge der Liste.
                         */
                        if (info.expectedCount > 0 &&
                            info.frequencies.size() >=
                                info.expectedCount) {

                            rdsAfCurrentBaseKhz_ = -1;
                            rdsAfCurrentExpectedCount_ = -1;
                            rdsAfCurrentMethod_ = 0;
                        }
                    }
                }
            }
        }
        else {

            /*
             * Filler 205 sowie Anzahlcodes werden
             * hier bewusst nicht als unbekannte
             * Frequenzen ausgegeben.
             */
            const int codes[2] = {a, b};

            for (int code : codes) {
                if (code == 0 ||
                    code == 205 ||
                    isCountCode(code)) {
                    continue;
                }

                if (!isFmCode(code))
                    rdsAfOtherCodes_.insert(code);
            }
        }
    }

    /*
     * Sprache:
     * Gruppe 1A, Slow-Labelling Variante 3.
     */
    if (groupType == 1 &&
        !versionB &&
        errC == 0) {

        const int variant =
            (blockC >> 12) & 0x07;

        if (variant == 3) {
            rdsLanguageCode_ =
                blockC & 0xFF;
        }
    }

    /*
     * PTYN:
     * Gruppe 10A.
     */
    if (groupType == 10 &&
        !versionB &&
        errC == 0 &&
        errD == 0) {

        const bool ab =
            ((blockB >> 4) & 1) != 0;

        const int segment =
            blockB & 1;

        if (rdsPtynAbKnown_ &&
            ab != rdsPtynAb_) {

            rdsPtynBuffer_.fill(
                QLatin1Char(' '),
                8);

            rdsPtynSegments_.fill(false);
        }

        rdsPtynAbKnown_ = true;
        rdsPtynAb_ = ab;

        const quint8 bytes[4] = {
            static_cast<quint8>(
                (blockC >> 8) & 0xFF),
            static_cast<quint8>(
                blockC & 0xFF),
            static_cast<quint8>(
                (blockD >> 8) & 0xFF),
            static_cast<quint8>(
                blockD & 0xFF)
        };

        const int base =
            segment * 4;

        for (int i = 0; i < 4; ++i) {
            rdsPtynBuffer_[base + i] =
                decodeRdsCharacter(bytes[i]);
        }

        rdsPtynSegments_[
            static_cast<std::size_t>(
                segment)] = true;
    }

    /*
     * ODA-Ankündigungen:
     * Gruppe 3A.
     *
     * Block D = AID
     * untere 5 Bit von B = zugeordnete Gruppe.
     */
    if (groupType == 3 &&
        !versionB &&
        errD == 0) {

        const quint16 aid =
            blockD;

        const int groupCode =
            blockB & 0x1F;

        rdsOdaGroupCode_[aid] =
            groupCode;

        ++rdsOdaCount_[aid];

        if (errC == 0)
            rdsOdaLastData_[aid] =
                blockC;

        // XDRTABLET_TMC_ECL_LCL_V1
        // TMC System Information aus der 3A-Ankündigung.
        if (aid == static_cast<quint16>(0xCD46) && errC == 0) {
            const int variant =
                (blockC >> 14) & 0x03;

            bool changed = false;

            if (variant == 0) {
                const int ltn =
                    (blockC >> 6) & 0x3F;
                if (tmcLocationTableNumber_ != ltn) {
                    tmcLocationTableNumber_ = ltn;
                    changed = true;
                }
            }
            else if (variant == 1) {
                const int sid =
                    (blockC >> 6) & 0x3F;
                if (tmcServiceId_ != sid) {
                    tmcServiceId_ = sid;
                    changed = true;
                }
            }

            if (changed)
                emit tmcChanged();
        }
    }

    /*
     * EON: Gruppe 14A / 14B.
     *
     * Block D enthält PI(ON).
     */
    if (groupType == 14 &&
        errD == 0) {

        const quint16 onPi =
            blockD;

        RdsEonInfo &info =
            rdsEon_[onPi];

        ++info.groups;

        /*
         * 14B: TA switching burst.
         */
        if (versionB) {
            ++info.taBursts;
            return;
        }

        if (errC != 0)
            return;

        const int variant =
            blockB & 0x0F;

        info.tp =
            (blockB >> 4) & 1;

        ++info.variantCounts[
            static_cast<std::size_t>(
                variant)];

        info.lastVariantData[
            variant] = blockC;

        /*
         * Varianten 0..3:
         * PS des Other Network.
         */
        if (variant >= 0 &&
            variant <= 3) {

            const int pos =
                variant * 2;

            info.ps[pos] =
                decodeRdsCharacter(
                    static_cast<quint8>(
                        (blockC >> 8) &
                        0xFF));

            info.ps[pos + 1] =
                decodeRdsCharacter(
                    static_cast<quint8>(
                        blockC &
                        0xFF));

            info.psSeen[
                static_cast<std::size_t>(
                    variant)] = true;
        }

        /*
         * Variante 4:
         * AF(ON).
         */
        else if (variant == 4) {

            const int codes[2] = {
                (blockC >> 8) & 0xFF,
                blockC & 0xFF
            };

            for (int code : codes) {
                if (code >= 1 &&
                    code <= 204) {

                    info.afKhz.insert(
                        87500 +
                        code * 100);
                }
            }
        }

        /*
         * Varianten 5..9:
         * Mapped Frequency Pair.
         */
        else if (variant >= 5 &&
                 variant <= 9) {

            const int a =
                (blockC >> 8) & 0xFF;

            const int b =
                blockC & 0xFF;

            if (a >= 1 && a <= 204 &&
                b >= 1 && b <= 204) {

                const double own =
                    (87500 +
                     a * 100) /
                    1000.0;

                const double other =
                    (87500 +
                     b * 100) /
                    1000.0;

                info.mappedAf.insert(
                    QStringLiteral(
                        "%1 -> %2 MHz")
                        .arg(
                            own,
                            0, 'f', 1)
                        .arg(
                            other,
                            0, 'f', 1));
            }
        }

        /*
         * Variante 12:
         * Linkage Set.
         */
        else if (variant == 12) {

            const int la =
                (blockC >> 15) & 1;

            const int eg =
                (blockC >> 14) & 1;

            const int ils =
                (blockC >> 12) & 1;

            const int lsn =
                blockC & 0x0FFF;

            info.linkage =
                QStringLiteral(
                    "LA=%1 EG=%2 ILS=%3 LSN=%4")
                    .arg(la)
                    .arg(eg)
                    .arg(ils)
                    .arg(
                        lsn,
                        3, 16,
                        QLatin1Char('0'))
                    .toUpper();
        }

        /*
         * Variante 13:
         * PTY(ON) + TA(ON).
         */
        else if (variant == 13) {

            info.pty =
                (blockC >> 11) &
                0x1F;

            info.ta =
                blockC & 1;
        }

        /*
         * Variante 14:
         * PIN(ON).
         */
        else if (variant == 14) {

            const int day =
                (blockC >> 11) &
                0x1F;

            const int hour =
                (blockC >> 6) &
                0x1F;

            const int minute =
                blockC & 0x3F;

            if (day >= 1 &&
                day <= 31 &&
                hour >= 0 &&
                hour <= 23 &&
                minute >= 0 &&
                minute <= 59) {

                info.pin =
                    QStringLiteral(
                        "%1. %2:%3")
                        .arg(
                            day,
                            2, 10,
                            QLatin1Char('0'))
                        .arg(
                            hour,
                            2, 10,
                            QLatin1Char('0'))
                        .arg(
                            minute,
                            2, 10,
                            QLatin1Char('0'));
            }
        }
    }
}

void XdrClient::processRdsGroup(quint16 blockA, quint16 blockB,
                                 quint16 blockC, quint16 blockD, quint8 errors)
{

    /*
     * AUS: nur Status 0
     * EIN: Status 0, 1 und 2
     * Status 3 ist immer unbrauchbar.
     */
    const auto rdsTextBlockUsable =
        [&](int blockIndex) -> bool {
            const int error =
                rdsBlockError(errors, blockIndex);

            if (rdsErrorCorrectionEnabled_)
                return error < 3;

            return error == 0;
        };


    /*
     * PI darf weiterhin die vom TEF korrigierten Werte verwenden.
     */
    if (rdsBlockError(errors, 0) < 3) {
        const int newPi =
            static_cast<int>(blockA);

        if (rdsPi_ >= 0 &&
            newPi != rdsPi_) {

            rtPlusTitle_.clear();
            rtPlusArtist_.clear();
            ctText_.clear();

            rtPlusGroupCode_ = -1;
            rtPlusItemToggle_ = -1;
    rtPlusItemRunning_ = false;
    rtPlusItemRunningKnown_ = false;
            rtPlusItemRunning_ = false;
            rtPlusItemRunningKnown_ = false;
        }

        if (newPi != rdsPi_) {
            if (rdsPi_ >= 0) {
                rdsEcc_ = -1;
                eccCode_ = QStringLiteral("--");
                pinText_.clear();
            }

            rdsPi_ = newPi;
            refreshEpg();

            piCode_ =
                QStringLiteral("%1")
                    .arg(newPi,
                         4, 16,
                         QLatin1Char('0'))
                    .toUpper();
        }
    }

    /*
     * Ohne brauchbaren Block B kann die Gruppe generell
     * nicht ausgewertet werden.
     */
    if (rdsBlockError(errors, 1) >= 3)
        return;

    const int groupType =
        (blockB >> 12) & 0x0F;

    const bool versionB =
        ((blockB >> 11) & 0x01) != 0;

    updateRdsExtendedMonitor(
        blockB,
        blockC,
        blockD,
        errors);

    /* XDRTABLET_TMC_LED_TEST_V1
     * XDRTABLET_TMC_SINGLE_V1
     *
     * AID 0xCD46 = TMC / ALERT-C, groupCode 16 = 8A.
     * Die bereits getestete LED-Logik bleibt erhalten.
     * Zusätzlich werden Single-Group-User-Messages zerlegt.
     *
     * B, untere 5 Bit:
     *   X4      T: 0 User Message, 1 Tuning/System
     *   X3      F: 1 Single Group, 0 Multi Group
     *   X2..X0  DP bei Single Group
     *
     * C bei Single Group:
     *   C15     Umleitung
     *   C14     Richtung (1 = negativ, 0 = positiv)
     *   C13..11 Extent
     *   C10..0  Event Code
     * D15..0    Location Code
     */
    const bool tmcAssignedTo8A =
        rdsOdaGroupCode_.value(
            static_cast<quint16>(0xCD46),
            -1) == 16;

    const bool validTmc8A =
        groupType == 8 &&
        !versionB &&
        tmcAssignedTo8A &&
        rdsBlockError(errors, 1) == 0 &&
        rdsBlockError(errors, 2) == 0 &&
        rdsBlockError(errors, 3) == 0;

    if (validTmc8A) {
        tmcActive_ = true;
        ++tmcGroupCount_;

        tmcLastRaw_ =
            QStringLiteral("PI=%1  B=%2  C=%3  D=%4")
                .arg(static_cast<int>(blockA),
                     4, 16, QLatin1Char('0'))
                .arg(static_cast<int>(blockB),
                     4, 16, QLatin1Char('0'))
                .arg(static_cast<int>(blockC),
                     4, 16, QLatin1Char('0'))
                .arg(static_cast<int>(blockD),
                     4, 16, QLatin1Char('0'))
                .toUpper();

        const int x =
            blockB & 0x1F;

        const int t =
            (x >> 4) & 0x01;

        const int f =
            (x >> 3) & 0x01;

        const int low3 =
            x & 0x07;

        // T=0 und F=1: echte Single-Group-Verkehrsmeldung.
        if (t == 0 && f == 1) {
            const int dp =
                low3;

            const bool diversion =
                (blockC & 0x8000U) != 0;

            const int direction =
                (blockC & 0x4000U)
                    ? -1
                    : +1;

            const int extent =
                (blockC >> 11) & 0x07;

            const int eventCode =
                blockC & 0x07FF;

            const int locationCode =
                blockD;

            ++tmcSingleCount_;

            const QString key =
                QStringLiteral(
                    "%1/%2/%3/%4/%5/%6")
                    .arg(eventCode)
                    .arg(locationCode)
                    .arg(direction)
                    .arg(extent)
                    .arg(dp)
                    .arg(diversion ? 1 : 0);

                const QString directionText =
                    direction < 0
                        ? QStringLiteral("- / negative LCL-Richtung")
                        : QStringLiteral("+ / positive LCL-Richtung");

                const XdrTmcTables::EventInfo *eventInfo =
                    XdrTmcTables::findEvent(eventCode);

                const QString eventText =
                    tmcEventText(eventCode);

                QString durationText;
                if (dp == 0) {
                    durationText =
                        QStringLiteral("keine explizite Dauer (Code 0)");
                }
                else {
                    const QString dtype =
                        eventInfo && eventInfo->durationType[0]
                            ? QString::fromUtf8(eventInfo->durationType)
                            : QString();

                    QString dtypeName;
                    if (dtype == QStringLiteral("D"))
                        dtypeName = QStringLiteral("dynamisch/kurz");
                    else if (dtype == QStringLiteral("L"))
                        dtypeName = QStringLiteral("länger andauernd");
                    else if (dtype == QStringLiteral("(D)"))
                        dtypeName = QStringLiteral("dynamisch, nur Persistenz/Verwaltung");
                    else if (dtype == QStringLiteral("(L)"))
                        dtypeName = QStringLiteral("länger andauernd, nur Persistenz/Verwaltung");

                    if (!dtype.isEmpty()) {
                        durationText =
                            QStringLiteral("Stufe %1/7, Typ %2 (%3)")
                                .arg(dp)
                                .arg(dtype)
                                .arg(dtypeName.isEmpty() ? dtype : dtypeName);
                    }
                    else {
                        durationText =
                            QStringLiteral("Stufe %1/7 (Dauertyp unbekannt)")
                                .arg(dp);
                    }
                }

                const auto formatLocation =
                    [](const XdrTmcTables::LocationInfo *loc) -> QString {
                        if (!loc)
                            return QString();

                        QStringList parts;
                        const QString road = QString::fromUtf8(loc->road);
                        const QString name = QString::fromUtf8(loc->name);

                        if (!road.isEmpty())
                            parts << road;
                        if (!name.isEmpty() && !parts.contains(name))
                            parts << name;

                        if (loc->lat >= 46.0 && loc->lat <= 56.5 &&
                            loc->lon >= 4.0 && loc->lon <= 16.5) {
                            parts << QStringLiteral("%1,%2")
                                         .arg(loc->lat, 0, 'f', 5)
                                         .arg(loc->lon, 0, 'f', 5);
                        }

                        return parts.join(QStringLiteral(" | "));
                    };

                QString locationLine;
                QStringList sectionLines;

                if (tmcLocationTableNumber_ == 1) {
                    const XdrTmcTables::LocationInfo *start =
                        XdrTmcTables::findLocation(locationCode);

                    const QString loc = formatLocation(start);
                    if (!loc.isEmpty()) {
                        locationLine =
                            QStringLiteral("Ort (LTN 1): %1").arg(loc);
                    }
                    else {
                        locationLine =
                            QStringLiteral("Ort (LTN 1): Location %1 nicht gefunden")
                                .arg(locationCode);
                    }

                    if (start) {
                        QStringList codes;
                        const XdrTmcTables::LocationInfo *current = start;
                        const XdrTmcTables::LocationInfo *last = start;
                        QString problem;

                        for (int step = 0; step <= extent; ++step) {
                            if (!current) {
                                problem = QStringLiteral("Location fehlt");
                                break;
                            }

                            codes << QString::number(current->code);
                            last = current;

                            if (step == extent)
                                break;

                            const int nextCode =
                                direction >= 0
                                    ? current->posOffset
                                    : current->negOffset;

                            if (nextCode <= 0) {
                                problem =
                                    QStringLiteral("%1 bei Location %2 fehlt")
                                        .arg(direction >= 0
                                                 ? QStringLiteral("pos_offset")
                                                 : QStringLiteral("neg_offset"))
                                        .arg(current->code);
                                break;
                            }

                            current =
                                XdrTmcTables::findLocation(nextCode);

                            if (!current) {
                                problem =
                                    QStringLiteral("Location %1 fehlt in der LCL")
                                        .arg(nextCode);
                                break;
                            }
                        }

                        sectionLines <<
                            QStringLiteral("Abschnitt: %1 (%2)")
                                .arg(codes.join(QStringLiteral(" → ")))
                                .arg(direction >= 0
                                         ? QStringLiteral("positive LCL-Richtung")
                                         : QStringLiteral("negative LCL-Richtung"));

                        const QString startText = formatLocation(start);
                        const QString endText = formatLocation(last);
                        if (!startText.isEmpty())
                            sectionLines << QStringLiteral("  Start: %1").arg(startText);
                        if (last != start && !endText.isEmpty())
                            sectionLines << QStringLiteral("  Ende : %1").arg(endText);
                        if (!problem.isEmpty())
                            sectionLines << QStringLiteral("  Hinweis: %1").arg(problem);
                    }
                }
                else if (tmcLocationTableNumber_ >= 0) {
                    locationLine =
                        QStringLiteral(
                            "Location %1 – LTN %2 ist nicht die eingebaute deutsche LTN-1-Tabelle")
                            .arg(locationCode)
                            .arg(tmcLocationTableNumber_);
                }
                else {
                    locationLine =
                        QStringLiteral("Location %1 – LTN noch nicht empfangen")
                            .arg(locationCode);
                }

                QStringList lines;
                lines << eventText;
                lines << QStringLiteral("Event %1    Location %2")
                             .arg(eventCode)
                             .arg(locationCode);
                lines << QStringLiteral("Richtung: %1    Extent: %2")
                             .arg(directionText)
                             .arg(extent);
                lines << QStringLiteral("Dauer/Persistenz: %1")
                             .arg(durationText);
                lines << QStringLiteral("Umleitung: %1")
                             .arg(diversion
                                      ? QStringLiteral("ja")
                                      : QStringLiteral("nein"));
                if (!locationLine.isEmpty())
                    lines << locationLine;
                lines << sectionLines;

                const QString message =
                    lines.join(QLatin1Char('\n'));

                upsertTmcMessage(
                    key,
                    eventCode,
                    locationCode,
                    message);
            
        }
        else if (t == 0 && f == 0) {
            const int ci = low3;

            /*
             * CI 0 ist bei ALERT-C nicht als normale unverschlüsselte
             * Multi-Message zu behandeln.
             */
            if (ci != 0) {
                const bool first =
                    (blockC & 0x8000U) != 0;

                if (first) {
                    const int direction =
                        (blockC & 0x4000U)
                            ? -1
                            : +1;

                    const int extent =
                        (blockC >> 11) & 0x07;

                    const int eventCode =
                        blockC & 0x07FF;

                    const int locationCode =
                        blockD;

                    if (tmcLocationTableNumber_ == 1 &&
                        !XdrTmcTables::findLocation(locationCode)) {
                        tmcMultiStates_.remove(ci);
                    }
                    else {
                        TmcMultiState state;
                        state.eventCode = eventCode;
                        state.locationCode = locationCode;
                        state.direction = direction;
                        state.extent = extent;
                        state.expectedMax = -1;
                        state.parts.clear();
                        tmcMultiStates_[ci] = state;
                    }
                }
                else {
                    const bool second =
                        (blockC & 0x4000U) != 0;

                    const int gsi =
                        (blockC >> 12) & 0x03;

                    const quint32 data28 =
                        ((static_cast<quint32>(blockC) & 0x0FFFU) << 16U) |
                        static_cast<quint32>(blockD);

                    auto stateIt =
                        tmcMultiStates_.find(ci);

                    if (stateIt == tmcMultiStates_.end()) {
                        ++tmcMultiOrphanCount_;
                    }
                    else {
                        TmcMultiState &state =
                            stateIt.value();

                        bool usablePart = true;

                        if (second) {
                            // In der zweiten Gruppe gibt GSI die Zahl
                            // der danach noch folgenden Gruppen an.
                            state.expectedMax = gsi;
                        }
                        else if (state.expectedMax < 0) {
                            ++tmcMultiOrphanCount_;
                            usablePart = false;
                        }

                        if (usablePart) {
                            state.parts[gsi] = data28;

                            bool complete =
                                state.expectedMax >= 0;

                            if (complete) {
                                for (int wanted = state.expectedMax;
                                     wanted >= 0;
                                     --wanted) {
                                    if (!state.parts.contains(wanted)) {
                                        complete = false;
                                        break;
                                    }
                                }
                            }

                            if (complete) {
                                const QVector<TmcOptionalLabel> labels =
                                    tmcParseOptionalLabels(
                                        state.parts,
                                        state.expectedMax);

                                const QPair<int, int> effective =
                                    tmcEffectiveDirectionExtent(
                                        state.direction,
                                        state.extent,
                                        labels);

                                const int effectiveDirection =
                                    effective.first;

                                const int effectiveExtent =
                                    effective.second;

                                QStringList partHex;
                                for (int wanted = state.expectedMax;
                                     wanted >= 0;
                                     --wanted) {
                                    partHex <<
                                        QStringLiteral("%1")
                                            .arg(
                                                state.parts.value(wanted),
                                                7, 16,
                                                QLatin1Char('0'))
                                            .toUpper();
                                }

                                const QString signature =
                                    QStringLiteral("%1/%2/%3/%4/%5")
                                        .arg(state.eventCode)
                                        .arg(state.locationCode)
                                        .arg(state.direction)
                                        .arg(state.extent)
                                        .arg(partHex.join(QLatin1Char(',')));

                                const bool repeatedForCi =
                                    tmcMultiLastSignature_.value(ci) ==
                                    signature;

                                tmcMultiLastSignature_[ci] =
                                    signature;

                                if (!repeatedForCi)
                                    ++tmcMultiCount_;

                                    const QString key =
                                        QStringLiteral("M/%1/%2/%3/%4/%5")
                                            .arg(state.eventCode)
                                            .arg(state.locationCode)
                                            .arg(effectiveDirection)
                                            .arg(effectiveExtent)
                                            .arg(signature);

                                    QStringList lines;
                                    lines << tmcEventText(state.eventCode);

                                    const int groups =
                                        2 + state.expectedMax;

                                    lines << QStringLiteral(
                                                 "TMC-MULTI: CI=%1    Gruppen=%2    Event %3    Location %4")
                                                 .arg(ci)
                                                 .arg(groups)
                                                 .arg(state.eventCode)
                                                 .arg(state.locationCode);

                                    lines << QStringLiteral(
                                                 "Richtung: %1 / %2 LCL-Richtung    Extent: %3")
                                                 .arg(
                                                     effectiveDirection < 0
                                                         ? QStringLiteral("-")
                                                         : QStringLiteral("+"))
                                                 .arg(
                                                     effectiveDirection < 0
                                                         ? QStringLiteral("negative")
                                                         : QStringLiteral("positive"))
                                                 .arg(effectiveExtent);

                                    if (effectiveDirection != state.direction ||
                                        effectiveExtent != state.extent) {
                                        lines << QStringLiteral(
                                                     "Rohwert: Richtung=%1, Extent=%2; Control-Labels angewendet")
                                                     .arg(
                                                         state.direction < 0
                                                             ? QStringLiteral("-")
                                                             : QStringLiteral("+"))
                                                     .arg(state.extent);
                                    }

                                    if (tmcEventIsCancellation(state.eventCode)) {
                                        lines << QStringLiteral(
                                                     "Storno: vorherige Meldungen für diese Location wurden entfernt.");
                                    }

                                    lines <<
                                        tmcLocationLine(
                                            state.locationCode,
                                            tmcLocationTableNumber_);

                                    lines <<
                                        tmcSectionLines(
                                            state.locationCode,
                                            effectiveDirection,
                                            effectiveExtent,
                                            tmcLocationTableNumber_);

                                    if (!labels.isEmpty()) {
                                        QStringList optionalLines;

                                        for (const TmcOptionalLabel &label : labels) {
                                            const QString line =
                                                tmcOptionalLabelText(
                                                    label,
                                                    state.eventCode);
                                            if (!line.isEmpty()) {
                                                optionalLines <<
                                                    QStringLiteral("  - %1")
                                                        .arg(line);
                                            }
                                        }

                                        if (!optionalLines.isEmpty()) {
                                            lines << QStringLiteral("Optional:");
                                            lines << optionalLines;
                                        }
                                    }

                                    const QString message =
                                        lines.join(QLatin1Char('\n'));

                                    upsertTmcMessage(
                                        key,
                                        state.eventCode,
                                        state.locationCode,
                                        message);

                                // Vollständiger CI ist geschlossen.
                                tmcMultiStates_.remove(ci);
                            }
                        }
                    }
                }
            }
        }


        emit tmcChanged();
    }

    /*
     * ============================================================
     * PIN - Programme Item Number
     * ============================================================
     *
     * RDS Gruppe 1A und 1B, Block D:
     *
     * Bits 15..11 = Tag des Monats
     * Bits 10..6  = Stunde
     * Bits 5..0   = Minute
     *
     * Für PIN werden nur fehlerfreie Blöcke B und D verwendet.
     */
    if (groupType == 1 &&
        rdsBlockError(errors, 1) == 0 &&
        rdsBlockError(errors, 3) == 0) {

        const int pinDay =
            (blockD >> 11) & 0x1F;

        const int pinHour =
            (blockD >> 6) & 0x1F;

        const int pinMinute =
            blockD & 0x3F;

        QString newPin;

        if (pinDay >= 1 && pinDay <= 31 &&
            pinHour >= 0 && pinHour <= 23 &&
            pinMinute >= 0 && pinMinute <= 59) {

            newPin =
                QStringLiteral("%1. %2:%3")
                    .arg(pinDay,
                         2, 10,
                         QLatin1Char('0'))
                    .arg(pinHour,
                         2, 10,
                         QLatin1Char('0'))
                    .arg(pinMinute,
                         2, 10,
                         QLatin1Char('0'));
        }

        if (newPin != pinText_) {
            pinText_ = newPin;

            qInfo().noquote()
                << "RDS PIN:"
                << (pinText_.isEmpty()
                    ? QStringLiteral("--")
                    : pinText_);
        }
    }

    /*
     * ============================================================
     * ECC - Extended Country Code
     * ============================================================
     *
     * RDS Gruppe 1A, Slow-Labelling-Variante 0.
     * Für ECC werden nur fehlerfreie Blöcke B und C verwendet.
     */
    if (groupType == 1 &&
        !versionB &&
        rdsBlockError(errors, 1) == 0 &&
        rdsBlockError(errors, 2) == 0) {

        const int variant =
            (blockC >> 12) & 0x07;

        if (variant == 0) {
            const int newEcc =
                blockC & 0x00FF;

            if (newEcc != rdsEcc_) {
                rdsEcc_ = newEcc;

                eccCode_ =
                    QStringLiteral("%1")
                        .arg(newEcc,
                             2, 16,
                             QLatin1Char('0'))
                        .toUpper();

                qInfo().noquote()
                    << "RDS ECC:"
                    << eccCode_;
            }
        }
    }


    /*
     * PTY:
     * Fehlerkorrektur AUS -> nur fehlerfreier Block B
     * Fehlerkorrektur EIN -> auch korrigierte Blöcke 1/2
     */
    if (rdsTextBlockUsable(1)) {
        updatePty(
            (blockB >> 5) & 0x1F);
    }

    /*
     * ============================================================
     * PS
     * ============================================================
     *
     * Block B enthält die Segmentnummer,
     * Block D enthält die beiden PS-Zeichen.
     *
     * Beide Blöcke müssen zur Einstellung des vorhandenen
     * RDS-Fehlerkorrektur-Schalters passen.
     */
    if (groupType == 0 &&
        rdsTextBlockUsable(1) &&
        rdsTextBlockUsable(3)) {

        updatePsSegment(
            blockB & 0x03,
            blockD);
    }

    /*
     * ============================================================
     * RadioText 2A / 2B
     * ============================================================
     *
     * TEST:
     *
     * Nur Fehlerstatus 0 verwenden.
     */
    else if (groupType == 2) {

        /*
         * Block B enthält unter anderem:
         * - Gruppe
         * - A/B-Flag
         * - Segmentnummer
         *
         * Deshalb muss auch B fehlerfrei sein.
         */
        if (!rdsTextBlockUsable(1))
            return;

        /*
         * Block D enthält bei 2A und 2B Text.
         */
        if (!rdsTextBlockUsable(3))
            return;

        /*
         * 2A verwendet zusätzlich Block C.
         */
        if (!versionB &&
            !rdsTextBlockUsable(2))
            return;

        const bool abFlag =
            ((blockB >> 4) & 0x01) != 0;

        updateRadioTextSegment(
            versionB,
            blockB & 0x0F,
            abFlag,
            blockC,
            blockD);
    }

    /*
     * ============================================================
     * RT+ ODA-Ankündigung
     * ============================================================
     *
     * Auch hier nur fehlerfreies B und D.
     */
    if (groupType == 3 &&
        !versionB &&
        rdsTextBlockUsable(1) &&
        rdsTextBlockUsable(3) &&
        blockD == 0x4BD7) {

        const int groupCode =
            blockB & 0x1F;

        if (groupCode !=
            rtPlusGroupCode_) {

            rtPlusGroupCode_ =
                groupCode;

            const int number =
                groupCode >> 1;

            const QChar version =
                (groupCode & 1)
                    ? QLatin1Char('B')
                    : QLatin1Char('A');

            qInfo().noquote()
                << QStringLiteral(
                       "RT+ ODA erkannt: AID 4BD7 -> Gruppe %1%2")
                       .arg(number)
                       .arg(version);
        }
    }

    const int receivedGroupCode =
        (groupType << 1) |
        (versionB ? 1 : 0);

    /*
     * RT+ Item-Status.
     *
     * Item Running muss unabhängig davon ausgewertet werden,
     * ob der zugehörige RadioText bereits vollständig ist.
     *
     * Running = 1 : Programmelement läuft
     * Running = 0 : kein laufendes Programmelement /
     *               Unterbrechung
     */
    const bool receivedRtPlusDataGroup =
        rtPlusGroupCode_ >= 0 &&
        receivedGroupCode == rtPlusGroupCode_ &&
        rdsTextBlockUsable(1) &&
        rdsTextBlockUsable(2) &&
        rdsTextBlockUsable(3);

    if (receivedRtPlusDataGroup) {

        const int statusItemToggle =
            (blockB >> 4) & 0x01;

        const bool statusItemRunning =
            ((blockB >> 3) & 0x01) != 0;

        static int lastRtPlusToggle = -1;
        static int lastRtPlusRunning = -1;

        const bool statusChanged =
            statusItemToggle != lastRtPlusToggle ||
            static_cast<int>(statusItemRunning) !=
                lastRtPlusRunning;

        /*
         * RT+ Toggle sofort synchronisieren.
         *
         * Bisher wurde der Togglewechsel erst in der
         * RT+-Textauswertung behandelt. Zu diesem Zeitpunkt
         * konnte der neue RadioText bereits vollständig
         * empfangen worden sein und wurde dann unnötig
         * wieder verworfen.
         *
         * Jetzt beginnt die RT-Sammlung bereits beim
         * Empfang des neuen Toggle-Zustands neu.
         */
        if (rtPlusItemToggle_ < 0 ||
            statusItemToggle != rtPlusItemToggle_) {

            const bool firstItem =
                rtPlusItemToggle_ < 0;

            rtPlusItemToggle_ =
                statusItemToggle;

            /*
             * Alter Titel gehört nicht mehr zum neuen Item.
             */
            rtPlusTitle_.clear();
            rtPlusArtist_.clear();

            rtTextComplete_ = false;
            rtBuffer_.fill(QLatin1Char(' '), 64);
            rtSegments_.fill(false);

            qInfo().noquote()
                << "RT+ SYNC EARLY:"
                << (firstItem
                        ? "erster Toggle"
                        : "Togglewechsel auf")
                << statusItemToggle;
        }

        rtPlusItemRunningKnown_ = true;

        const bool wasRunning =
            rtPlusItemRunning_;

        rtPlusItemRunning_ =
            statusItemRunning;

        /*
         * RT+ Item Running = 0:
         *
         * Es läuft momentan kein RT+-Programmelement.
         * Titel und Interpret deshalb aus der Anzeige entfernen.
         *
         * Gleichzeitig die interne RT-Sammlung neu beginnen,
         * damit beim nächsten START keine RT+-Positionen auf
         * RadioText einer Unterbrechung angewendet werden.
         */
        if (!statusItemRunning &&
            (wasRunning ||
             !rtPlusTitle_.isEmpty() ||
             !rtPlusArtist_.isEmpty())) {

            rtPlusTitle_.clear();
            rtPlusArtist_.clear();

            rtTextComplete_ = false;
            rtBuffer_.fill(QLatin1Char(' '), 64);
            rtSegments_.fill(false);

            qInfo().noquote()
                << "RT+: STOP - Titelanzeige gelöscht";
        }

        if (statusChanged) {

            qInfo().noquote()
                << "RT+ STATUS:"
                << "Toggle =" << statusItemToggle
                << "| Running ="
                << (statusItemRunning ? 1 : 0)
                << "|"
                << (statusItemRunning
                        ? "START"
                        : "STOP");

            lastRtPlusToggle =
                statusItemToggle;

            lastRtPlusRunning =
                statusItemRunning ? 1 : 0;
        }
    }

    /*
     * ============================================================
     * RT+ Daten
     * ============================================================
     *
     * B, C und D müssen Fehlerstatus 0 besitzen.
     */
    if (receivedRtPlusDataGroup &&
        rtTextComplete_) {

        const int itemToggle =
            (blockB >> 4) & 0x01;

        const bool itemRunning =
            ((blockB >> 3) & 0x01) != 0;

        const int contentType1 =
            ((blockB & 0x0007) << 3) |
            ((blockC >> 13) & 0x0007);

        const int start1 =
            (blockC >> 7) & 0x003F;

        const int length1 =
            (blockC >> 1) & 0x003F;

        const int contentType2 =
            ((blockC & 0x0001) << 5) |
            ((blockD >> 11) & 0x001F);

        const int start2 =
            (blockD >> 5) & 0x003F;

        const int length2 =
            blockD & 0x001F;

        /*
         * RT+ Synchronisation:
         *
         * Ein Wechsel des Item-Toggle bedeutet ein neues
         * Programmelement. Die neuen RT+-Positionsdaten dürfen
         * nicht mehr auf den noch sichtbaren alten RadioText
         * angewendet werden.
         *
         * Deshalb RT-Sammlung neu beginnen und dieses RT+-Paket
         * noch nicht auswerten. Der alte sichtbare RT/RT+-Text
         * bleibt dabei erhalten.
         */
        if (rtPlusItemToggle_ >= 0 &&
            itemToggle != rtPlusItemToggle_) {

            rtPlusItemToggle_ = itemToggle;

            /*
             * Das bisherige Item ist mit dem Togglewechsel
             * beendet. Alten Titel/Interpret deshalb nicht
             * weiter als aktuell anzeigen.
             */
            rtPlusTitle_.clear();
            rtPlusArtist_.clear();

            rtTextComplete_ = false;
            rtBuffer_.fill(QLatin1Char(' '), 64);
            rtSegments_.fill(false);

            qInfo().noquote()
                << "RT+ SYNC:"
                << "Togglewechsel auf"
                << itemToggle
                << "- warte auf vollständigen neuen RadioText";

            return;
        }

        /*
         * Auch beim allerersten RT+-Item zunächst
         * synchronisieren.
         *
         * Beim Programmstart kann radioText_ bereits einen
         * allgemeinen Sendertext enthalten, während die
         * RT+-Positionsdaten schon zum aktuellen Musiktitel
         * gehören.
         */
        if (rtPlusItemToggle_ < 0) {

            rtPlusItemToggle_ = itemToggle;

            rtTextComplete_ = false;
            rtBuffer_.fill(QLatin1Char(' '), 64);
            rtSegments_.fill(false);

            qInfo().noquote()
                << "RT+ SYNC:"
                << "erster Toggle"
                << itemToggle
                << "- warte auf vollständigen RadioText";

            return;
        }

        auto extractText =
            [&](int startPosition,
                int encodedLength)
                -> QString {

                const int count =
                    encodedLength + 1;

                /*
                 * radioText_ wird erst nach einem kompletten
                 * fehlerfreien Durchlauf gesetzt.
                 */
                if (startPosition < 0 ||
                    count <= 0 ||
                    startPosition + count >
                        radioText_.size())
                    return QString();

                const int endPosition =
                    startPosition + count;

                /*
                 * RT+-Positionen dürfen nicht mitten in einem
                 * Wort beginnen oder enden.
                 *
                 * Damit werden Positionsdaten verworfen, die
                 * noch zu einem anderen RadioText gehören.
                 */
                auto isWordCharacter =
                    [](QChar ch) -> bool {
                        return ch.isLetterOrNumber();
                    };

                if (startPosition > 0 &&
                    isWordCharacter(
                        radioText_.at(startPosition - 1)) &&
                    isWordCharacter(
                        radioText_.at(startPosition))) {

                    return QString();
                }

                if (endPosition <
                        radioText_.size() &&
                    isWordCharacter(
                        radioText_.at(endPosition - 1)) &&
                    isWordCharacter(
                        radioText_.at(endPosition))) {

                    return QString();
                }

                return radioText_
                    .mid(startPosition,
                         count)
                    .trimmed();
            };

        QString newTitle;
        QString newArtist;

        bool gotTitle = false;
        bool gotArtist = false;

        auto decodeTag =
            [&](int contentType,
                int startPosition,
                int encodedLength) {

                const QString value =
                    extractText(
                        startPosition,
                        encodedLength);

                if (value.isEmpty())
                    return;

                if (contentType == 1) {
                    newTitle = value;
                    gotTitle = true;
                }

                else if (contentType == 4) {
                    newArtist = value;
                    gotArtist = true;
                }
            };

        if (itemRunning) {

            decodeTag(
                contentType1,
                start1,
                length1);

            decodeTag(
                contentType2,
                start2,
                length2);

            /*
             * Für die Musiktitelanzeige verlangen wir
             * ITEM.TITLE und ITEM.ARTIST gemeinsam.
             */
            if (!gotTitle || !gotArtist) {

                /*
                 * Das Paket passt momentan nicht vollständig
                 * zum RadioText.
                 *
                 * Einen bereits gültigen laufenden Titel
                 * NICHT löschen. Er wird nur bei STOP oder
                 * bei einem neuen Item-Toggle entfernt.
                 */
                qInfo().noquote()
                    << "RT+: unvollständiges Paket - bisherigen Titel behalten";

                return;
            }
        }

        /*
         * KEINE Zweifachbestätigung mehr.
         */
        if (newTitle !=
                rtPlusTitle_ ||
            newArtist !=
                rtPlusArtist_) {

            rtPlusTitle_ =
                newTitle;

            rtPlusArtist_ =
                newArtist;

            qInfo().noquote()
                << "RT+: Titel ="
                << rtPlusTitle_
                << "| Interpret ="
                << rtPlusArtist_
                << "| Toggle ="
                << itemToggle;
        }
    }

    /*
     * ============================================================
     * CT / Clock Time
     * ============================================================
     *
     * B, C und D nur mit Fehlerstatus 0.
     */
    if (groupType == 4 &&
        !versionB &&
        rdsTextBlockUsable(1) &&
        rdsTextBlockUsable(2) &&
        rdsTextBlockUsable(3)) {

        const int mjd =
            ((blockB & 0x0003) << 15) |
            ((blockC >> 1) & 0x7FFF);

        const int utcHour =
            ((blockC & 0x0001) << 4) |
            ((blockD >> 12) &
             0x000F);

        const int utcMinute =
            (blockD >> 6) &
            0x003F;

        int offsetHalfHours =
            blockD & 0x001F;

        if (blockD & 0x0020)
            offsetHalfHours =
                -offsetHalfHours;

        if (utcHour < 24 &&
            utcMinute < 60) {

            QDate date =
                QDate::fromJulianDay(
                    static_cast<qint64>(
                        mjd) +
                    2400001LL);

            /*
             * Die zusätzliche Plausibilitätskontrolle
             * behalten wir trotzdem bei.
             */
            if (date.isValid() &&
                qAbs(
                    date.daysTo(
                        QDate::currentDate()))
                    <= 2 &&
                qAbs(offsetHalfHours)
                    <= 28) {

                const int offsetMinutes =
                    offsetHalfHours * 30;

                int localMinutes =
                    utcHour * 60 +
                    utcMinute +
                    offsetMinutes;

                while (localMinutes < 0) {
                    localMinutes += 1440;
                    date =
                        date.addDays(-1);
                }

                while (localMinutes >=
                       1440) {

                    localMinutes -= 1440;
                    date =
                        date.addDays(1);
                }

                const int localHour =
                    localMinutes / 60;

                const int localMinute =
                    localMinutes % 60;

                const int absOffset =
                    qAbs(offsetMinutes);

                const QString offsetText =
                    QStringLiteral(
                        "UTC%1%2:%3")
                        .arg(
                            offsetMinutes < 0
                            ? QStringLiteral("-")
                            : QStringLiteral("+"))
                        .arg(
                            absOffset / 60,
                            2, 10,
                            QLatin1Char('0'))
                        .arg(
                            absOffset % 60,
                            2, 10,
                            QLatin1Char('0'));

                const QString newCt =
                    QStringLiteral(
                        "%1 %2:%3 (%4)")
                        .arg(
                            date.toString(
                                QStringLiteral(
                                    "dd.MM.yyyy")))
                        .arg(
                            localHour,
                            2, 10,
                            QLatin1Char('0'))
                        .arg(
                            localMinute,
                            2, 10,
                            QLatin1Char('0'))
                        .arg(offsetText);

                if (newCt != ctText_) {
                    ctText_ = newCt;

                    qInfo().noquote()
                        << "RDS CT:"
                        << ctText_;
                }
            }
        }
    }
}

void XdrClient::updatePty(int code)
{
    if (code < 0 || code > 31)
        return;
    const QString text = ptyName(code);
    if (ptyCode_ == code && ptyText_ == text)
        return;
    ptyCode_ = code;
    ptyText_ = text;
}

void XdrClient::updatePsSegment(int segment, quint16 blockD)
{
    if (segment < 0 || segment > 3)
        return;

    const int offset = segment * 2;
    psBuffer_[offset] = decodeRdsCharacter(static_cast<quint8>(blockD >> 8));
    psBuffer_[offset + 1] = decodeRdsCharacter(static_cast<quint8>(blockD & 0xFF));
    psSegments_[static_cast<std::size_t>(segment)] = true;

    QString display = psBuffer_;
    while (display.endsWith(QLatin1Char(' ')))
        display.chop(1);
    psText_ = display;
}

void XdrClient::updateRadioTextSegment(bool versionB, int segment, bool abFlag,
                                        quint16 blockC, quint16 blockD)
{
    if (segment < 0 || segment > 15)
        return;

    /*
     * Neuer RadioText:
     * A/B-Flag oder 2A/2B hat gewechselt.
     */
    if (!rtAbFlagKnown_ ||
        rtAbFlag_ != abFlag ||
        rtVersionB_ != versionB) {

        rtBuffer_.fill(QLatin1Char(' '), 64);
        rtSegments_.fill(false);

        /*
         * Den alten sichtbaren RT/RT+ stehen lassen,
         * bis ein vollständiger neuer RadioText vorliegt.
         */
        rtTextComplete_ = false;

        rtAbFlagKnown_ = true;
        rtAbFlag_ = abFlag;
        rtVersionB_ = versionB;
    }

    if (versionB) {
        const int offset = segment * 2;

        rtBuffer_[offset] =
            decodeRdsCharacter(
                static_cast<quint8>(blockD >> 8));

        rtBuffer_[offset + 1] =
            decodeRdsCharacter(
                static_cast<quint8>(blockD & 0xFF));

    } else {
        const int offset = segment * 4;

        rtBuffer_[offset] =
            decodeRdsCharacter(
                static_cast<quint8>(blockC >> 8));

        rtBuffer_[offset + 1] =
            decodeRdsCharacter(
                static_cast<quint8>(blockC & 0xFF));

        rtBuffer_[offset + 2] =
            decodeRdsCharacter(
                static_cast<quint8>(blockD >> 8));

        rtBuffer_[offset + 3] =
            decodeRdsCharacter(
                static_cast<quint8>(blockD & 0xFF));
    }

    rtSegments_[static_cast<std::size_t>(segment)] = true;

    /*
     * Nur EIN vollständiger Durchlauf wird verlangt.
     *
     * 2A = 4 Zeichen je Segment
     * 2B = 2 Zeichen je Segment
     */
    const int charsPerSegment =
        versionB ? 2 : 4;

    const int maximumCharacters =
        versionB ? 32 : 64;

    QString candidate =
        rtBuffer_.left(maximumCharacters);

    const int terminator =
        candidate.indexOf(QChar(0x000D));

    int lastRequiredSegment = 15;

    if (terminator >= 0)
        lastRequiredSegment =
            terminator / charsPerSegment;

    /*
     * Alle Segmente dieses Textes müssen einmal mit
     * Fehlerstatus 0 angekommen sein.
     */
    for (int s = 0;
         s <= lastRequiredSegment;
         ++s) {

        if (!rtSegments_[
                static_cast<std::size_t>(s)])
            return;
    }

    if (terminator >= 0)
        candidate.truncate(terminator);

    while (candidate.endsWith(QLatin1Char(' ')))
        candidate.chop(1);

    /*
     * Der nächste angezeigte Text muss wieder aus einem
     * vollständigen Durchlauf bestehen.
     *
     * Es wird aber NICHT mehr verlangt, dass derselbe Text
     * zweimal empfangen wird.
     */
    rtSegments_.fill(false);

    if (candidate.isEmpty())
        return;

    /*
     * Ab jetzt darf RT+ wieder auf diesen RadioText zugreifen.
     */
    rtTextComplete_ = true;

    if (candidate != radioText_) {
        radioText_ = candidate;

        qInfo().noquote()
            << "RDS RT:" << radioText_;
    }
}

QString XdrClient::ptyName(int code)
{
    static const QStringList names = {
        QStringLiteral("Kein PTY"), QStringLiteral("Nachrichten"),
        QStringLiteral("Aktuelles"), QStringLiteral("Information"),
        QStringLiteral("Sport"), QStringLiteral("Bildung"),
        QStringLiteral("Hörspiel"), QStringLiteral("Kultur"),
        QStringLiteral("Wissenschaft"), QStringLiteral("Verschiedenes"),
        QStringLiteral("Popmusik"), QStringLiteral("Rockmusik"),
        QStringLiteral("Unterhaltungsmusik"), QStringLiteral("Leichte Klassik"),
        QStringLiteral("Ernste Klassik"), QStringLiteral("Sonstige Musik"),
        QStringLiteral("Wetter"), QStringLiteral("Wirtschaft"),
        QStringLiteral("Kinder"), QStringLiteral("Soziales"),
        QStringLiteral("Religion"), QStringLiteral("Hörertelefon"),
        QStringLiteral("Reisen"), QStringLiteral("Freizeit"),
        QStringLiteral("Jazz"), QStringLiteral("Country"),
        QStringLiteral("Nationale Musik"), QStringLiteral("Oldies"),
        QStringLiteral("Folk"), QStringLiteral("Dokumentation"),
        QStringLiteral("Alarmtest"), QStringLiteral("Alarm")
    };
    return (code >= 0 && code < names.size()) ? names.at(code) : QString();
}

QChar XdrClient::decodeRdsCharacter(quint8 value)
{
    /*
     * RDS verwendet nicht ISO-8859-1/Latin-1, sondern
     * den eigenen RDS-G0-Zeichensatz.
     */

    // 0x0D beendet RadioText.
    if (value == 0x0D)
        return QChar(0x000D);

    // Andere Steuerzeichen nicht anzeigen.
    if (value < 0x20)
        return QLatin1Char(' ');

    /*
     * Auch im ASCII-Bereich unterscheiden sich einige
     * Zeichen vom normalen ASCII/Latin-1.
     */
    if (value < 0x80) {
        switch (value) {
        case 0x24: return QChar(0x00A4); // ¤
        case 0x5E: return QChar(0x2015); // ―
        case 0x60: return QChar(0x2551); // ║
        case 0x7E: return QChar(0x00AF); // ¯
        case 0x7F: return QChar(0x0132); // Ĳ
        default:
            return QChar(value);
        }
    }

    /*
     * RDS G0, Codes 0x80 ... 0xFF.
     * Ein Eintrag 0x0000 wird als Leerzeichen behandelt.
     */
    static const ushort table[128] = {
        0x00E1,0x00E0,0x00E9,0x00E8,0x00ED,0x00EC,0x00F3,0x00F2,
        0x00FA,0x00F9,0x00D1,0x00C7,0x015E,0x00DF,0x00A1,0x0133,
        0x00E2,0x00E4,0x00EA,0x00EB,0x00EE,0x00EF,0x00F4,0x00F6,
        0x00FB,0x00FC,0x00F1,0x00E7,0x015F,0x011F,0x0131,0x2193,

        0x00AA,0x03B1,0x00A9,0x2030,0x011E,0x011B,0x0148,0x0151,
        0x03C0,0x20AC,0x00A3,0x0024,0x2190,0x2191,0x2192,0x00A7,
        0x00BA,0x00B9,0x00B2,0x00B3,0x00B1,0x0130,0x0144,0x0171,
        0x00B5,0x00BF,0x00F7,0x00B0,0x00BC,0x00BD,0x00BE,0x013F,

        0x00C1,0x00C0,0x00C9,0x00C8,0x00CD,0x00CC,0x00D3,0x00D2,
        0x00DA,0x00D9,0x0158,0x010C,0x0160,0x017D,0x0110,0x0140,
        0x00C2,0x00C4,0x00CA,0x00CB,0x00CE,0x00CF,0x00D4,0x00D6,
        0x00DB,0x00DC,0x0159,0x010D,0x0161,0x017E,0x0111,0x00F0,

        0x00C3,0x00C5,0x00C6,0x0152,0x0177,0x00DD,0x00D5,0x00D8,
        0x00DE,0x014A,0x0154,0x0106,0x015A,0x0179,0x0166,0x0000,
        0x00E3,0x00E5,0x00E6,0x0153,0x0175,0x00FD,0x00F5,0x00F8,
        0x00FE,0x014B,0x0155,0x0107,0x015B,0x017A,0x0167,0x0000
    };

    const ushort unicode = table[value - 0x80];

    if (unicode == 0x0000)
        return QLatin1Char(' ');

    return QChar(unicode);
}

int XdrClient::rdsBlockError(quint8 errors, int blockIndex)
{
    if (blockIndex < 0 || blockIndex > 3)
        return 3;
    const int shift = (3 - blockIndex) * 2;
    return (errors >> shift) & 0x03;
}

void XdrClient::sendFrequencyCommand(int khz)
{
    const int bounded = qBound(MinimumFmFrequencyKhz, khz, MaximumFmFrequencyKhz);
    sendLine(QStringLiteral("T%1").arg(bounded));
}

void XdrClient::advanceSeek()
{
    if (!seeking_ || seekDirection_ == 0)
        return;

    const int next = frequencyKhz_ + seekDirection_ * smallStepKhz_;
    if (next < MinimumFmFrequencyKhz || next > MaximumFmFrequencyKhz) {
        finishSeek(seekDirection_ > 0
                       ? QStringLiteral("Obere Bandgrenze 108,000 MHz erreicht")
                       : QStringLiteral("Untere Bandgrenze 87,500 MHz erreicht"));
        return;
    }

    seekSignalSum_ = 0.0;
    seekSignalSamples_ = 0;
    sendFrequencyCommand(next);
}

void XdrClient::evaluateSeekStep()
{
    if (!seeking_)
        return;

    // Es kamen für 650 ms keine weiteren T-Frequenzmeldungen.
    // Damit hat die PE5PVB-Firmware ihren Suchlauf angehalten.
    finishSeek(
        QStringLiteral("Suchlauf beendet bei %1 MHz")
            .arg(frequencyKhz_ / 1000.0, 0, 'f', 3));
}

void XdrClient::finishSeek(const QString &message)
{
    if (!seeking_)
        return;

    seekEvaluationTimer_.stop();
    seeking_ = false;
    seekDirection_ = 0;
    seekSignalSum_ = 0.0;
    seekSignalSamples_ = 0;
    saveFrequency(frequencyKhz_);
    emit seekingChanged();
    setStatusText(message);
}

void XdrClient::cancelSeekSilently()
{
    seekEvaluationTimer_.stop();
    if (!seeking_ && seekDirection_ == 0)
        return;
    seeking_ = false;
    seekDirection_ = 0;
    seekSignalSum_ = 0.0;
    seekSignalSamples_ = 0;
    emit seekingChanged();
}

void XdrClient::saveFrequency(int khz)
{
    QSettings().setValue(QStringLiteral("radio/lastFrequencyKhz"), khz);
    qInfo() << "FREQUENCY SAVED:" << khz;
}

void XdrClient::setStatusText(const QString &text)
{
    if (statusText_ == text)
        return;
    statusText_ = text;
    emit statusTextChanged();
}

void XdrClient::setReady(bool value)
{
    if (ready_ == value)
        return;
    ready_ = value;
    emit readyChanged();
}

void XdrClient::clearSignalData()
{
    const bool hadSignal = signalAvailable_ || signalLevel_ != 0.0;
    const bool hadQuality = cci_ != -1 || aci_ != -1;
    const bool hadMode = stereo_;
    const bool hadBandwidth = bandwidthHz_ != 0;

    signalAvailable_ = false;
    signalLevel_ = 0.0;
    cci_ = -1;
    aci_ = -1;
    stereo_ = false;
    bandwidthHz_ = 0;

    if (hadSignal)
        emit signalChanged();
    if (hadQuality)
        emit qualityChanged();
    if (hadMode)
        emit receptionModeChanged();
    if (hadBandwidth)
        emit bandwidthChanged();
}

void XdrClient::updateReceptionMode(bool stereo, bool forcedMono)
{
    if (stereo_ == stereo && forcedMono_ == forcedMono)
        return;
    stereo_ = stereo;
    forcedMono_ = forcedMono;
    QSettings().setValue(QStringLiteral("receiver/forcedMono"), forcedMono_);
    emit receptionModeChanged();
}
