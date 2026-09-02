import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: tmcWindow

    required property var client

    property color aluminiumLight: "#eeeeea"
    property color aluminiumMid: "#c9c9c3"
    property color ink: "#262620"
    property color mutedInk: "#686861"
    property int smallFontSize: 12
    // XDRTABLET_TMC_LED_TEST_V1
    // XDRTABLET_TMC_SINGLE_V1
    width: 700
    height: 600
    minimumWidth: 520
    minimumHeight: 420
    visible: false
    title: "TMC / ALERT-C"
    color: tmcWindow.aluminiumMid
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint
    modality: Qt.NonModal

    // XDRTABLET_TMC_CLEAN_VIEW_V1
    property bool showTechnicalDetails: false
    property bool showAllMessages: false
    property int filteredMessageLimit: 15

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

    function tmcMessageBlocks(fullText) {
        if (!fullText || fullText.length === 0)
            return []

        const sourceBlocks = fullText.split(/\n\s*\n/)
        const blocks = []

        for (let i = 0; i < sourceBlocks.length; ++i) {
            const block = sourceBlocks[i].trim()
            if (block.length > 0)
                blocks.push(block)
        }

        return blocks
    }

    function textContainsAny(text, terms) {
        const probe = text.toLowerCase()

        for (let i = 0; i < terms.length; ++i) {
            if (probe.indexOf(terms[i]) >= 0)
                return true
        }

        return false
    }

    function isImportantTmcMessage(compactText) {
        return textContainsAny(compactText, [
            "unfall",
            "stau",
            "gefahr",
            "gesperrt",
            "sperrung",
            "defekt",
            "zeitverlust",
            "verkehrsbehinderung",
            "verengt",
            "umleitung",
            "gegenstände auf der fahrbahn",
            "fahrstreifen"
        ])
    }

    function isRoutineRoadwork(compactText) {
        const isRoadwork = textContainsAny(compactText, [
            "baustelle",
            "bauarbeiten",
            "dauerbaustelle",
            "fahrbahnerneuerung",
            "brückenarbeiten",
            "wartungsarbeiten",
            "straßenarbeiten"
        ])

        return isRoadwork && !isImportantTmcMessage(compactText)
    }

    function selectedTmcBlocks(fullText) {
        const blocks = tmcMessageBlocks(fullText)

        if (tmcWindow.showAllMessages)
            return blocks

        const important = []
        const normal = []

        for (let i = 0; i < blocks.length; ++i) {
            const compact = compactTmcMessage(blocks[i])

            if (isRoutineRoadwork(compact))
                continue

            if (isImportantTmcMessage(compact))
                important.push(blocks[i])
            else
                normal.push(blocks[i])
        }

        return important.concat(normal).slice(
                    0, tmcWindow.filteredMessageLimit)
    }

    function filteredTmcText(fullText) {
        const blocks = selectedTmcBlocks(fullText)
        const result = []

        for (let i = 0; i < blocks.length; ++i) {
            const text = tmcWindow.showTechnicalDetails
                         ? blocks[i]
                         : compactTmcMessage(blocks[i])

            if (text.length > 0)
                result.push(text)
        }

        return result.join("\n\n")
    }

    function displayedTmcMessageCount(fullText) {
        return selectedTmcBlocks(fullText).length
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 12
        radius: 3
        color: tmcWindow.aluminiumLight
        border.width: 1
        border.color: "#777770"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 10

            Label {
                Layout.fillWidth: true
                text: "TMC / ALERT-C"
                color: tmcWindow.ink
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

                Label { text: "Status"; color: tmcWindow.mutedInk }
                Label {
                    Layout.fillWidth: true
                    text: tmcWindow.client.tmcActive
                          ? "TMC-Daten werden empfangen"
                          : (tmcWindow.client.tmcGroupCount > 0
                             ? "Zurzeit keine TMC-Daten empfangen"
                             : "Noch keine gültigen TMC-Daten empfangen")
                    color: tmcWindow.client.tmcActive
                           ? "#315b37"
                           : tmcWindow.ink
                    font.bold: tmcWindow.client.tmcActive
                }

                Label { text: "Sender"; color: tmcWindow.mutedInk }
                Label {
                    Layout.fillWidth: true
                    text: tmcWindow.client.psText.length > 0
                          ? tmcWindow.client.psText
                          : tmcWindow.client.piCode
                    color: tmcWindow.ink
                    font.bold: true
                }

                Label { text: "PI"; color: tmcWindow.mutedInk }
                Label {
                    Layout.fillWidth: true
                    text: tmcWindow.client.piCode
                    color: tmcWindow.ink
                    font.family: "monospace"
                }

                Label { text: "LTN / SID"; color: tmcWindow.mutedInk }
                Label {
                    Layout.fillWidth: true
                    text: (tmcWindow.client.tmcLocationTableNumber >= 0
                           ? tmcWindow.client.tmcLocationTableNumber : "–")
                          + " / "
                          + (tmcWindow.client.tmcServiceId >= 0
                             ? tmcWindow.client.tmcServiceId : "–")
                    color: tmcWindow.ink
                    font.family: "monospace"
                }

                Label { text: "gültige 8A-Gruppen"; color: tmcWindow.mutedInk }
                Label {
                    Layout.fillWidth: true
                    text: tmcWindow.client.tmcGroupCount
                    color: tmcWindow.ink
                }

                Label { text: "Single-Groups"; color: tmcWindow.mutedInk }
                Label {
                    Layout.fillWidth: true
                    text: tmcWindow.client.tmcSingleCount
                    color: tmcWindow.ink
                    font.bold: true
                }

                Label { text: "Multi komplett"; color: tmcWindow.mutedInk }
                Label {
                    Layout.fillWidth: true
                    text: tmcWindow.client.tmcMultiCount
                    color: tmcWindow.ink
                    font.bold: true
                }

                Label {
                    text: "Fragmente ohne Start"
                    color: tmcWindow.mutedInk
                    visible: tmcWindow.client.tmcMultiOrphanCount > 0
                }
                Label {
                    Layout.fillWidth: true
                    text: tmcWindow.client.tmcMultiOrphanCount
                    color: tmcWindow.ink
                    font.bold: true
                    visible: tmcWindow.client.tmcMultiOrphanCount > 0
                }

                Label { text: "Aktuelle Meldungen"; color: tmcWindow.mutedInk }
                Label {
                    Layout.fillWidth: true
                    text: tmcWindow.showAllMessages
                          ? tmcWindow.client.tmcMessageCount
                          : tmcWindow.displayedTmcMessageCount(
                                tmcWindow.client.tmcMessagesText)
                            + " von "
                            + tmcWindow.client.tmcMessageCount
                    color: tmcWindow.ink
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
                    color: tmcWindow.ink
                    font.pixelSize: 16
                    font.bold: true
                }

                CheckBox {
                    text: "Alle Meldungen"
                    checked: tmcWindow.showAllMessages
                    onToggled: tmcWindow.showAllMessages = checked
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

                    text: {
                        const fullText = tmcWindow.client.tmcMessagesText

                        if (fullText.length === 0)
                            return "Noch keine aktuelle TMC-Meldung empfangen."

                        const filtered = tmcWindow.filteredTmcText(fullText)

                        if (filtered.length > 0)
                            return filtered

                        return "Keine wichtigen Verkehrsmeldungen im Filter. "
                               + "Mit „Alle Meldungen“ kann die vollständige "
                               + "Liste eingeblendet werden."
                    }

                    color: tmcWindow.ink
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
                      + (tmcWindow.client.tmcLastRaw.length > 0
                         ? tmcWindow.client.tmcLastRaw
                         : "–")
                color: tmcWindow.mutedInk
                font.family: "monospace"
                font.pixelSize: tmcWindow.smallFontSize
                wrapMode: Text.WrapAnywhere
            }

            Label {
                Layout.fillWidth: true
                text: tmcWindow.showTechnicalDetails
                      ? "Detailansicht: vollständige ALERT-C-/LCL-Decoderinformationen. "
                        + "Storno-Meldungen entfernen die betroffene Meldung sofort; nicht mehr wiederholte "
                        + "Meldungen verschwinden nach 15 Minuten."
                      : (tmcWindow.showAllMessages
                         ? "Kompaktansicht: Alle aktuellen Meldungen werden angezeigt; "
                           + "technische Decoderwerte bleiben ausgeblendet."
                         : "Gefilterte Ansicht: höchstens 15 Meldungen, wichtige Ereignisse zuerst. "
                           + "Reine Baustellenmeldungen ohne Sperrung oder Behinderung werden ausgeblendet.")
                // XDRTABLET_TMC_ECL_LCL_V1
                // XDRTABLET_TMC_MULTI_V1
                // XDRTABLET_TMC_ACTIVE_MESSAGES_V1
                // XDRTABLET_TMC_CLEAN_VIEW_V1
                // XDRTABLET_TMC_ECL_CORRECTIONS_V1
                // XDRTABLET_TMC_DISPLAY_POLISH_V1
                // XDRTABLET_TMC_DETOUR_TEXT_V2
                // XDRTABLET_TMC_SECTION_NAMES_V1
                color: tmcWindow.mutedInk
                font.pixelSize: tmcWindow.smallFontSize
                wrapMode: Text.WordWrap
            }
        }
    }
}
