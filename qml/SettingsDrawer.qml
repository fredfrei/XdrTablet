import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore

Drawer {
    id: settingsDrawer

    required property var client
    required property real hostWindowWidth
    required property real hostWindowHeight

    property color ink: "#262620"
    property color mutedInk: "#686861"
    property int sectionSpacing: 14
    property int smallFontSize: 12

    readonly property int connectionType: connectionTypeBox.currentIndex
    readonly property string tcpHost: hostField.text
    readonly property int tcpPort: Number(portField.text)
    readonly property string password: passwordField.text
    readonly property string usbPort: usbPortBox.currentText.length > 0
                                      ? usbPortBox.currentText
                                      : appSettings.usbPort
    readonly property int usbBaudRate: Number(usbBaudField.text)

    readonly property var stationPresetFrequencies: [
        appSettings.preset1Khz,
        appSettings.preset2Khz,
        appSettings.preset3Khz,
        appSettings.preset4Khz,
        appSettings.preset5Khz,
        appSettings.preset6Khz
    ]

    readonly property var bandwidthValues: [
        0, 56000, 64000, 72000, 84000, 97000, 114000, 133000,
        151000, 168000, 184000, 200000, 217000, 236000, 254000,
        287000, 311000
    ]
    readonly property var smallStepValues: [50, 100, 200]
    readonly property var largeStepValues: [500, 1000, 2000]

    signal connectionToggleRequested()

    edge: Qt.RightEdge
    width: Math.min(hostWindowWidth * 0.44, 520)
    height: hostWindowHeight
    modal: true

    onOpened: refreshUsbPorts()

    Component.onCompleted: {
        client.setRdsErrorCorrectionEnabled(
                    appSettings.rdsErrorCorrectionEnabled)
    }

    function valueIndex(values, value) {
        const index = values.indexOf(value)
        return index >= 0 ? index : 0
    }

    function refreshUsbPorts() {
        const ports = client.availableSerialPorts()
        usbPortBox.model = ports

        if (ports.length === 0) {
            usbPortBox.currentIndex = -1
            return
        }

        const index = ports.indexOf(appSettings.usbPort)
        usbPortBox.currentIndex = index >= 0 ? index : 0
        appSettings.usbPort = usbPortBox.currentText
    }

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
        const frequencyKhz = client.frequencyKhz

        if (frequencyKhz < client.minimumFmFrequencyKhz
                || frequencyKhz > client.maximumFmFrequencyKhz) {
            return
        }

        setPresetFrequency(index, frequencyKhz)
    }

    function clearPreset(index) {
        setPresetFrequency(index, 0)
    }

    function recallPreset(index) {
        const frequencyKhz = presetFrequency(index)

        if (frequencyKhz > 0 && client.ready && !client.seeking)
            client.setFrequencyKhz(frequencyKhz)
    }

    function presetFrequencyText(index) {
        const frequencyKhz = presetFrequency(index)
        return frequencyKhz > 0
               ? (frequencyKhz / 1000).toFixed(3) + " MHz"
               : "–"
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
            spacing: settingsDrawer.sectionSpacing

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.topMargin: 18
                text: "XDR · EINSTELLUNGEN"
                color: settingsDrawer.ink
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

                Label { text: "Verbindung"; color: settingsDrawer.ink }
                ComboBox {
                    id: connectionTypeBox
                    Layout.fillWidth: true
                    model: ["TCP", "USB"]
                    enabled: !client.connected
                }

                Label {
                    text: "IP-Adresse"
                    color: settingsDrawer.ink
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

                Label { text: "TCP-Port"; color: settingsDrawer.ink }
                TextField {
                    id: portField
                    Layout.fillWidth: true
                    enabled: connectionTypeBox.currentIndex === 0
                    text: "7373"
                    placeholderText: "Port"
                    inputMethodHints: Qt.ImhDigitsOnly
                }

                Label { text: "Passwort"; color: settingsDrawer.ink }
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
                    color: settingsDrawer.ink
                    enabled: connectionTypeBox.currentIndex === 1
                }
                ComboBox {
                    id: usbPortBox
                    Layout.fillWidth: true
                    enabled: connectionTypeBox.currentIndex === 1 && count > 0
                    model: []
                    displayText: count > 0
                                 ? currentText
                                 : "Kein serieller Port gefunden"
                    onActivated: appSettings.usbPort = currentText
                }

                Label {
                    text: "USB-Baudrate"
                    color: settingsDrawer.ink
                    enabled: connectionTypeBox.currentIndex === 1
                }
                TextField {
                    id: usbBaudField
                    Layout.fillWidth: true
                    text: "115200"
                    inputMethodHints: Qt.ImhDigitsOnly
                    enabled: connectionTypeBox.currentIndex === 1
                }
            }

            VintageButton {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: 230
                text: client.connected ? "VERBINDUNG TRENNEN" : "VERBINDEN"
                onClicked: settingsDrawer.connectionToggleRequested()
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
                        enabled: client.ready && !client.seeking
                        text: checked ? "Mono erzwungen" : "Stereo-Automatik"
                        checked: client.forcedMono
                        onToggled: {
                            if (checked !== client.forcedMono)
                                client.setForcedMono(checked)
                        }
                    }

                    Label { text: "Bandbreite" }
                    ComboBox {
                        Layout.fillWidth: true
                        enabled: client.ready && !client.seeking
                        model: ["Auto", "56 kHz", "64 kHz", "72 kHz",
                                "84 kHz", "97 kHz", "114 kHz", "133 kHz",
                                "151 kHz", "168 kHz", "184 kHz", "200 kHz",
                                "217 kHz", "236 kHz", "254 kHz", "287 kHz",
                                "311 kHz"]
                        currentIndex: settingsDrawer.valueIndex(
                                          settingsDrawer.bandwidthValues,
                                          client.bandwidthSettingHz)
                        onActivated: client.setBandwidth(
                                         settingsDrawer.bandwidthValues[
                                             currentIndex])
                    }

                    Label { text: "De-Emphasis" }
                    ComboBox {
                        Layout.fillWidth: true
                        enabled: client.ready
                        model: ["50 µs", "75 µs", "0 µs"]
                        currentIndex: client.deemphasis
                        onActivated: client.setDeemphasis(currentIndex)
                    }

                    Label { text: "AGC-Schwelle" }
                    ComboBox {
                        Layout.fillWidth: true
                        enabled: client.ready
                        model: ["Höchste", "Hoch", "Mittel", "Niedrig"]
                        currentIndex: client.agc
                        onActivated: client.setAgc(currentIndex)
                    }

                    Label { text: "Signalverarbeitung" }
                    ColumnLayout {
                        spacing: 2

                        CheckBox {
                            enabled: client.ready
                            text: "Channel Equalizer"
                            checked: client.channelEqualizer
                            onToggled: {
                                if (checked !== client.channelEqualizer)
                                    client.setChannelEqualizer(checked)
                            }
                        }

                        CheckBox {
                            enabled: client.ready
                            text: "iMS / Multipath"
                            checked: client.multipathSuppression
                            onToggled: {
                                if (checked !== client.multipathSuppression)
                                    client.setMultipathSuppression(checked)
                            }
                        }
                    }

                    Label { text: "RDS-Fehlerkorrektur" }
                    Switch {
                        text: checked
                              ? "Fehlerstatus 0,1,2"
                              : "Fehlerstatus nur 0"
                        checked: client.rdsErrorCorrectionEnabled
                        onToggled: {
                            appSettings.rdsErrorCorrectionEnabled = checked
                            if (checked !== client.rdsErrorCorrectionEnabled)
                                client.setRdsErrorCorrectionEnabled(checked)
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
                        enabled: !client.seeking
                        model: ["50 kHz", "100 kHz", "200 kHz"]
                        currentIndex: settingsDrawer.valueIndex(
                                          settingsDrawer.smallStepValues,
                                          client.smallStepKhz)
                        onActivated: client.setSmallStepKhz(
                                         settingsDrawer.smallStepValues[
                                             currentIndex])
                    }

                    Label { text: "Großer Schritt" }
                    ComboBox {
                        Layout.fillWidth: true
                        enabled: !client.seeking
                        model: ["500 kHz", "1 MHz", "2 MHz"]
                        currentIndex: settingsDrawer.valueIndex(
                                          settingsDrawer.largeStepValues,
                                          client.largeStepKhz)
                        onActivated: client.setLargeStepKhz(
                                         settingsDrawer.largeStepValues[
                                             currentIndex])
                    }

                    Label { text: "Suchempfindlichkeit" }
                    SpinBox {
                        from: 1
                        to: 30
                        value: client.seekThreshold
                        editable: true
                        enabled: !client.seeking
                        onValueModified: client.setSeekThreshold(value)
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
                    spacing: 7

                    Repeater {
                        model: 6

                        delegate: RowLayout {
                            required property int index
                            Layout.fillWidth: true
                            spacing: 7

                            Label {
                                Layout.preferredWidth: 28
                                text: "M" + (index + 1)
                                color: settingsDrawer.ink
                                font.bold: true
                            }

                            Label {
                                Layout.fillWidth: true
                                text: settingsDrawer.presetFrequencyText(index)
                                color: settingsDrawer.presetFrequency(index) > 0
                                       ? settingsDrawer.ink
                                       : settingsDrawer.mutedInk
                                font.family: "monospace"
                                elide: Text.ElideRight
                            }

                            Button {
                                text: "Speichern"
                                enabled: client.ready && !client.seeking
                                onClicked: settingsDrawer.storePreset(index)
                            }

                            Button {
                                text: "Löschen"
                                enabled: settingsDrawer.presetFrequency(index) > 0
                                onClicked: settingsDrawer.clearPreset(index)
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: "Kurzen Zeiger in der Skala antippen, "
                              + "um den Sender aufzurufen."
                        color: settingsDrawer.mutedInk
                        font.pixelSize: settingsDrawer.smallFontSize
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                text: "Letzte Antwort: " + (client.lastLine || "–")
                color: settingsDrawer.mutedInk
                wrapMode: Text.WrapAnywhere
                font.pixelSize: 12
            }

            Item { Layout.preferredHeight: 20 }
        }
    }
}
