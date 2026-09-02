import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import QtCore

ApplicationWindow {
    id: window
// Reine Tuner-Oberfläche ohne Fensterrand
    flags: Qt.Window | Qt.FramelessWindowHint
visible: true
    Shortcut {
    sequence: "Esc"
    onActivated: window.visibility = Window.Windowed
}
    // Zwei Größenklassen: Tablet/kompakt und Desktop/breit.
    readonly property bool compactLayout: width < 1180
    readonly property bool wideLayout: width >= 1180
    readonly property bool portraitLayout: height > width

    readonly property int outerMargin:
        compactLayout ? 12 : 18
    readonly property int panelMargin:
        compactLayout ? 14 : 22
    readonly property int sectionSpacing:
        compactLayout ? 11 : 14
    readonly property int controlSpacing:
        compactLayout ? 12 : 18

    readonly property int headerControlHeight: 40
    readonly property int headerControlWidth:
        compactLayout ? 138 : 150

    readonly property int scaleHeight:
        compactLayout ? 190 : 180

    readonly property int meterWidth:
        wideLayout ? 200 : 0

    readonly property int meterHeight:
        compactLayout ? 105 : 120

    readonly property int tuningKnobSize:
        compactLayout ? 125 : 135

    readonly property int smallFontSize:
        compactLayout ? 11 : 12
    readonly property int normalFontSize:
        compactLayout ? 12 : 13
    readonly property int largeFontSize:
        compactLayout ? 24 : 27

    // Desktop: immer die volle nutzbare Bildschirmbreite.
    // Android behält seine normale Fenstergröße.
    // Rechner: volle Bildschirmbreite, 620 Pixel Höhe.
    // Android-Tablet: vollständige Bildschirmgröße.
    x: Qt.platform.os === "android" ? 0 : Screen.virtualX

    width: Screen.width > 0
           ? Screen.width
           : 1920

    height: Qt.platform.os === "android"
            ? Screen.height
            : 690

    minimumWidth: Qt.platform.os === "android" ? 0 : 360
    minimumHeight: Qt.platform.os === "android" ? 0 : 300
    title: "XDR CT-610"

    Component.onCompleted: {
        xdrClient.setRdsErrorCorrectionEnabled(
            appSettings.rdsErrorCorrectionEnabled)

        // Android verwendet ausschließlich TCP.
        if (Qt.platform.os === "android")
            connectionTypeBox.currentIndex = 0
    }

    property color aluminiumLight: "#eeeeea"
    property color aluminiumMid: "#c9c9c3"
    property color aluminiumDark: "#9e9e98"
    property color ink: "#262620"
    property color mutedInk: "#686861"
    property color scaleGlass: "#d8d8cc"
    property color amber: "#b66c27"
    property color woodDark: "#4b2d1d"
    property color woodLight: "#795039"

    Window {
        id: rdsInfoWindow

        width: 1180
        height: 820
        minimumWidth: 850
        minimumHeight: 600

        visible: false
        title: "RDS-Monitor"

        color: window.aluminiumMid

        flags: Qt.Dialog
               | Qt.WindowTitleHint
               | Qt.WindowCloseButtonHint
               | Qt.WindowMaximizeButtonHint

        modality: Qt.NonModal
        transientParent: window

        property string saveMessage: ""

        Rectangle {
            anchors.fill: parent
            anchors.margins: 8

            radius: 8
            color: "#d8d8cc"
            border.width: 1
            border.color: "#8a8a82"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                /*
                 * Fester Kopf
                 */
                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true

                        text: xdrClient.psText.length > 0
                              ? xdrClient.psText
                              : "RDS-Monitor"

                        color: window.ink
                        font.pixelSize: 22
                        font.bold: true
                    }

                    Label {
                        text: "RDS-Monitor"
                        color: window.mutedInk
                        font.pixelSize: 13
                        font.bold: true
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: "Nur Daten aus dem empfangenen RDS-Signal"
                    color: window.mutedInk
                    font.pixelSize: 10
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#8a8a82"
                }

                /*
                 * Hauptbereich
                 */
                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 10

                    /*
                     * ==================================================
                     * LINKE SEITE
                     * ==================================================
                     */
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredWidth: 1

                        radius: 6
                        color: "#eeeee6"

                        border.width: 1
                        border.color: "#a5a59d"

                        ScrollView {
                            id: leftRdsScroll

                            anchors.fill: parent
                            anchors.margins: 8

                            clip: true

                            contentWidth: availableWidth

                            ScrollBar.horizontal.policy:
                                ScrollBar.AlwaysOff

                            ColumnLayout {
                                width: leftRdsScroll.availableWidth
                                spacing: 10

                                /*
                                 * Basis
                                 */
                                Label {
                                    text: "Basisdaten"
                                    color: window.ink
                                    font.pixelSize: 15
                                    font.bold: true
                                }

                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: 2
                                    columnSpacing: 14
                                    rowSpacing: 4

                                    Label {
                                        text: "PI"
                                        color: window.mutedInk
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: xdrClient.piCode
                                        color: window.ink
                                        font.family: "monospace"
                                        font.bold: true
                                    }

                                    Label {
                                        text: "PS"
                                        color: window.mutedInk
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: xdrClient.psText.length > 0
                                              ? xdrClient.psText
                                              : "–"
                                        color: window.ink
                                    }

                                    Label {
                                        text: "PTY"
                                        color: window.mutedInk
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: xdrClient.ptyCode >= 0
                                              ? xdrClient.ptyCode
                                                + "  "
                                                + xdrClient.ptyText
                                              : "–"
                                        color: window.ink
                                    }

                                    Label {
                                        text: "ECC"
                                        color: window.mutedInk
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: xdrClient.eccCode !== "--"
                                              ? xdrClient.eccCode
                                              : "–"
                                        color: window.ink
                                    }

                                    Label {
                                        text: "PIN"
                                        color: window.mutedInk
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: xdrClient.pinText.length > 0
                                              ? xdrClient.pinText
                                              : "–"
                                        color: window.ink
                                    }

                                    Label {
                                        text: "CT"
                                        color: window.mutedInk
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: xdrClient.ctText.length > 0
                                              ? xdrClient.ctText
                                              : "–"
                                        color: window.ink
                                    }

                                    Label {
                                        text: "RDS"
                                        color: window.mutedInk
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: xdrClient.rdsActive
                                              ? xdrClient.rdsGroupCount
                                                + " Gruppen"
                                              : "nicht aktiv"
                                        color: window.ink
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: "#aaa"
                                }

                                /*
                                 * Radiotext
                                 */
                                Label {
                                    text: "Radiotext / RT+"
                                    color: window.ink
                                    font.pixelSize: 15
                                    font.bold: true
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: xdrClient.radioText.length > 0
                                          ? xdrClient.radioText
                                          : "–"
                                    color: window.ink
                                    wrapMode: Text.WordWrap
                                }

                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: 2
                                    columnSpacing: 14
                                    rowSpacing: 4

                                    Label {
                                        text: "Titel"
                                        color: window.mutedInk
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: xdrClient.rtPlusTitle.length > 0
                                              ? xdrClient.rtPlusTitle
                                              : "–"
                                        color: window.ink
                                        wrapMode: Text.WordWrap
                                    }

                                    Label {
                                        text: "Interpret"
                                        color: window.mutedInk
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: xdrClient.rtPlusArtist.length > 0
                                              ? xdrClient.rtPlusArtist
                                              : "–"
                                        color: window.ink
                                        wrapMode: Text.WordWrap
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: "#aaa"
                                }

                                /*
                                 * Flags
                                 */
                                Label {
                                    text: "TP / TA / MS / DI"
                                    color: window.ink
                                    font.pixelSize: 15
                                    font.bold: true
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: xdrClient.rdsFlagsText
                                    color: window.ink
                                    font.family: "monospace"
                                }

                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: 2
                                    columnSpacing: 14

                                    Label {
                                        text: "PTYN"
                                        color: window.mutedInk
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: xdrClient.rdsPtynText
                                        color: window.ink
                                    }

                                    Label {
                                        text: "Sprache"
                                        color: window.mutedInk
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: xdrClient.rdsLanguageText
                                        color: window.ink
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: "#aaa"
                                }

                                /*
                                 * Gruppen
                                 */
                                Label {
                                    text: "RDS-Gruppen 0A bis 15B"
                                    color: window.ink
                                    font.pixelSize: 15
                                    font.bold: true
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: xdrClient.rdsGroupStats
                                    color: window.ink
                                    font.family: "monospace"
                                    font.pixelSize: 12
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: "#aaa"
                                }

                                /*
                                 * Fehlerstatus
                                 */
                                Label {
                                    text: "Block-Fehlerstatus"
                                    color: window.ink
                                    font.pixelSize: 15
                                    font.bold: true
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: xdrClient.rdsErrorStats
                                    color: window.ink
                                    font.family: "monospace"
                                    font.pixelSize: 12
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text:
                                        "0 = fehlerfrei, "
                                        + "1/2 = korrigiert, "
                                        + "3 = unbrauchbar"
                                    color: window.mutedInk
                                    font.pixelSize: 10
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: "#aaa"
                                }

                                /*
                                 * Rohgruppen
                                 */
                                Label {
                                    text: "Letzte 30 Rohgruppen"
                                    color: window.ink
                                    font.pixelSize: 15
                                    font.bold: true
                                }

                                Label {
                                    Layout.fillWidth: true

                                    text:
                                        xdrClient.rdsRawGroups.length > 0
                                        ? xdrClient.rdsRawGroups
                                        : "–"

                                    color: window.ink

                                    font.family: "monospace"
                                    font.pixelSize: 11

                                    wrapMode:
                                        Text.WrapAnywhere
                                }

                                Item {
                                    Layout.preferredHeight: 5
                                }
                            }
                        }
                    }

                    /*
                     * ==================================================
                     * RECHTE SEITE
                     * ==================================================
                     */
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredWidth: 1

                        spacing: 8

                        /*
                         * AF
                         */
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight:
                                afRightColumn.implicitHeight + 18

                            radius: 6
                            color: "#eeeee6"

                            border.width: 1
                            border.color: "#a5a59d"

                            ColumnLayout {
                                id: afRightColumn

                                anchors.fill: parent
                                anchors.margins: 8

                                spacing: 4

                                Label {
                                    text: "Alternative Frequenzen (AF)"
                                    color: window.ink
                                    font.pixelSize: 15
                                    font.bold: true
                                }

                                Label {
                                    Layout.fillWidth: true

                                    text: xdrClient.rdsAfText

                                    color: window.ink

                                    font.family: "monospace"
                                    font.pixelSize: 11

                                    wrapMode:
                                        Text.WordWrap
                                }
                            }
                        }

                        /*
                         * ODA
                         */
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight:
                                odaRightColumn.implicitHeight + 18

                            radius: 6
                            color: "#eeeee6"

                            border.width: 1
                            border.color: "#a5a59d"

                            ColumnLayout {
                                id: odaRightColumn

                                anchors.fill: parent
                                anchors.margins: 8

                                spacing: 4

                                Label {
                                    text: "ODA / Applications"
                                    color: window.ink
                                    font.pixelSize: 15
                                    font.bold: true
                                }

                                Label {
                                    Layout.fillWidth: true

                                    text: xdrClient.rdsOdaText

                                    color: window.ink

                                    font.family: "monospace"
                                    font.pixelSize: 11

                                    wrapMode:
                                        Text.WordWrap
                                }
                            }
                        }

                        /*
                         * EON bekommt ALLES was übrig bleibt
                         */
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 250

                            radius: 6
                            color: "#eeeee6"

                            border.width: 1
                            border.color: "#a5a59d"

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 4

                                Label {
                                    text: "EON / Other Networks"
                                    color: window.ink
                                    font.pixelSize: 15
                                    font.bold: true
                                }

                                Flickable {
                                    id: eonFlick

                                    Layout.fillWidth: true
                                    Layout.fillHeight: true

                                    clip: true
                                    interactive: true

                                    contentWidth: width
                                    contentHeight:
                                        eonMonitorText.implicitHeight

                                    boundsBehavior:
                                        Flickable.StopAtBounds

                                    ScrollBar.vertical: ScrollBar {
                                        policy: ScrollBar.AsNeeded
                                    }

                                    Text {
                                        id: eonMonitorText

                                        width:
                                            Math.max(
                                                1,
                                                eonFlick.width - 14)

                                        text:
                                            xdrClient.rdsEonText

                                        color: window.ink

                                        font.family:
                                            "monospace"

                                        font.pixelSize: 11

                                        wrapMode:
                                            Text.Wrap

                                        textFormat:
                                            Text.PlainText
                                    }
                                }
                            }
                        }
                    }
                }

                /*
                 * Feste untere Leiste
                 */
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#8a8a82"
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        Layout.fillWidth: true

                        visible:
                            rdsInfoWindow.saveMessage.length > 0

                        text:
                            rdsInfoWindow.saveMessage

                        color:
                            window.mutedInk

                        font.pixelSize: 10

                        elide:
                            Text.ElideMiddle
                    }

                    Button {
                        text: "Speichern"

                        onClicked: {
                            rdsInfoWindow.saveMessage =
                                xdrClient.saveRdsMonitor()
                        }
                    }

                    Button {
                        text: "Monitor zurücksetzen"

                        onClicked: {
                            xdrClient.clearRdsMonitor()
                            rdsInfoWindow.saveMessage = ""
                        }
                    }

                    Button {
                        text: "Schließen"

                        onClicked:
                            rdsInfoWindow.close()
                    }
                }
            }
        }
    }

    // XDRTABLET_TMC_LED_TEST_V1
    Window {
        id: tmcWindow
        // XDRTABLET_TMC_LED_TEST_V1
        // XDRTABLET_TMC_SINGLE_V1
        width: 700
        height: 600
        minimumWidth: 520
        minimumHeight: 420
        visible: false
        title: "TMC / ALERT-C"
        color: window.aluminiumMid
        flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint
        modality: Qt.NonModal
        transientParent: window

        // XDRTABLET_TMC_CLEAN_VIEW_V1
        property bool showTechnicalDetails: false

        // XDRTABLET_TMC_SECTION_NAMES_V1
        function tmcLocationName(line) {
            if (!line || line.length === 0)
                return ""

            const colon = line.indexOf(":")
            if (colon < 0)
                return ""

            const value = line.substring(colon + 1).trim()
            const parts = value.split("|")

            if (parts.length < 2)
                return ""

            const road = parts[0].trim()

            for (let i = 1; i < parts.length; ++i) {
                const candidate = parts[i].trim()

                if (candidate.length === 0)
                    continue

                // Koordinaten nicht als Ortsnamen verwenden.
                if (/^-?\d+[\.,]\d+\s*,\s*-?\d+[\.,]\d+$/.test(candidate))
                    continue

                if (candidate === road)
                    continue

                return candidate
            }

            return ""
        }

        function compactTmcMessage(block) {
            const lines = block.split("\n")
            const out = []

            let detourEstablished = false
            let detourRecommended = false
            let detourUnavailable = false
            let detourNoLongerRecommended = false
            let sectionStartName = ""
            let sectionEndName = ""

            for (let i = 0; i < lines.length; ++i) {
                const probe = lines[i].trim()

                if (probe.indexOf("Start:") === 0)
                    sectionStartName = tmcLocationName(probe)
                else if (probe.indexOf("Ende :") === 0
                         || probe.indexOf("Ende:") === 0)
                    sectionEndName = tmcLocationName(probe)
            }

            for (let i = 0; i < lines.length; ++i) {
                let line = lines[i]
                let t = line.trim()

                if (t.length === 0)
                    continue

                if (i === 0) {
                    out.push(t)
                    continue
                }

                if (t.indexOf("TMC-MULTI:") === 0
                        || /^Event\s+\d+\s+Location\s+\d+/.test(t)
                        || t.indexOf("Rohwert:") === 0
                        || t === "Optional:"
                        || t.indexOf("- Separator") === 0
                        || t.indexOf("- Steuerung: Richtung umkehren") === 0) {
                    continue
                }

                if (t.indexOf("Richtung:") === 0)
                    continue

                if (t.indexOf("Dauer/Persistenz: keine explizite Dauer") === 0)
                    continue

                if (t === "Umleitung: nein")
                    continue

                if (t.indexOf("Ort (LTN 1):") === 0) {
                    let value = t.substring("Ort (LTN 1):".length).trim()
                    const pipe = value.indexOf("|")
                    if (pipe >= 0)
                        value = value.substring(0, pipe).trim()

                    if (value.length > 0)
                        out.push("Straße: " + value)

                    continue
                }

                if (t.indexOf("Start:") === 0 || t.indexOf("Ende :") === 0) {
                    const label = t.indexOf("Start:") === 0 ? "Start: " : "Ende: "
                    let value = t.substring(t.indexOf(":") + 1).trim()
                    const pipe = value.indexOf("|")

                    if (pipe >= 0)
                        value = value.substring(0, pipe).trim()

                    if (value.length > 0 && value.indexOf(" ") >= 0)
                        out.push(label + value)

                    continue
                }

                if (t.indexOf("Abschnitt:") === 0) {
                    if (sectionStartName.length > 0
                            && sectionEndName.length > 0) {
                        out.push("Abschnitt: "
                                 + sectionStartName
                                 + " → "
                                 + sectionEndName)
                    } else {
                        t = t.replace(
                            /\s+\((positive|negative) LCL-Richtung\)$/,
                            "")
                        out.push(t)
                    }
                    continue
                }

                if (t.indexOf("- Zusatzereignis:") === 0) {
                    let value =
                        t.substring("- Zusatzereignis:".length).trim()
                    value = value.replace(/^\d+\s*-\s*/, "")
                    out.push("Zusätzlich: " + value)
                    continue
                }

                if (t.indexOf("- Zusatzinformation:") === 0) {
                    let value =
                        t.substring("- Zusatzinformation:".length).trim()
                    value = value.replace(/\s*\(Code \d+\)$/, "")

                    if (value === "eine Umleitung ist eingerichtet"
                            || value === "Umleitung ist eingerichtet") {
                        detourEstablished = true
                    } else if (value === "keine geeignete Umleitung verfügbar") {
                        detourUnavailable = true
                    } else if (value === "Umleitung wird nicht mehr empfohlen") {
                        detourNoLongerRecommended = true
                    } else {
                        out.push("Hinweis: " + value)
                    }
                    continue
                }

                if (t.indexOf("- Steuerung: Umleitung empfohlen/vorhanden") === 0) {
                    detourRecommended = true
                    continue
                }

                if (t.indexOf("- Dauer/Persistenz:") === 0) {
                    const value =
                        t.substring("- Dauer/Persistenz:".length).trim()

                    if (value.indexOf("Typ L") >= 0) {
                        out.push("Dauer: länger andauernd")
                    } else if (value.indexOf("Typ D") >= 0) {
                        out.push("Dauer: dynamisch")
                    } else {
                        out.push("Dauer: " + value)
                    }
                    continue
                }

                out.push(t)
            }

            if (detourEstablished && detourRecommended) {
                out.push("Umleitung: eingerichtet und empfohlen")
            } else if (detourEstablished) {
                out.push("Umleitung: eingerichtet")
            } else if (detourRecommended) {
                out.push("Umleitung: empfohlen")
            }

            // Diese beiden Zusatzinformationen können unabhängig von
            // "eingerichtet/empfohlen" gesendet werden. Deshalb bleiben
            // sie als eigene, eindeutige Zeilen sichtbar.
            if (detourUnavailable)
                out.push("Umleitung: keine geeignete verfügbar")

            if (detourNoLongerRecommended)
                out.push("Umleitung: nicht mehr empfohlen")

            return out.join("\n")
        }

        function compactTmcText(fullText) {
            if (!fullText || fullText.length === 0)
                return ""

            const blocks = fullText.split(/\n\s*\n/)
            const result = []

            for (let i = 0; i < blocks.length; ++i) {
                const compact = compactTmcMessage(blocks[i])
                if (compact.length > 0)
                    result.push(compact)
            }

            return result.join("\n\n")
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 12
            radius: 3
            color: window.aluminiumLight
            border.width: 1
            border.color: "#777770"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 10

                Label {
                    Layout.fillWidth: true
                    text: "TMC / ALERT-C"
                    color: window.ink
                    font.pixelSize: 22
                    font.bold: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#8a8a82"
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 18
                    rowSpacing: 7

                    Label { text: "Status"; color: window.mutedInk }
                    Label {
                        Layout.fillWidth: true
                        text: xdrClient.tmcActive
                              ? "TMC-Daten werden empfangen"
                              : "Noch keine gültigen TMC-Daten empfangen"
                        color: xdrClient.tmcActive
                               ? "#315b37"
                               : window.ink
                        font.bold: xdrClient.tmcActive
                    }

                    Label { text: "Sender"; color: window.mutedInk }
                    Label {
                        Layout.fillWidth: true
                        text: xdrClient.psText.length > 0
                              ? xdrClient.psText
                              : xdrClient.piCode
                        color: window.ink
                        font.bold: true
                    }

                    Label { text: "PI"; color: window.mutedInk }
                    Label {
                        Layout.fillWidth: true
                        text: xdrClient.piCode
                        color: window.ink
                        font.family: "monospace"
                    }

                    Label { text: "LTN / SID"; color: window.mutedInk }
                    Label {
                        Layout.fillWidth: true
                        text: (xdrClient.tmcLocationTableNumber >= 0
                               ? xdrClient.tmcLocationTableNumber : "–")
                              + " / "
                              + (xdrClient.tmcServiceId >= 0
                                 ? xdrClient.tmcServiceId : "–")
                        color: window.ink
                        font.family: "monospace"
                    }

                    Label { text: "gültige 8A-Gruppen"; color: window.mutedInk }
                    Label {
                        Layout.fillWidth: true
                        text: xdrClient.tmcGroupCount
                        color: window.ink
                    }

                    Label { text: "Single-Groups"; color: window.mutedInk }
                    Label {
                        Layout.fillWidth: true
                        text: xdrClient.tmcSingleCount
                        color: window.ink
                        font.bold: true
                    }

                    Label { text: "Multi komplett"; color: window.mutedInk }
                    Label {
                        Layout.fillWidth: true
                        text: xdrClient.tmcMultiCount
                        color: window.ink
                        font.bold: true
                    }

                    Label {
                        text: "Fragmente ohne Start"
                        color: window.mutedInk
                        visible: xdrClient.tmcMultiOrphanCount > 0
                    }
                    Label {
                        Layout.fillWidth: true
                        text: xdrClient.tmcMultiOrphanCount
                        color: window.ink
                        font.bold: true
                        visible: xdrClient.tmcMultiOrphanCount > 0
                    }

                    Label { text: "Aktuelle Meldungen"; color: window.mutedInk }
                    Label {
                        Layout.fillWidth: true
                        text: xdrClient.tmcMessageCount
                        color: window.ink
                        font.bold: true
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#8a8a82"
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Label {
                        Layout.fillWidth: true
                        text: "Aktuelle Verkehrsmeldungen"
                        color: window.ink
                        font.pixelSize: 16
                        font.bold: true
                    }

                    CheckBox {
                        id: tmcTechnicalDetails
                        text: "Technische Details"
                        checked: tmcWindow.showTechnicalDetails
                        onToggled:
                            tmcWindow.showTechnicalDetails = checked
                    }
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    TextArea {
                        width: parent.width
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextEdit.Wrap

                        text: xdrClient.tmcMessagesText.length > 0
                              ? (tmcWindow.showTechnicalDetails
                                 ? xdrClient.tmcMessagesText
                                 : tmcWindow.compactTmcText(
                                       xdrClient.tmcMessagesText))
                              : "Noch keine aktuelle TMC-Meldung empfangen."

                        color: window.ink
                        font.family: tmcWindow.showTechnicalDetails
                                     ? "monospace"
                                     : Qt.application.font.family
                        font.pixelSize: tmcWindow.showTechnicalDetails
                                        ? 13 : 15

                        background: Rectangle {
                            color: "#f3f3ee"
                            border.width: 1
                            border.color: "#aaa9a1"
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: tmcWindow.showTechnicalDetails
                    text: "Letzte Rohgruppe: "
                          + (xdrClient.tmcLastRaw.length > 0
                             ? xdrClient.tmcLastRaw
                             : "–")
                    color: window.mutedInk
                    font.family: "monospace"
                    font.pixelSize: window.smallFontSize
                    wrapMode: Text.WrapAnywhere
                }

                Label {
                    Layout.fillWidth: true
                    text: tmcWindow.showTechnicalDetails
                          ? "Detailansicht: vollständige ALERT-C-/LCL-Decoderinformationen. "
                            + "Storno-Meldungen entfernen die betroffene Meldung sofort; nicht mehr wiederholte "
                            + "Meldungen verschwinden nach 15 Minuten."
                          : "Kompaktansicht: technische Decoderwerte werden ausgeblendet. "
                            + "Mit „Technische Details“ kann die vollständige Ausgabe "
                            + "jederzeit eingeblendet werden."
                    // XDRTABLET_TMC_ECL_LCL_V1
                    // XDRTABLET_TMC_MULTI_V1
                    // XDRTABLET_TMC_ACTIVE_MESSAGES_V1
                    // XDRTABLET_TMC_CLEAN_VIEW_V1
                    // XDRTABLET_TMC_ECL_CORRECTIONS_V1
                    // XDRTABLET_TMC_DISPLAY_POLISH_V1
                    // XDRTABLET_TMC_DETOUR_TEXT_V2
                    // XDRTABLET_TMC_SECTION_NAMES_V1
                    color: window.mutedInk
                    font.pixelSize: window.smallFontSize
                    wrapMode: Text.WordWrap
                }
            }
        }
    }


    // Zustand des POWER-Schalters in der Oberfläche. Das xdrd-Protokoll
    // bleibt unverändert: ON verbindet, OFF trennt die TCP-Verbindung.
    property bool powerEnabled: false

    property var bandwidthValues: [0, 56000, 64000, 72000, 84000, 97000,
                                   114000, 133000, 151000, 168000, 184000,
                                   200000, 217000, 236000, 254000, 287000,
                                   311000]
    property var smallStepValues: [50, 100, 200]
    property var largeStepValues: [500, 1000, 2000]

    function valueIndex(values, value) {
        const index = values.indexOf(value)
        return index >= 0 ? index : 0
    }

    function bandwidthLabel(hz) {
        return hz > 0 ? Math.round(hz / 1000) + " kHz" : "Auto"
    }

    function refreshUsbPorts() {
        // Unter Android wird USB in XdrTablet nicht angeboten.
        if (Qt.platform.os === "android") {
            usbPortBox.model = []
            usbPortBox.currentIndex = -1
            return
        }

        const ports = xdrClient.availableSerialPorts()

        usbPortBox.model = ports

        if (ports.length === 0) {
            usbPortBox.currentIndex = -1
            return
        }

        const wanted = appSettings.usbPort
        const index = ports.indexOf(wanted)

        usbPortBox.currentIndex = index >= 0 ? index : 0
        appSettings.usbPort = usbPortBox.currentText
    }

    function connectOrDisconnect() {
        if (powerEnabled) {
            powerEnabled = false
            if (xdrClient.connected)
                xdrClient.disconnectFromServer()
        } else {
            powerEnabled = true
            if (!xdrClient.connected) {
                if (Qt.platform.os !== "android"
                        && connectionTypeBox.currentIndex === 1) {
                    xdrClient.connectToUsb(
                                usbPortBox.currentText,
                                Number(usbBaudField.text))
                } else {
                    xdrClient.connectToServer(
                                hostField.text,
                                Number(portField.text),
                                passwordField.text)
                }
            }
        }
    }

    function powerOffAndQuit() {
        powerEnabled = false
        if (xdrClient.connected)
            xdrClient.disconnectFromServer()
        Qt.quit()
    }


    readonly property var stationPresetFrequencies: [
        appSettings.preset1Khz,
        appSettings.preset2Khz,
        appSettings.preset3Khz,
        appSettings.preset4Khz,
        appSettings.preset5Khz,
        appSettings.preset6Khz
    ]

    function presetFrequency(index) {
        switch (index) {
        case 0: return appSettings.preset1Khz
        case 1: return appSettings.preset2Khz
        case 2: return appSettings.preset3Khz
        case 3: return appSettings.preset4Khz
        case 4: return appSettings.preset5Khz
        case 5: return appSettings.preset6Khz
        default: return 0
        }
    }

    function setPresetFrequency(index, frequencyKhz) {
        switch (index) {
        case 0: appSettings.preset1Khz = frequencyKhz; break
        case 1: appSettings.preset2Khz = frequencyKhz; break
        case 2: appSettings.preset3Khz = frequencyKhz; break
        case 3: appSettings.preset4Khz = frequencyKhz; break
        case 4: appSettings.preset5Khz = frequencyKhz; break
        case 5: appSettings.preset6Khz = frequencyKhz; break
        }
    }

    function storePreset(index) {
        const frequencyKhz = xdrClient.frequencyKhz

        if (frequencyKhz < xdrClient.minimumFmFrequencyKhz
                || frequencyKhz > xdrClient.maximumFmFrequencyKhz) {
            return
        }

        setPresetFrequency(index, frequencyKhz)
    }

    function clearPreset(index) {
        setPresetFrequency(index, 0)
    }

    function recallPreset(index) {
        const frequencyKhz = presetFrequency(index)

        if (frequencyKhz > 0
                && xdrClient.ready
                && !xdrClient.seeking) {
            xdrClient.setFrequencyKhz(frequencyKhz)
        }
    }

    function presetFrequencyText(index) {
        const frequencyKhz = presetFrequency(index)
        return frequencyKhz > 0
               ? (frequencyKhz / 1000).toFixed(3) + " MHz"
               : "–"
    }

    function qualityValue() {
        if (!xdrClient.signalAvailable
                || xdrClient.cci < 0
                || xdrClient.aci < 0) {
            return 0
        }

        // Die PE5PVB-Firmware liefert hier WAM und USN.
        // Kleine Werte bedeuten wenig Störungen.
        const wam = xdrClient.cci
        const usn = xdrClient.aci
        const disturbance = Math.max(wam, usn)

        return Math.round(
                    Math.max(0, Math.min(100, 100 - disturbance)))
    }

    Settings {
        id: appSettings

        property int preset1Khz: 0
        property int preset2Khz: 0
        property int preset3Khz: 0
        property int preset4Khz: 0
        property int preset5Khz: 0
        property int preset6Khz: 0

        property alias host: hostField.text
        property alias port: portField.text
        property alias connectionType: connectionTypeBox.currentIndex
        property string usbPort: "/dev/ttyUSB0"
        property alias usbBaud: usbBaudField.text

        // false = nur TEF-Fehlerstatus 0 verwenden
        // true  = auch korrigierte RDS-Blöcke 1 und 2 verwenden
        property bool rdsErrorCorrectionEnabled: false
    }

    Shortcut {
        sequence: "Left"
        enabled: xdrClient.ready && !xdrClient.seeking
        onActivated: xdrClient.stepSmall(-1)
    }
    Shortcut {
        sequence: "Right"
        enabled: xdrClient.ready && !xdrClient.seeking
        onActivated: xdrClient.stepSmall(1)
    }
    Shortcut {
        sequence: "Shift+Left"
        enabled: xdrClient.ready && !xdrClient.seeking
        onActivated: xdrClient.stepLarge(-1)
    }
    Shortcut {
        sequence: "Shift+Right"
        enabled: xdrClient.ready && !xdrClient.seeking
        onActivated: xdrClient.stepLarge(1)
    }
    Shortcut {
        sequence: "Escape"
        enabled: xdrClient.seeking
        onActivated: xdrClient.stopSeek()
    }

    background: Rectangle {
        color: "#2d2018"
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: window.woodDark }
            GradientStop { position: 0.12; color: window.woodLight }
            GradientStop { position: 0.5; color: "#5e3b28" }
            GradientStop { position: 0.88; color: window.woodLight }
            GradientStop { position: 1.0; color: window.woodDark }
        }
    }

    Drawer {
        id: settingsDrawer
        edge: Qt.RightEdge
        width: false
               ? window.width
               : Math.min(window.width * 0.44, 520)
        height: window.height
        modal: true

        onOpened: window.refreshUsbPorts()

        background: Rectangle {
            color: "#deded8"
            border.color: "#777770"
        }

        ScrollView {
            anchors.fill: parent
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: window.sectionSpacing

                Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: false ? 14 : 20
                    Layout.rightMargin: false ? 14 : 20
                    Layout.topMargin: false ? 14 : 18
                    text: "XDR · EINSTELLUNGEN"
                    color: window.ink
                    font.pixelSize: false ? 19 : 22
                    font.bold: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    height: 1
                    color: "#888881"
                }

                GridLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    columns: false ? 1 : 2
                    columnSpacing: 12
                    rowSpacing: false ? 6 : 10

                    Label { text: "Verbindung"; color: window.ink }
                    ComboBox {
                        id: connectionTypeBox
                        Layout.fillWidth: true
                        model: Qt.platform.os === "android"
                               ? ["TCP"]
                               : ["TCP", "USB"]
                        enabled: !xdrClient.connected
                    }

                    Label {
                        text: "IP-Adresse"
                        color: window.ink
                        enabled: connectionTypeBox.currentIndex === 0
                    }
                    TextField {
                        id: hostField
                        Layout.fillWidth: true
                        enabled: connectionTypeBox.currentIndex === 0
                        text: "10.193.149.131"
                        placeholderText: "IP-Adresse"
                        inputMethodHints: Qt.ImhUrlCharactersOnly
                    }

                    Label { text: "TCP-Port"; color: window.ink }
                    TextField {
                        id: portField
                        Layout.fillWidth: true
                        enabled: connectionTypeBox.currentIndex === 0
                        text: "7373"
                        placeholderText: "Port"
                        inputMethodHints: Qt.ImhDigitsOnly
                    }

                    Label { text: "Passwort"; color: window.ink }
                    TextField {
                        id: passwordField
                        Layout.fillWidth: true
                        enabled: connectionTypeBox.currentIndex === 0
                        text: ""
                        placeholderText: "leer möglich"
                        echoMode: TextInput.Password
                        passwordCharacter: "*"
                    }

                    Label {
                        text: "USB-Anschluss"
                        color: window.ink
                        visible: Qt.platform.os !== "android"
                        enabled: visible
                                 && connectionTypeBox.currentIndex === 1
                    }
                    ComboBox {
                        id: usbPortBox
                        Layout.fillWidth: true
                        visible: Qt.platform.os !== "android"
                        enabled: visible
                                 && connectionTypeBox.currentIndex === 1
                                 && count > 0
                        model: []

                        displayText: count > 0
                                     ? currentText
                                     : "Kein serieller Port gefunden"

                        onActivated: {
                            appSettings.usbPort = currentText
                        }
                    }

                    Label {
                        text: "USB-Baudrate"
                        color: window.ink
                        visible: Qt.platform.os !== "android"
                        enabled: visible
                                 && connectionTypeBox.currentIndex === 1
                    }
                    TextField {
                        id: usbBaudField
                        Layout.fillWidth: true
                        visible: Qt.platform.os !== "android"
                        text: "115200"
                        inputMethodHints: Qt.ImhDigitsOnly
                        enabled: visible
                                 && connectionTypeBox.currentIndex === 1
                    }
                }

                VintageButton {
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: false ? 210 : 230
                    text: xdrClient.connected ? "VERBINDUNG TRENNEN" : "VERBINDEN"
                    onClicked: window.connectOrDisconnect()
                }

                GroupBox {
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    title: "Empfang"

                    GridLayout {
                        anchors.fill: parent
                        columns: false ? 1 : 2
                        columnSpacing: 12
                        rowSpacing: false ? 6 : 10

                        Label { text: "Betriebsart" }
                        Switch {
                            enabled: xdrClient.ready && !xdrClient.seeking
                            text: checked ? "Mono erzwungen" : "Stereo-Automatik"
                            checked: xdrClient.forcedMono
                            onToggled: {
                                if (checked !== xdrClient.forcedMono)
                                    xdrClient.setForcedMono(checked)
                            }
                        }

                        Label { text: "Bandbreite" }
                        ComboBox {
                            Layout.fillWidth: true
                            enabled: xdrClient.ready && !xdrClient.seeking
                            model: ["Auto", "56 kHz", "64 kHz", "72 kHz", "84 kHz",
                                    "97 kHz", "114 kHz", "133 kHz", "151 kHz",
                                    "168 kHz", "184 kHz", "200 kHz", "217 kHz",
                                    "236 kHz", "254 kHz", "287 kHz", "311 kHz"]
                            currentIndex: window.valueIndex(window.bandwidthValues,
                                                            xdrClient.bandwidthSettingHz)
                            onActivated: xdrClient.setBandwidth(
                                             window.bandwidthValues[currentIndex])
                        }

                        Label { text: "De-Emphasis" }
                        ComboBox {
                            Layout.fillWidth: true
                            enabled: xdrClient.ready
                            model: ["50 µs", "75 µs", "0 µs"]
                            currentIndex: xdrClient.deemphasis
                            onActivated: xdrClient.setDeemphasis(currentIndex)
                        }

                        Label { text: "AGC-Schwelle" }
                        ComboBox {
                            Layout.fillWidth: true
                            enabled: xdrClient.ready
                            model: ["Höchste", "Hoch", "Mittel", "Niedrig"]
                            currentIndex: xdrClient.agc
                            onActivated: xdrClient.setAgc(currentIndex)
                        }

                        Label { text: "Signalverarbeitung" }
                        ColumnLayout {
                            spacing: 2

                            CheckBox {
                                enabled: xdrClient.ready
                                text: "Channel Equalizer"
                                checked: xdrClient.channelEqualizer
                                onToggled: {
                                    if (checked !==
                                        xdrClient.channelEqualizer) {
                                        xdrClient.setChannelEqualizer(
                                            checked)
                                    }
                                }
                            }

                            CheckBox {
                                enabled: xdrClient.ready
                                text: "iMS / Multipath"
                                checked:
                                    xdrClient.multipathSuppression
                                onToggled: {
                                    if (checked !==
                                        xdrClient.multipathSuppression) {
                                        xdrClient
                                            .setMultipathSuppression(
                                                checked)
                                    }
                                }
                            }
                        }

                        Label {
                            text: "RDS-Fehlerkorrektur"
                        }

                        Switch {
                            id: rdsErrorCorrectionSwitch

                            text: checked
                                  ? "Fehlerstatus 0,1,2"
                                  : "Fehlerstatus nur 0"

                            checked:
                                xdrClient.rdsErrorCorrectionEnabled

                            onToggled: {
                                appSettings.rdsErrorCorrectionEnabled =
                                    checked

                                if (checked !==
                                    xdrClient.rdsErrorCorrectionEnabled) {

                                    xdrClient
                                        .setRdsErrorCorrectionEnabled(
                                            checked)
                                }
                            }
                        }

                    }
                }

                GroupBox {
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    title: "Abstimmung"

                    GridLayout {
                        anchors.fill: parent
                        columns: false ? 1 : 2
                        columnSpacing: 12
                        rowSpacing: false ? 6 : 10

                        Label { text: "Kleiner Schritt" }
                        ComboBox {
                            Layout.fillWidth: true
                            enabled: !xdrClient.seeking
                            model: ["50 kHz", "100 kHz", "200 kHz"]
                            currentIndex: window.valueIndex(window.smallStepValues,
                                                            xdrClient.smallStepKhz)
                            onActivated: xdrClient.setSmallStepKhz(
                                             window.smallStepValues[currentIndex])
                        }

                        Label { text: "Großer Schritt" }
                        ComboBox {
                            Layout.fillWidth: true
                            enabled: !xdrClient.seeking
                            model: ["500 kHz", "1 MHz", "2 MHz"]
                            currentIndex: window.valueIndex(window.largeStepValues,
                                                            xdrClient.largeStepKhz)
                            onActivated: xdrClient.setLargeStepKhz(
                                             window.largeStepValues[currentIndex])
                        }

                        Label { text: "Suchempfindlichkeit" }
                        SpinBox {
                            from: 1
                            to: 30
                            value: xdrClient.seekThreshold
                            editable: true
                            enabled: !xdrClient.seeking
                            onValueModified: xdrClient.setSeekThreshold(value)
                        }

                        Label {
                            text: "1 = streng · 30 = empfindlich"
                            opacity: 0.7
                        }
                    }
                }

                GroupBox {
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    title: "Senderspeicher"

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: false ? 5 : 7

                        Repeater {
                            model: 6

                            delegate: RowLayout {
                                required property int index
                                Layout.fillWidth: true
                                spacing: 7

                                Label {
                                    Layout.preferredWidth: 28
                                    text: "M" + (index + 1)
                                    color: window.ink
                                    font.bold: true
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: window.presetFrequencyText(index)
                                    color: window.presetFrequency(index) > 0
                                           ? window.ink
                                           : window.mutedInk
                                    font.family: "monospace"
                                    elide: Text.ElideRight
                                }

                                Button {
                                    text: "Speichern"
                                    enabled: xdrClient.ready
                                             && !xdrClient.seeking
                                    onClicked:
                                        window.storePreset(index)
                                }

                                Button {
                                    text: "Löschen"
                                    enabled:
                                        window.presetFrequency(index) > 0
                                    onClicked:
                                        window.clearPreset(index)
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: "Kurzen Zeiger in der Skala antippen, "
                                  + "um den Sender aufzurufen."
                            color: window.mutedInk
                            font.pixelSize: window.smallFontSize
                            wrapMode: Text.WordWrap
                        }
                    }
                }



                Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    text: "Letzte Antwort: " + (xdrClient.lastLine || "–")
                    color: window.mutedInk
                    wrapMode: Text.WrapAnywhere
                    font.pixelSize: 12
                }

                Item { Layout.preferredHeight: 20 }
            }
        }
    }

    ScrollView {
        id: mainScroll

        anchors.fill: parent
        anchors.margins: window.outerMargin

        contentWidth: availableWidth
        contentHeight: frontPanel.height
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        Rectangle {
            id: frontPanel

            width: mainScroll.availableWidth
            implicitHeight: faceColumn.implicitHeight + 2 * window.panelMargin
            // PC bleibt wie bisher auf Fensterhöhe.
            // Android endet direkt nach dem tatsächlichen Inhalt.
            height: Qt.platform.os === "android"
                    ? implicitHeight
                    : Math.max(
                          implicitHeight,
                          window.height - 2 * window.outerMargin
                      )
            radius: 3
            border.width: 1
            border.color: "#70706a"
            gradient: Gradient {
                GradientStop { position: 0.0; color: window.aluminiumLight }
                GradientStop { position: 0.48; color: window.aluminiumMid }
                GradientStop { position: 1.0; color: window.aluminiumDark }
            }

            ColumnLayout {
                id: faceColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: window.panelMargin
                spacing: window.sectionSpacing

                // Auf Tablet und Desktop bleibt die Kopfzeile in einer Zeile.
                GridLayout {
                    Layout.fillWidth: true
                    columns: false ? 2 : 4
                    columnSpacing: false ? 8 : 10
                    rowSpacing: 8

                    RowLayout {
                        Layout.row: 0
                        Layout.column: 0
                        Layout.columnSpan: false ? 2 : 1
                        Layout.fillWidth: true
                        spacing: 8

                        Rectangle {
                            width: 18
                            height: 18
                            radius: 9
                            color: "transparent"
                            border.color: window.ink
                            border.width: 2
                            Text {
                                anchors.centerIn: parent
                                text: "Y"
                                color: window.ink
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }

                        Label {
                            text: "XDR TABLET"
                            color: window.ink
                            font.pixelSize: false ? 15 : 16
                            font.bold: true
                            font.letterSpacing: 1.2
                        }

                        Label {
                            visible: !false
                            Layout.fillWidth: true
                            text: "NATURAL SOUND FM STEREO TUNER"
                            color: window.mutedInk
                            font.pixelSize: 12
                            font.letterSpacing: 0.8
                            elide: Text.ElideRight
                        }
                    }

                    Item {
                        visible: !false
                        Layout.row: 0
                        Layout.column: 1
                        Layout.fillWidth: true
                    }

                    VintageButton {
                        id: statusButton
                        Layout.row: false ? 1 : 0
                        Layout.column: false ? 0 : 2
                        Layout.fillWidth: false
                        Layout.minimumWidth: false ? 118 : window.headerControlWidth
                        Layout.preferredWidth: window.headerControlWidth
                        Layout.minimumHeight: window.headerControlHeight
                        Layout.preferredHeight: window.headerControlHeight
                        Layout.maximumHeight: window.headerControlHeight
                        enabled: false

                        contentItem: Text {
                            text: !window.powerEnabled ? "○ AUS"
                                  : xdrClient.ready ? "● ONLINE"
                                  : xdrClient.connected ? "◐ VERBINDUNG"
                                  : "○ OFFLINE"
                            color: !window.powerEnabled ? "#666660"
                                   : xdrClient.ready ? "#315b37"
                                   : xdrClient.connected ? "#775a20"
                                   : "#6c3434"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: window.normalFontSize
                            font.bold: true
                            fontSizeMode: Text.Fit
                            minimumPixelSize: 9
                            wrapMode: Text.NoWrap
                            maximumLineCount: 1
                            clip: true
                        }
                    }

                    VintageButton {
                        Layout.row: false ? 1 : 0
                        Layout.column: false ? 1 : 3
                        Layout.fillWidth: false
                        Layout.minimumWidth: false ? 118 : window.headerControlWidth
                        Layout.preferredWidth: false ? 150 : window.headerControlWidth
                        Layout.minimumHeight: window.headerControlHeight
                        Layout.preferredHeight: window.headerControlHeight
                        Layout.maximumHeight: window.headerControlHeight
                        text: "EINSTELLUNGEN"
                        onClicked: settingsDrawer.open()
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: window.scaleHeight
                    Layout.minimumHeight: 150
                    color: "#22231f"
                    border.color: "#5b5b55"
                    border.width: false ? 3 : 5

                    // Ein gemeinsamer Skalenrahmen. Die LED-Gruppe liegt
                    // auf Desktop und Android-Tablet rechts innerhalb des Rahmens.
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: false ? 7 : 12
                        color: window.scaleGlass
                        border.color: "#77776f"

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: false ? 2 : 5
                            spacing: 2

                            FrequencyScale {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.minimumWidth: 0
                                Layout.leftMargin: 10
                                frequencyKhz: xdrClient.frequencyKhz
                                minimumKhz: xdrClient.minimumFmFrequencyKhz
                                maximumKhz: xdrClient.maximumFmFrequencyKhz
                                scaleColor: window.scaleGlass
                                textColor: window.ink
                                pointerColor: window.amber
                                presetFrequenciesKhz:
                                    window.stationPresetFrequencies
                                presetPointerColor: "#725224"
                                activePresetPointerColor: window.amber

                                onPresetActivated:
                                    function(index, frequencyKhz) {
                                        window.recallPreset(index)
                                    }
                            }

                            Item {
                                id: scaleLedArea
                                visible: true
                                Layout.preferredWidth: 72
                                Layout.minimumWidth: 72
                                Layout.maximumWidth: 72
                                Layout.fillHeight: true

                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    anchors.topMargin: 14
                                    anchors.bottomMargin: 14
                                    width: 1
                                    color: "#8a8a82"
                                }

                                ColumnLayout {
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.right: parent.right
                                    anchors.rightMargin: 0
                                    width: 48
                                    spacing: 10

                                    ColumnLayout {
                                        spacing: 3
                                        Layout.fillWidth: true

                                        Rectangle {
                                            Layout.alignment: Qt.AlignHCenter
                                            width: 13
                                            height: 13
                                            radius: 6.5
                                            antialiasing: true
                                            color:
                                                (xdrClient.rtPlusItemRunningKnown
                                                 && xdrClient.rtPlusItemRunning)
                                                ? "#3a78d6"
                                                : (xdrClient.rdsActive
                                                   ? "#d7b45d"
                                                   : "#4a4034")
                                            border.width: 1
                                            border.color:
                                                (xdrClient.rtPlusItemRunningKnown
                                                 && xdrClient.rtPlusItemRunning)
                                                ? "#234c89"
                                                : (xdrClient.rdsActive
                                                   ? "#8f6f22"
                                                   : "#77716b")

                                            MouseArea {
                                                id: rdsLedButton
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    rdsInfoWindow.visible = true
                                                    rdsInfoWindow.raise()
                                                    rdsInfoWindow.requestActivate()
                                                }
                                            }
                                        }

                                        Label {
                                            text: "RDS"
                                            color: window.ink
                                            font.pixelSize: 9
                                            font.bold: true
                                            Layout.fillWidth: true
                                            horizontalAlignment: Text.AlignHCenter
                                        }
                                    }

                                    ColumnLayout {
                                        spacing: 3
                                        Layout.fillWidth: true

                                        Rectangle {
                                            Layout.alignment: Qt.AlignHCenter
                                            width: 13
                                            height: 13
                                            radius: 6.5
                                            antialiasing: true
                                            color: xdrClient.stereo
                                                   ? "#c43a32" : "#51483f"
                                            border.width: 1
                                            border.color: xdrClient.stereo
                                                          ? "#7e241f" : "#77716b"

                                            MouseArea {
                                                id: stereoLedButton
                                                anchors.fill: parent
                                                enabled: xdrClient.ready && !xdrClient.seeking
                                                cursorShape: enabled
                                                             ? Qt.PointingHandCursor
                                                             : Qt.ArrowCursor
                                                onClicked:
                                                    xdrClient.setForcedMono(
                                                        !xdrClient.forcedMono)
                                            }
                                        }

                                        Label {
                                            text: xdrClient.forcedMono ? "MONO" : "STEREO"
                                            color: window.ink
                                            font.pixelSize: 9
                                            font.bold: true
                                            Layout.fillWidth: true
                                            horizontalAlignment: Text.AlignHCenter
                                        }
                                    }

                                    ColumnLayout {
                                        spacing: 3
                                        Layout.fillWidth: true

                                        Rectangle {
                                            Layout.alignment: Qt.AlignHCenter
                                            width: 13
                                            height: 13
                                            radius: 6.5
                                            antialiasing: true
                                            color: xdrClient.tmcActive ? "#3f9b55" : "#51483f"
                                            border.width: 1
                                            border.color: xdrClient.tmcActive ? "#286837" : "#77716b"

                                            MouseArea {
                                                id: thirdLedButton
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    tmcWindow.visible = true
                                                    tmcWindow.raise()
                                                    tmcWindow.requestActivate()
                                                }
                                            }
                                        }

                                        Label {
                                            text: "TMC"
                                            color: xdrClient.tmcActive ? window.ink : window.mutedInk
                                            font.pixelSize: 9
                                            font.bold: true
                                            Layout.fillWidth: true
                                            horizontalAlignment: Text.AlignHCenter
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Echte responsive Anordnung:
                // Desktop: POWER | SIGNAL | QUALITY | TUNING | DREHKNOPF
                // Tablet: 2 Spalten, TUNING anschließend über volle Breite.
                GridLayout {
                    id: controlsGrid
                    Layout.fillWidth: true
                    columns: window.wideLayout ? 5 : 2
                    columnSpacing: window.controlSpacing
                    rowSpacing: window.controlSpacing

                    ColumnLayout {
                        Layout.row: 0
                        Layout.column: 0
                        Layout.fillWidth: !window.wideLayout
                        Layout.preferredWidth: window.wideLayout ? 145 : -1
                        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
                        spacing: 8

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: "POWER"
                            color: window.ink
                            font.pixelSize: window.smallFontSize
                            font.bold: true
                        }

                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            width: false ? 58 : 64
                            height: false ? 84 : 92
                            color: "#b8b8b1"
                            border.color: "#777770"

                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                y: window.powerEnabled
                                   ? (false ? 11 : 13)
                                   : (false ? 42 : 46)
                                width: 24
                                height: 34
                                radius: 2
                                color: "#30302d"
                                border.color: "#11110f"
                                Behavior on y { NumberAnimation { duration: 120 } }
                            }

                            MouseArea {
                                anchors.fill: parent
                                pressAndHoldInterval: 1200
                                property bool held: false

                                onPressed: held = false
                                onPressAndHold: {
                                    held = true
                                    window.powerOffAndQuit()
                                }
                                onClicked: {
                                    if (!held)
                                        window.connectOrDisconnect()
                                }
                            }
                        }

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: window.powerEnabled ? "ON" : "OFF"
                            color: window.ink
                            font.pixelSize: 12
                            font.bold: true
                        }

                        VintageButton {
                            Layout.alignment: Qt.AlignHCenter
                            implicitWidth: false ? 118 : 130
                            text: xdrClient.forcedMono ? "MONO" : "AUTO"
                            opacity: 0
                            enabled: false
                        }
                    }

                    ColumnLayout {
                        Layout.row: window.wideLayout ? 0 : 1
                        Layout.column: window.wideLayout ? 1 : 0
                        Layout.fillWidth: !window.wideLayout
                        Layout.minimumWidth: window.wideLayout ? window.meterWidth : 0
                        Layout.preferredWidth: window.wideLayout ? window.meterWidth : -1
                        Layout.maximumWidth: window.wideLayout
                                             ? window.meterWidth
                                             : Number.POSITIVE_INFINITY
                        Layout.alignment: Qt.AlignTop
                        spacing: 4

                        VintageMeter {
                            Layout.fillWidth: true
                            Layout.preferredHeight: window.meterHeight
                            Layout.maximumHeight: window.meterHeight
                            title: ""
                            value: xdrClient.signalAvailable
                                   ? xdrClient.signalLevel : 0
                            minimumValue: 0
                            maximumValue: 80
                            valueText: xdrClient.signalAvailable
                                       ? xdrClient.signalLevel.toFixed(2) : "–"
                            needleColor: window.amber
                        }

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: "SIGNAL"
                            color: window.ink
                            font.pixelSize: window.normalFontSize
                            font.bold: true
                            font.letterSpacing: 1.0
                        }
                    }

                    ColumnLayout {
                        Layout.row: window.wideLayout ? 0 : 1
                        Layout.column: window.wideLayout ? 2 : 1
                        Layout.fillWidth: !window.wideLayout
                        Layout.minimumWidth: window.wideLayout ? window.meterWidth : 0
                        Layout.preferredWidth: window.wideLayout ? window.meterWidth : -1
                        Layout.maximumWidth: window.wideLayout
                                             ? window.meterWidth
                                             : Number.POSITIVE_INFINITY
                        Layout.alignment: Qt.AlignTop
                        spacing: 4

                        VintageMeter {
                            Layout.fillWidth: true
                            Layout.preferredHeight: window.meterHeight
                            Layout.maximumHeight: window.meterHeight
                            title: ""
                            value: window.qualityValue()
                            minimumValue: 0
                            maximumValue: 100
                            valueText: xdrClient.signalAvailable
                                       ? window.qualityValue() + " %"
                                       : "–"
                        }

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: "FM QUALITY"
                            color: window.ink
                            font.pixelSize: window.normalFontSize
                            font.bold: true
                            font.letterSpacing: 1.0
                        }
                    }

                    ColumnLayout {
                        Layout.row: window.wideLayout ? 0 : 2
                        Layout.column: window.wideLayout ? 3 : 0
                        Layout.columnSpan: window.wideLayout ? 1 : 2
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        spacing: 9

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: "TUNING"
                            color: window.ink
                            font.pixelSize: window.normalFontSize
                            font.bold: true
                            font.letterSpacing: 1.0
                        }

                        GridLayout {
                            Layout.fillWidth: !window.wideLayout
                            Layout.preferredWidth: window.wideLayout ? 560 : -1
                            Layout.maximumWidth: window.wideLayout
                                                 ? 560
                                                 : Number.POSITIVE_INFINITY
                            Layout.alignment: Qt.AlignHCenter
                            columns: 4
                            columnSpacing: 8
                            rowSpacing: 8

                            VintageButton {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                text: "− " + (xdrClient.largeStepKhz / 1000) + " MHz"
                                enabled: xdrClient.ready && !xdrClient.seeking
                                onClicked: xdrClient.stepLarge(-1)
                            }
                            VintageButton {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                text: "− " + xdrClient.smallStepKhz + " kHz"
                                enabled: xdrClient.ready && !xdrClient.seeking
                                onClicked: xdrClient.stepSmall(-1)
                            }
                            VintageButton {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                text: "+ " + xdrClient.smallStepKhz + " kHz"
                                enabled: xdrClient.ready && !xdrClient.seeking
                                onClicked: xdrClient.stepSmall(1)
                            }
                            VintageButton {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                text: "+ " + (xdrClient.largeStepKhz / 1000) + " MHz"
                                enabled: xdrClient.ready && !xdrClient.seeking
                                onClicked: xdrClient.stepLarge(1)
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: !window.wideLayout
                            Layout.preferredWidth: window.wideLayout ? 560 : -1
                            Layout.maximumWidth: window.wideLayout
                                                 ? 560
                                                 : Number.POSITIVE_INFINITY
                            Layout.alignment: Qt.AlignHCenter
                            columns: 3
                            columnSpacing: 8

                            VintageButton {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                text: "SEEK ◀"
                                enabled: xdrClient.ready && !xdrClient.seeking
                                onClicked: xdrClient.startSeek(-1)
                            }
                            VintageButton {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                text: "■ STOP"
                                enabled: xdrClient.seeking
                                onClicked: xdrClient.stopSeek()
                            }
                            VintageButton {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                text: "SEEK ▶"
                                enabled: xdrClient.ready && !xdrClient.seeking
                                onClicked: xdrClient.startSeek(1)
                            }
                        }

                        Label {
                            // Statuszeile bleibt als unsichtbarer Layout-Platzhalter
                            // erhalten, damit TUNING und Drehknopf nicht verrutschen.
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            text: xdrClient.seeking
                                  ? (xdrClient.seekDirection > 0
                                     ? "SUCHLAUF AUFWÄRTS"
                                     : "SUCHLAUF ABWÄRTS")
                                  : xdrClient.receptionModeText
                            color: xdrClient.seeking ? "#7a5015" : window.mutedInk
                            font.pixelSize: window.smallFontSize
                            font.bold: true
                            elide: Text.ElideRight
                            opacity: 0
                        }
                    }

                    // Auf breiten Displays bleiben es zusammen mit dem
                    // Panelrand genau 35 Pixel bis zum rechten Aluminiumrand.
                    ColumnLayout {
                        Layout.row: 0
                        Layout.column: window.wideLayout ? 4 : 1
                        Layout.fillWidth: !window.wideLayout
                        Layout.preferredWidth: window.wideLayout ? 170 : -1
                        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
                        Layout.rightMargin: window.wideLayout ? 13 : 0
                        spacing: 6

                        Label {
                            Layout.preferredWidth: window.tuningKnobSize
                            Layout.alignment: Qt.AlignHCenter
                            horizontalAlignment: Text.AlignHCenter
                            text: (xdrClient.frequencyKhz / 1000).toFixed(3) + " MHz"
                            color: window.ink
                            font.pixelSize: window.smallFontSize
                            font.bold: true
                        }

                        TuningKnob {
                            Layout.preferredWidth: window.tuningKnobSize
                            Layout.preferredHeight: window.tuningKnobSize
                            Layout.minimumWidth: window.tuningKnobSize
                            Layout.minimumHeight: window.tuningKnobSize
                            Layout.alignment: Qt.AlignHCenter
                            enabled: xdrClient.ready && !xdrClient.seeking

                            onStepRequested: function(direction) {
                                xdrClient.stepSmall(direction)
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignHCenter
                            horizontalAlignment: Text.AlignHCenter
                            text: Qt.platform.os === "android"
                                  ? "ziehen · tippen"
                                  : "ziehen · klicken · Mausrad"
                            color: window.mutedInk
                            font.pixelSize: window.smallFontSize
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: rdsLayout.implicitHeight
                                            + 2 * (false ? 10 : 14)
                    Layout.minimumHeight: false ? 176 : 142
                    color: "#171915"
                    border.color: "#55564f"
                    border.width: 3

                    GridLayout {
                        id: rdsLayout
                        anchors.fill: parent
                        anchors.margins: false ? 10 : 14
                        columns: false ? 1 : 3
                        columnSpacing: 18
                        rowSpacing: 10

                        // Der Informationsblock belegt auf breiten
                        // Displays immer das rechte Drittel.
                        property real rightThirdWidth:
                            false
                            ? width
                            : Math.max(
                                  0,
                                  (width - 2 * columnSpacing) / 3)

                        ColumnLayout {
                            Layout.row: 0
                            Layout.column: 0
                            Layout.fillWidth: true
                            spacing: 5

                            RowLayout {
                                Layout.fillWidth: true

                                Label {
                                    Layout.fillWidth: true
                                    text: xdrClient.psText.length > 0
                                          ? xdrClient.psText
                                          : "RDS / PROGRAM SERVICE"
                                    color: xdrClient.rdsActive
                                           ? "#e6dba8" : "#77766b"
                                    font.family: "monospace"
                                    font.pixelSize: window.largeFontSize
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                RowLayout {
                                    // START/STOP und RDS-Status unten werden nicht mehr
                                    // angezeigt; der Platz bleibt für unverändertes Layout.
                                    spacing: 8
                                    opacity: 0

                                    Label {
                                        visible:
                                            xdrClient.rtPlusItemRunningKnown

                                        text:
                                            xdrClient.rtPlusItemRunning
                                            ? "▶ START"
                                            : "■ STOP"

                                        color:
                                            xdrClient.rtPlusItemRunning
                                            ? "#e6dba8"
                                            : "#85857b"

                                        font.family: "monospace"
                                        font.pixelSize:
                                            window.smallFontSize
                                        font.bold: true
                                    }

                                    Label {
                                        text:
                                            xdrClient.rdsActive
                                            ? "● RDS"
                                            : "○ RDS"

                                        color:
                                            (xdrClient.rtPlusItemRunningKnown
                                             && xdrClient.rtPlusItemRunning)
                                            ? "#e53935"
                                            : (xdrClient.rdsActive
                                               ? "#d7b45d"
                                               : "#65655f")

                                        font.pixelSize:
                                            window.normalFontSize
                                        font.bold: true
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight:
                                    false ? 76 : 78
                                spacing: 1

                                property bool hasRtPlus:
                                    xdrClient.rtPlusTitle.length > 0
                                    || xdrClient.rtPlusArtist.length > 0

                                Label {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight:
                                        parent.hasRtPlus
                                        ? 20
                                        : (false ? 68 : 72)

                                    text: xdrClient.radioText.length > 0
                                          ? xdrClient.radioText
                                          : "Radiotext wird empfangen …"

                                    color:
                                        xdrClient.radioText.length > 0
                                        ? "#d2d0bd"
                                        : "#67675f"

                                    font.family: "monospace"
                                    font.pixelSize:
                                        parent.hasRtPlus
                                        ? (false ? 11 : 13)
                                        : (false ? 13 : 16)

                                    wrapMode:
                                        parent.hasRtPlus
                                        ? Text.NoWrap
                                        : Text.WordWrap

                                    elide:
                                        parent.hasRtPlus
                                        ? Text.ElideRight
                                        : Text.ElideNone

                                    verticalAlignment:
                                        Text.AlignVCenter
                                }

                                Label {
                                    Layout.fillWidth: true
                                    visible:
                                        xdrClient.rtPlusTitle.length > 0

                                    text:
                                        "♪ " + xdrClient.rtPlusTitle

                                    color: "#e6dba8"
                                    font.family: "monospace"
                                    font.pixelSize:
                                        false ? 13 : 16
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                Label {
                                    Layout.fillWidth: true
                                    visible:
                                        xdrClient.rtPlusArtist.length > 0

                                    text:
                                        xdrClient.rtPlusArtist

                                    color: "#b9b7a6"
                                    font.family: "monospace"
                                    font.pixelSize:
                                        false ? 12 : 14
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        Rectangle {
                            Layout.row: false ? 1 : 0
                            Layout.column: false ? 0 : 1
                            Layout.fillWidth: false
                            Layout.fillHeight: !false
                            Layout.preferredWidth: false ? -1 : 1
                            Layout.preferredHeight: false ? 1 : -1
                            color: "#50514a"
                        }

                        GridLayout {
                            Layout.row: false ? 2 : 0
                            Layout.column: false ? 0 : 2
                            Layout.fillWidth: false
                            Layout.preferredWidth: false
                                                   ? -1
                                                   : rdsLayout.rightThirdWidth
                            Layout.minimumWidth: false
                                                 ? 0
                                                 : rdsLayout.rightThirdWidth
                            Layout.maximumWidth: false
                                                 ? Number.POSITIVE_INFINITY
                                                 : rdsLayout.rightThirdWidth
                            Layout.alignment: false
                                              ? Qt.AlignLeft
                                              : Qt.AlignRight
                            columns: 2
                            columnSpacing: 10
                            rowSpacing: 7

                            Label { text: "PI"; color: "#85857b" }
                            Label {
                                Layout.fillWidth: true
                                text: xdrClient.piCode
                                color: "#ded8b5"
                                font.family: "monospace"
                                font.bold: true
                            }

                            Label { text: "ECC"; color: "#85857b" }
                            Label {
                                Layout.fillWidth: true
                                text: xdrClient.eccCode !== "--"
                                      ? xdrClient.eccCode
                                      : "–"
                                color: "#ded8b5"
                                font.family: "monospace"
                                font.bold: true
                            }

                            Label { text: "PIN"; color: "#85857b" }
                            Label {
                                Layout.fillWidth: true
                                text: xdrClient.pinText.length > 0
                                      ? xdrClient.pinText
                                      : "–"
                                color: "#ded8b5"
                                font.family: "monospace"
                                font.bold: true
                            }

                            Label { text: "PTY"; color: "#85857b" }
                            Label {
                                Layout.fillWidth: true
                                text: xdrClient.ptyCode >= 0
                                      ? xdrClient.ptyText : "–"
                                color: "#ded8b5"
                                elide: Text.ElideRight
                            }
                            Label { text: "BW"; color: "#85857b" }
                            Label {
                                Layout.fillWidth: true
                                text: window.bandwidthLabel(xdrClient.bandwidthHz)
                                color: "#ded8b5"
                            }
                            Label { text: "RDS"; color: "#85857b" }
                            Label {
                                Layout.fillWidth: true
                                text: xdrClient.rdsGroupCount + " Gruppen"
                                color: "#ded8b5"
                            }

                            Label { text: "CT"; color: "#85857b" }
                            Label {
                                Layout.fillWidth: true
                                text: xdrClient.ctText.length > 0
                                      ? xdrClient.ctText
                                      : "–"
                                color: "#ded8b5"
                                font.family: "monospace"
                                font.pixelSize: window.smallFontSize
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    Layout.bottomMargin: 4
                    columns: false ? 1 : 2
                    columnSpacing: 12
                    rowSpacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: xdrClient.statusText
                        color: window.ink
                        font.pixelSize: window.smallFontSize
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: false
                        horizontalAlignment: false
                                             ? Text.AlignLeft
                                             : Text.AlignRight
                        text: "FM 87,5–108 MHz  ·  "
                              + (xdrClient.bandwidthSettingHz > 0
                                 ? "BW " + window.bandwidthLabel(
                                               xdrClient.bandwidthSettingHz)
                                 : "BW AUTO")
                        color: window.mutedInk
                        font.pixelSize: window.smallFontSize
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }

    // ---------------------------------------------------------
    // Rahmenloses Fenster trotzdem verschieben und skalieren
    // ---------------------------------------------------------

    // Oben anfassen und Fenster verschieben.
    MouseArea {
        visible: Qt.platform.os !== "android"
        x: 10
        y: 7
        width: parent.width - 20
        height: 25
        z: 10000
        cursorShape: Qt.SizeAllCursor

        onPressed: function(mouse) {
            window.startSystemMove()
        }
    }

    // Linke Kante
    MouseArea {
        visible: Qt.platform.os !== "android"
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 6
        z: 10001
        cursorShape: Qt.SizeHorCursor
        onPressed: window.startSystemResize(Qt.LeftEdge)
    }

    // Rechte Kante
    MouseArea {
        visible: Qt.platform.os !== "android"
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 6
        z: 10001
        cursorShape: Qt.SizeHorCursor
        onPressed: window.startSystemResize(Qt.RightEdge)
    }

    // Obere Kante
    MouseArea {
        visible: Qt.platform.os !== "android"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 6
        z: 10001
        cursorShape: Qt.SizeVerCursor
        onPressed: window.startSystemResize(Qt.TopEdge)
    }

    // Untere Kante
    MouseArea {
        visible: Qt.platform.os !== "android"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 6
        z: 10001
        cursorShape: Qt.SizeVerCursor
        onPressed: window.startSystemResize(Qt.BottomEdge)
    }

    // Linke obere Ecke
    MouseArea {
        visible: Qt.platform.os !== "android"
        anchors.left: parent.left
        anchors.top: parent.top
        width: 12
        height: 12
        z: 10002
        cursorShape: Qt.SizeFDiagCursor
        onPressed:
            window.startSystemResize(Qt.LeftEdge | Qt.TopEdge)
    }

    // Rechte obere Ecke
    MouseArea {
        visible: Qt.platform.os !== "android"
        anchors.right: parent.right
        anchors.top: parent.top
        width: 12
        height: 12
        z: 10002
        cursorShape: Qt.SizeBDiagCursor
        onPressed:
            window.startSystemResize(Qt.RightEdge | Qt.TopEdge)
    }

    // Linke untere Ecke
    MouseArea {
        visible: Qt.platform.os !== "android"
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: 12
        height: 12
        z: 10002
        cursorShape: Qt.SizeBDiagCursor
        onPressed:
            window.startSystemResize(Qt.LeftEdge | Qt.BottomEdge)
    }

    // Rechte untere Ecke
    MouseArea {
        visible: Qt.platform.os !== "android"
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: 12
        height: 12
        z: 10002
        cursorShape: Qt.SizeFDiagCursor
        onPressed:
            window.startSystemResize(Qt.RightEdge | Qt.BottomEdge)
    }


}
