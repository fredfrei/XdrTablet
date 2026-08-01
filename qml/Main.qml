import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore

ApplicationWindow {
    id: window
    width: 1180
    height: 1000
    minimumWidth: 700
    minimumHeight: 700
    visible: true
    title: "XDR Tablet"

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

    function stepLabel(khz) {
        return khz >= 1000 ? (khz / 1000) + " MHz" : khz + " kHz"
    }

    function bandwidthLabel(hz) {
        return hz > 0 ? Math.round(hz / 1000) + " kHz" : "–"
    }

    Settings {
        property alias host: hostField.text
        property alias port: portField.text
        property alias password: passwordField.text
    }

    background: Rectangle { color: "#10151c" }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: 16

            Item { Layout.preferredHeight: 6 }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 22
                Layout.rightMargin: 22
                spacing: 10

                TextField {
                    id: hostField
                    Layout.fillWidth: true
                    placeholderText: "IP-Adresse"
                    text: "10.193.149.131"
                    inputMethodHints: Qt.ImhUrlCharactersOnly
                }
                TextField {
                    id: portField
                    Layout.preferredWidth: 105
                    placeholderText: "Port"
                    text: "7373"
                    inputMethodHints: Qt.ImhDigitsOnly
                }
                TextField {
                    id: passwordField
                    Layout.preferredWidth: 170
                    placeholderText: "XDR-Passwort"
                    text: ""
                    echoMode: TextInput.Password
                    passwordCharacter: "●"
                }
                Button {
                    text: xdrClient.connected ? "Trennen" : "Verbinden"
                    onClicked: xdrClient.connected
                               ? xdrClient.disconnectFromServer()
                               : xdrClient.connectToServer(hostField.text,
                                                           Number(portField.text),
                                                           passwordField.text)
                }
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: (xdrClient.frequencyKhz / 1000).toFixed(3) + " MHz"
                font.pixelSize: Math.min(window.width * 0.078, 82)
                font.bold: true
                color: "white"
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 16

                Label {
                    text: xdrClient.ready ? "● " + xdrClient.statusText
                                          : (xdrClient.connected
                                             ? "◐ " + xdrClient.statusText
                                             : "○ " + xdrClient.statusText)
                    color: xdrClient.ready ? "#6ee7a8"
                                           : (xdrClient.connected ? "#ffd27a"
                                                                  : "#ff9d9d")
                    font.pixelSize: 18
                }

                Rectangle {
                    radius: 8
                    implicitWidth: modeLabel.implicitWidth + 24
                    implicitHeight: modeLabel.implicitHeight + 12
                    color: xdrClient.stereo && !xdrClient.forcedMono
                           ? "#185c42" : "#293642"
                    border.color: xdrClient.stereo && !xdrClient.forcedMono
                                  ? "#6ee7a8" : "#506173"
                    Label {
                        id: modeLabel
                        anchors.centerIn: parent
                        text: xdrClient.receptionModeText
                        color: "white"
                        font.bold: true
                    }
                }

                Rectangle {
                    radius: 8
                    implicitWidth: rdsStatusLabel.implicitWidth + 24
                    implicitHeight: rdsStatusLabel.implicitHeight + 12
                    color: xdrClient.rdsActive ? "#184f5c" : "#293642"
                    border.color: xdrClient.rdsActive ? "#72d7ef" : "#506173"
                    Label {
                        id: rdsStatusLabel
                        anchors.centerIn: parent
                        text: xdrClient.rdsActive ? "RDS" : "RDS –"
                        color: xdrClient.rdsActive ? "#a6ebff" : "#7f93a8"
                        font.bold: true
                    }
                }
            }

            Frame {
                Layout.fillWidth: true
                Layout.leftMargin: 22
                Layout.rightMargin: 22
                background: Rectangle {
                    radius: 12
                    color: "#17202a"
                    border.color: xdrClient.rdsActive ? "#346a76" : "#2b3a49"
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: xdrClient.psText.length > 0
                                  ? xdrClient.psText : "Kein Sendername"
                            color: "white"
                            font.pixelSize: 29
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: window.width < 780 ? 2 : 4
                        columnSpacing: 18
                        rowSpacing: 8

                        Label { text: "PI-Code"; color: "#8fa2b8" }
                        Label {
                            text: xdrClient.piCode
                            color: "#dce6f2"
                            font.pixelSize: 18
                            font.bold: true
                        }
                        Label { text: "PTY"; color: "#8fa2b8" }
                        Label {
                            text: xdrClient.ptyCode >= 0
                                  ? xdrClient.ptyText + " (" + xdrClient.ptyCode + ")"
                                  : "–"
                            color: "#dce6f2"
                            font.pixelSize: 17
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.minimumHeight: 76
                        radius: 8
                        color: "#101820"
                        border.color: "#2b3a49"
                        Label {
                            anchors.fill: parent
                            anchors.margins: 12
                            text: xdrClient.radioText.length > 0
                                  ? xdrClient.radioText : "Radiotext wird empfangen …"
                            color: xdrClient.radioText.length > 0 ? "#e5edf5" : "#667b8f"
                            font.pixelSize: 17
                            wrapMode: Text.WordWrap
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: "RDS-Gruppen: " + xdrClient.rdsGroupCount
                        color: "#667b8f"
                        font.pixelSize: 12
                    }
                }
            }

            Frame {
                Layout.fillWidth: true
                Layout.leftMargin: 22
                Layout.rightMargin: 22
                background: Rectangle {
                    radius: 12
                    color: "#17202a"
                    border.color: "#2b3a49"
                }

                GridLayout {
                    anchors.fill: parent
                    columns: window.width < 850 ? 2 : 4
                    columnSpacing: 16
                    rowSpacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        Label { text: "Signal"; color: "#8fa2b8" }
                        Label {
                            text: xdrClient.signalAvailable
                                  ? xdrClient.signalLevel.toFixed(2) : "–"
                            color: "white"
                            font.pixelSize: 26
                            font.bold: true
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Label { text: "CCI / ACI"; color: "#8fa2b8" }
                        Label {
                            text: (xdrClient.cci >= 0 ? xdrClient.cci + " %" : "–")
                                  + " / "
                                  + (xdrClient.aci >= 0 ? xdrClient.aci + " %" : "–")
                            color: "white"
                            font.pixelSize: 23
                            font.bold: true
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Label { text: "Aktuelle Bandbreite"; color: "#8fa2b8" }
                        Label {
                            text: window.bandwidthLabel(xdrClient.bandwidthHz)
                            color: "white"
                            font.pixelSize: 23
                            font.bold: true
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Label { text: "Empfang"; color: "#8fa2b8" }
                        Label {
                            text: xdrClient.receptionModeText
                            color: "white"
                            font.pixelSize: 20
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }

            Frame {
                Layout.fillWidth: true
                Layout.leftMargin: 22
                Layout.rightMargin: 22
                background: Rectangle {
                    radius: 12
                    color: "#17202a"
                    border.color: "#2b3a49"
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 14

                    Label {
                        text: "Empfangs- und DSP-Einstellungen"
                        color: "white"
                        font.pixelSize: 21
                        font.bold: true
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: window.width < 850 ? 2 : 4
                        columnSpacing: 14
                        rowSpacing: 12

                        Label { text: "Stereo / Mono"; color: "#b9c9d8" }
                        Switch {
                            id: monoSwitch
                            enabled: xdrClient.ready && !xdrClient.seeking
                            text: checked ? "Mono erzwungen" : "Stereo-Automatik"
                            checked: xdrClient.forcedMono
                            onToggled: {
                                if (checked !== xdrClient.forcedMono)
                                    xdrClient.setForcedMono(checked)
                            }
                        }

                        Label { text: "Bandbreite"; color: "#b9c9d8" }
                        ComboBox {
                            enabled: xdrClient.ready && !xdrClient.seeking
                            Layout.fillWidth: true
                            model: ["Auto", "56 kHz", "64 kHz", "72 kHz", "84 kHz",
                                    "97 kHz", "114 kHz", "133 kHz", "151 kHz",
                                    "168 kHz", "184 kHz", "200 kHz", "217 kHz",
                                    "236 kHz", "254 kHz", "287 kHz", "311 kHz"]
                            currentIndex: window.valueIndex(window.bandwidthValues,
                                                            xdrClient.bandwidthSettingHz)
                            onActivated: xdrClient.setBandwidth(
                                             window.bandwidthValues[currentIndex])
                        }

                        Label { text: "De-Emphasis"; color: "#b9c9d8" }
                        ComboBox {
                            enabled: xdrClient.ready
                            Layout.fillWidth: true
                            model: ["50 µs", "75 µs", "0 µs"]
                            currentIndex: xdrClient.deemphasis
                            onActivated: xdrClient.setDeemphasis(currentIndex)
                        }

                        Label { text: "AGC-Schwelle"; color: "#b9c9d8" }
                        ComboBox {
                            enabled: xdrClient.ready
                            Layout.fillWidth: true
                            model: ["Höchste", "Hoch", "Mittel", "Niedrig"]
                            currentIndex: xdrClient.agc
                            onActivated: xdrClient.setAgc(currentIndex)
                        }

                        Label { text: "Zusatzverstärkung"; color: "#b9c9d8" }
                        RowLayout {
                            Layout.fillWidth: true
                            CheckBox {
                                enabled: xdrClient.ready
                                text: "RF"
                                checked: xdrClient.rfGain
                                onToggled: {
                                    if (checked !== xdrClient.rfGain)
                                        xdrClient.setRfGain(checked)
                                }
                            }
                            CheckBox {
                                enabled: xdrClient.ready
                                text: "IF"
                                checked: xdrClient.ifGain
                                onToggled: {
                                    if (checked !== xdrClient.ifGain)
                                        xdrClient.setIfGain(checked)
                                }
                            }
                        }

                        Label { text: "Status"; color: "#b9c9d8" }
                        Label {
                            Layout.fillWidth: true
                            text: "Soll-BW: "
                                  + (xdrClient.bandwidthSettingHz > 0
                                     ? window.bandwidthLabel(xdrClient.bandwidthSettingHz)
                                     : "Auto")
                            color: "#8fa2b8"
                        }
                    }
                }
            }

            Frame {
                Layout.fillWidth: true
                Layout.leftMargin: 22
                Layout.rightMargin: 22
                background: Rectangle {
                    radius: 12
                    color: "#17202a"
                    border.color: xdrClient.seeking ? "#d9a441" : "#2b3a49"
                    border.width: xdrClient.seeking ? 2 : 1
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 14

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            text: "Abstimmung"
                            color: "white"
                            font.pixelSize: 21
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Label {
                            text: "FM 87,500–108,000 MHz"
                            color: "#8fa2b8"
                        }
                    }

                    GridLayout {
                        Layout.alignment: Qt.AlignHCenter
                        columns: window.width < 760 ? 2 : 4
                        columnSpacing: 12
                        rowSpacing: 12

                        Button {
                            enabled: xdrClient.ready && !xdrClient.seeking
                                     && xdrClient.frequencyKhz > xdrClient.minimumFmFrequencyKhz
                            text: "− " + window.stepLabel(xdrClient.largeStepKhz)
                            implicitWidth: 190
                            implicitHeight: 54
                            font.pixelSize: 17
                            onClicked: xdrClient.stepLarge(-1)
                        }
                        Button {
                            enabled: xdrClient.ready && !xdrClient.seeking
                                     && xdrClient.frequencyKhz > xdrClient.minimumFmFrequencyKhz
                            text: "− " + window.stepLabel(xdrClient.smallStepKhz)
                            implicitWidth: 190
                            implicitHeight: 54
                            font.pixelSize: 17
                            onClicked: xdrClient.stepSmall(-1)
                        }
                        Button {
                            enabled: xdrClient.ready && !xdrClient.seeking
                                     && xdrClient.frequencyKhz < xdrClient.maximumFmFrequencyKhz
                            text: "+ " + window.stepLabel(xdrClient.smallStepKhz)
                            implicitWidth: 190
                            implicitHeight: 54
                            font.pixelSize: 17
                            onClicked: xdrClient.stepSmall(1)
                        }
                        Button {
                            enabled: xdrClient.ready && !xdrClient.seeking
                                     && xdrClient.frequencyKhz < xdrClient.maximumFmFrequencyKhz
                            text: "+ " + window.stepLabel(xdrClient.largeStepKhz)
                            implicitWidth: 190
                            implicitHeight: 54
                            font.pixelSize: 17
                            onClicked: xdrClient.stepLarge(1)
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: window.width < 760 ? 2 : 4
                        columnSpacing: 12
                        rowSpacing: 8

                        Label { text: "Kleiner Schritt"; color: "#b9c9d8" }
                        ComboBox {
                            enabled: !xdrClient.seeking
                            Layout.fillWidth: true
                            model: ["50 kHz", "100 kHz", "200 kHz"]
                            currentIndex: window.valueIndex(window.smallStepValues,
                                                            xdrClient.smallStepKhz)
                            onActivated: xdrClient.setSmallStepKhz(
                                             window.smallStepValues[currentIndex])
                        }
                        Label { text: "Großer Schritt"; color: "#b9c9d8" }
                        ComboBox {
                            enabled: !xdrClient.seeking
                            Layout.fillWidth: true
                            model: ["500 kHz", "1 MHz", "2 MHz"]
                            currentIndex: window.valueIndex(window.largeStepValues,
                                                            xdrClient.largeStepKhz)
                            onActivated: xdrClient.setLargeStepKhz(
                                             window.largeStepValues[currentIndex])
                        }
                    }

                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 12

                        Button {
                            enabled: xdrClient.ready && !xdrClient.seeking
                                     && xdrClient.frequencyKhz > xdrClient.minimumFmFrequencyKhz
                            text: "Suchlauf ◀"
                            implicitWidth: 180
                            implicitHeight: 54
                            font.pixelSize: 17
                            onClicked: xdrClient.startSeek(-1)
                        }
                        Button {
                            enabled: xdrClient.seeking
                            text: "■ Stoppen"
                            implicitWidth: 150
                            implicitHeight: 54
                            font.pixelSize: 17
                            onClicked: xdrClient.stopSeek()
                        }
                        Button {
                            enabled: xdrClient.ready && !xdrClient.seeking
                                     && xdrClient.frequencyKhz < xdrClient.maximumFmFrequencyKhz
                            text: "Suchlauf ▶"
                            implicitWidth: 180
                            implicitHeight: 54
                            font.pixelSize: 17
                            onClicked: xdrClient.startSeek(1)
                        }
                    }

                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 10
                        Label { text: "Suchschwelle:"; color: "#b9c9d8" }
                        SpinBox {
                            from: 0
                            to: 80
                            value: xdrClient.seekThreshold
                            editable: true
                            enabled: !xdrClient.seeking
                            onValueModified: xdrClient.setSeekThreshold(value)
                        }
                        Label { text: "Signalwert"; color: "#8fa2b8" }
                        Label {
                            visible: xdrClient.seeking
                            text: xdrClient.seekDirection > 0
                                  ? "Suche aufwärts …" : "Suche abwärts …"
                            color: "#ffd27a"
                            font.bold: true
                        }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 22
                Layout.rightMargin: 22
                Layout.bottomMargin: 16
                text: "Letzte Antwort: " + (xdrClient.lastLine || "–")
                color: "#8fa2b8"
                elide: Text.ElideRight
            }
        }
    }
}
