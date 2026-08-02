import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore

ApplicationWindow {
    id: window
    width: 1280
    height: 820
    minimumWidth: Qt.platform.os === "android" ? 0 : 900
    minimumHeight: Qt.platform.os === "android" ? 0 : 640
    visible: true
    title: "XDR CT-610"

    property color aluminiumLight: "#eeeeea"
    property color aluminiumMid: "#c9c9c3"
    property color aluminiumDark: "#9e9e98"
    property color ink: "#262620"
    property color mutedInk: "#686861"
    property color scaleGlass: "#d8d8cc"
    property color amber: "#b66c27"
    property color woodDark: "#4b2d1d"
    property color woodLight: "#795039"

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

    function connectOrDisconnect() {
        if (xdrClient.connected)
            xdrClient.disconnectFromServer()
        else
            xdrClient.connectToServer(hostField.text,
                                      Number(portField.text),
                                      passwordField.text)
    }

    function qualityValue() {
        if (xdrClient.cci < 0 || xdrClient.aci < 0)
            return 0
        return Math.max(0, Math.min(100, (xdrClient.cci + xdrClient.aci) / 2))
    }

    Settings {
        property alias host: hostField.text
        property alias port: portField.text
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
        width: Math.min(window.width * 0.44, 520)
        height: window.height
        modal: true

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
                spacing: 14

                Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    Layout.topMargin: 18
                    text: "XDR · EINSTELLUNGEN"
                    color: window.ink
                    font.pixelSize: 22
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
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 10

                    Label { text: "IP-Adresse"; color: window.ink }
                    TextField {
                        id: hostField
                        Layout.fillWidth: true
                        text: "10.193.149.131"
                        placeholderText: "IP-Adresse"
                        inputMethodHints: Qt.ImhUrlCharactersOnly
                    }

                    Label { text: "TCP-Port"; color: window.ink }
                    TextField {
                        id: portField
                        Layout.fillWidth: true
                        text: "7373"
                        placeholderText: "Port"
                        inputMethodHints: Qt.ImhDigitsOnly
                    }

                    Label { text: "Passwort"; color: window.ink }
                    TextField {
                        id: passwordField
                        Layout.fillWidth: true
                        text: ""
                        placeholderText: "leer möglich"
                        echoMode: TextInput.Password
                        passwordCharacter: "●"
                    }
                }

                VintageButton {
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: 230
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
                        columns: 2
                        columnSpacing: 12
                        rowSpacing: 10

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

                        Label { text: "Zusatzverstärkung" }
                        RowLayout {
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
                    }
                }

                GroupBox {
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    title: "Abstimmung"

                    GridLayout {
                        anchors.fill: parent
                        columns: 2
                        columnSpacing: 12
                        rowSpacing: 10

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

                        Label { text: "Suchschwelle" }
                        SpinBox {
                            from: 0
                            to: 80
                            value: xdrClient.seekThreshold
                            editable: true
                            enabled: !xdrClient.seeking
                            onValueModified: xdrClient.setSeekThreshold(value)
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
        anchors.margins: 18

        contentWidth: Math.max(availableWidth, 850)
        contentHeight: frontPanel.height
        clip: true

        Rectangle {
            id: frontPanel

            width: mainScroll.contentWidth
            implicitHeight: faceColumn.implicitHeight + 44
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
                anchors.margins: 22
                spacing: 14

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

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
                        font.pixelSize: 16
                        font.bold: true
                        font.letterSpacing: 1.2
                    }
                    Label {
                        text: "NATURAL SOUND FM STEREO TUNER"
                        color: window.mutedInk
                        font.pixelSize: 12
                        font.letterSpacing: 0.8
                    }

                    Item { Layout.fillWidth: true }

                    Rectangle {
                        implicitWidth: statusText.implicitWidth + 22
                        implicitHeight: 30
                        radius: 2
                        color: "#bdbdb6"
                        border.color: "#777770"

                        Label {
                            id: statusText
                            anchors.centerIn: parent
                            text: xdrClient.ready ? "● ONLINE"
                                                  : (xdrClient.connected ? "◐ VERBINDUNG" : "○ OFFLINE")
                            color: xdrClient.ready ? "#315b37"
                                                   : (xdrClient.connected ? "#775a20" : "#6c3434")
                            font.pixelSize: 12
                            font.bold: true
                        }
                    }

                    VintageButton {
                        implicitWidth: 118
                        implicitHeight: 34
                        text: "EINSTELLUNGEN"
                        onClicked: settingsDrawer.open()
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(190, Math.min(235, window.height * 0.29))
                    color: "#22231f"
                    border.color: "#5b5b55"
                    border.width: 5

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 12
                        color: window.scaleGlass
                        border.color: "#77776f"

                        FrequencyScale {
                            anchors.fill: parent
                            anchors.margins: 5
                            frequencyKhz: xdrClient.frequencyKhz
                            minimumKhz: xdrClient.minimumFmFrequencyKhz
                            maximumKhz: xdrClient.maximumFmFrequencyKhz
                            scaleColor: window.scaleGlass
                            textColor: window.ink
                            pointerColor: window.amber
                        }

                        Rectangle {
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.rightMargin: 18
                            anchors.bottomMargin: 14
                            width: 190
                            height: 38
                            color: "#c9c9bd"
                            border.color: "#85857d"
                            Label {
                                anchors.centerIn: parent
                                text: (xdrClient.frequencyKhz / 1000).toFixed(3) + " MHz"
                                color: window.ink
                                font.family: "monospace"
                                font.pixelSize: 20
                                font.bold: true
                            }
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 225

                    RowLayout {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.right: fmTuningColumn.left
                        anchors.rightMargin: 18
                        spacing: 18


                        ColumnLayout {
                        Layout.preferredWidth: 145
                        Layout.fillHeight: true
                        spacing: 10

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: "POWER"
                            color: window.ink
                            font.pixelSize: 11
                            font.bold: true
                        }

                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            width: 64
                            height: 92
                            color: "#b8b8b1"
                            border.color: "#777770"

                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                y: xdrClient.connected ? 13 : 46
                                width: 24
                                height: 34
                                radius: 2
                                color: "#30302d"
                                border.color: "#11110f"
                                Behavior on y { NumberAnimation { duration: 120 } }
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: window.connectOrDisconnect()
                            }
                        }

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: xdrClient.connected ? "ON" : "OFF"
                            color: window.ink
                            font.pixelSize: 12
                            font.bold: true
                        }

                        VintageButton {
                            Layout.alignment: Qt.AlignHCenter
                            implicitWidth: 130
                            text: xdrClient.forcedMono ? "MONO" : "AUTO"
                            enabled: xdrClient.ready && !xdrClient.seeking
                            onClicked: xdrClient.setForcedMono(!xdrClient.forcedMono)
                        }
                    }

                    ColumnLayout {
                        Layout.preferredWidth: 220
                        Layout.minimumWidth: 220
                        Layout.maximumWidth: 220
                        Layout.fillWidth: false
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 4

                        VintageMeter {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 120
                            Layout.maximumHeight: 120

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
                            font.pixelSize: 12
                            font.bold: true
                            font.letterSpacing: 1.0
                        }
                    }
ColumnLayout {
    Layout.preferredWidth: 220
    Layout.minimumWidth: 220
    Layout.maximumWidth: 220
    Layout.fillWidth: false
    Layout.alignment: Qt.AlignVCenter
    spacing: 4

    VintageMeter {
        Layout.fillWidth: true
        Layout.preferredHeight: 120
        Layout.maximumHeight: 120

        title: ""
        value: window.qualityValue()
        minimumValue: 0
        maximumValue: 100
        valueText: (xdrClient.cci >= 0 ? xdrClient.cci : "–")
                   + " / "
                   + (xdrClient.aci >= 0 ? xdrClient.aci : "–")
    }

    Label {
        Layout.alignment: Qt.AlignHCenter
        text: "FM QUALITY"
        color: window.ink
        font.pixelSize: 12
        font.bold: true
        font.letterSpacing: 1.0
    }
}

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 9

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: "TUNING"
                            color: window.ink
                            font.pixelSize: 12
                            font.bold: true
                            font.letterSpacing: 1.0
                        }

                        RowLayout {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: 8
                            VintageButton {
                                implicitWidth: 98
                                text: "− " + (xdrClient.largeStepKhz / 1000) + " MHz"
                                enabled: xdrClient.ready && !xdrClient.seeking
                                onClicked: xdrClient.stepLarge(-1)
                            }
                            VintageButton {
                                implicitWidth: 98
                                text: "− " + xdrClient.smallStepKhz + " kHz"
                                enabled: xdrClient.ready && !xdrClient.seeking
                                onClicked: xdrClient.stepSmall(-1)
                            }
                            VintageButton {
                                implicitWidth: 98
                                text: "+ " + xdrClient.smallStepKhz + " kHz"
                                enabled: xdrClient.ready && !xdrClient.seeking
                                onClicked: xdrClient.stepSmall(1)
                            }
                            VintageButton {
                                implicitWidth: 98
                                text: "+ " + (xdrClient.largeStepKhz / 1000) + " MHz"
                                enabled: xdrClient.ready && !xdrClient.seeking
                                onClicked: xdrClient.stepLarge(1)
                            }
                        }

                        RowLayout {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: 8
                            VintageButton {
                                implicitWidth: 120
                                text: "SEEK ◀"
                                enabled: xdrClient.ready && !xdrClient.seeking
                                onClicked: xdrClient.startSeek(-1)
                            }
                            VintageButton {
                                implicitWidth: 110
                                text: "■ STOP"
                                enabled: xdrClient.seeking
                                onClicked: xdrClient.stopSeek()
                            }
                            VintageButton {
                                implicitWidth: 120
                                text: "SEEK ▶"
                                enabled: xdrClient.ready && !xdrClient.seeking
                                onClicked: xdrClient.startSeek(1)
                            }
                        }

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: xdrClient.seeking
                                  ? (xdrClient.seekDirection > 0 ? "SUCHLAUF AUFWÄRTS" : "SUCHLAUF ABWÄRTS")
                                  : xdrClient.receptionModeText
                            color: xdrClient.seeking ? "#7a5015" : window.mutedInk
                            font.pixelSize: 12
                            font.bold: true
                        }
                    }
                    }

                    // Dieser Block ist unabhängig vom RowLayout direkt am
                    // rechten Rand verankert. Dadurch bleibt der Knopf beim
                    // Vergrößern und Verkleinern immer exakt 35 Pixel vom Rand der Aluminiumfront entfernt.
                    ColumnLayout {
                        id: fmTuningColumn

                        width: 170
                        anchors.right: parent.right
                        anchors.rightMargin: 13
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6

                        Label {
                            Layout.preferredWidth: 135
                            Layout.alignment: Qt.AlignRight
                            horizontalAlignment: Text.AlignHCenter
                            text: "FM TUNING"
                            color: window.ink
                            font.pixelSize: 11
                            font.bold: true
                        }

                        TuningKnob {
                            Layout.preferredWidth: 135
                            Layout.preferredHeight: 135
                            Layout.minimumWidth: 135
                            Layout.minimumHeight: 135
                            Layout.alignment: Qt.AlignRight
                            enabled: xdrClient.ready && !xdrClient.seeking

                            onStepRequested: function(direction) {
                                xdrClient.stepSmall(direction)
                            }
                        }

                        Label {
                            Layout.preferredWidth: 170
                            Layout.alignment: Qt.AlignRight
                            horizontalAlignment: Text.AlignHCenter
                            text: "ziehen · klicken · Mausrad"
                            color: window.mutedInk
                            font.pixelSize: 10
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 142
                    color: "#171915"
                    border.color: "#55564f"
                    border.width: 3

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 18

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 5

                            RowLayout {
                                Layout.fillWidth: true
                                Label {
                                    Layout.fillWidth: true
                                    text: xdrClient.psText.length > 0
                                          ? xdrClient.psText : "RDS / PROGRAM SERVICE"
                                    color: xdrClient.rdsActive ? "#e6dba8" : "#77766b"
                                    font.family: "monospace"
                                    font.pixelSize: 27
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                                Label {
                                    text: xdrClient.rdsActive ? "● RDS" : "○ RDS"
                                    color: xdrClient.rdsActive ? "#d7b45d" : "#65655f"
                                    font.pixelSize: 13
                                    font.bold: true
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                text: xdrClient.radioText.length > 0
                                      ? xdrClient.radioText : "Radiotext wird empfangen …"
                                color: xdrClient.radioText.length > 0 ? "#d2d0bd" : "#67675f"
                                font.family: "monospace"
                                font.pixelSize: 16
                                wrapMode: Text.WordWrap
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        Rectangle {
                            width: 1
                            Layout.fillHeight: true
                            color: "#50514a"
                        }

                        GridLayout {
                            Layout.preferredWidth: 245
                            columns: 2
                            columnSpacing: 10
                            rowSpacing: 7

                            Label { text: "PI"; color: "#85857b" }
                            Label {
                                text: xdrClient.piCode
                                color: "#ded8b5"
                                font.family: "monospace"
                                font.bold: true
                            }
                            Label { text: "PTY"; color: "#85857b" }
                            Label {
                                text: xdrClient.ptyCode >= 0 ? xdrClient.ptyText : "–"
                                color: "#ded8b5"
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Label { text: "BW"; color: "#85857b" }
                            Label {
                                text: window.bandwidthLabel(xdrClient.bandwidthHz)
                                color: "#ded8b5"
                            }
                            Label { text: "RDS"; color: "#85857b" }
                            Label {
                                text: xdrClient.rdsGroupCount + " Gruppen"
                                color: "#ded8b5"
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.bottomMargin: 4
                    spacing: 12

                    Label {
                        Layout.fillWidth: true
                        text: xdrClient.statusText
                        color: window.ink
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    Label {
                        text: "FM 87,5–108 MHz  ·  "
                              + (xdrClient.bandwidthSettingHz > 0
                                 ? "BW " + window.bandwidthLabel(xdrClient.bandwidthSettingHz)
                                 : "BW AUTO")
                        color: window.mutedInk
                        font.pixelSize: 11
                    }
                }
            }
        }
    }
}
