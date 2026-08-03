import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    // Der vorhandene xdrClient wird beim Einbau aus Main.qml übergeben.
    required property var controller
    property bool powerEnabled: false
    property int currentPage: 0

    signal powerToggleRequested()
    signal powerQuitRequested()
    signal settingsRequested()
    signal bandRequested()
    signal muteRequested()
    signal modeRequested()

    readonly property color panel: "#171817"
    readonly property color panelLight: "#292a27"
    readonly property color panelBorder: "#5e5a4f"
    readonly property color amber: "#e2a447"
    readonly property color amberBright: "#ffc56a"
    readonly property color cream: "#e6ddc8"
    readonly property color muted: "#928b7c"
    readonly property color green: "#77bd59"
    readonly property color red: "#a6554f"

    readonly property bool connected: controller ? controller.connected : false
    readonly property bool ready: controller ? controller.ready : false
    readonly property bool seeking: controller ? controller.seeking : false
    readonly property int frequencyKhz: controller ? controller.frequencyKhz : 102300
    readonly property int minimumKhz: controller ? controller.minimumFmFrequencyKhz : 87500
    readonly property int maximumKhz: controller ? controller.maximumFmFrequencyKhz : 108000
    readonly property real signalLevel: controller && controller.signalAvailable
                                                ? controller.signalLevel : 0
    readonly property int litSignalSegments: Math.round(
        Math.max(0, Math.min(80, signalLevel)) / 80 * 12)

    implicitWidth: 390
    implicitHeight: 844

    function frequencyText() {
        return (frequencyKhz / 1000).toFixed(3)
    }

    function qualityValue() {
        if (!controller || controller.cci < 0 || controller.aci < 0)
            return 0
        return Math.max(0, Math.min(100,
                    (controller.cci + controller.aci) / 2))
    }

    function bandwidthText(hz) {
        return hz > 0 ? Math.round(hz / 1000) + " kHz" : "AUTO"
    }

    component PanelButton: Button {
        id: button
        implicitHeight: 66
        leftPadding: 8
        rightPadding: 8
        topPadding: 7
        bottomPadding: 7
        font.pixelSize: 14
        font.bold: true

        contentItem: Text {
            text: button.text
            color: button.enabled ? root.cream : "#655f54"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
            font: button.font
            lineHeight: 0.92
            maximumLineCount: 2
            fontSizeMode: Text.Fit
            minimumPixelSize: 10
        }

        background: Rectangle {
            radius: 7
            border.width: 1
            border.color: button.down ? root.amber : root.panelBorder
            gradient: Gradient {
                GradientStop {
                    position: 0
                    color: button.down ? "#171713"
                                       : (button.hovered ? "#34332e" : "#2a2a27")
                }
                GradientStop {
                    position: 1
                    color: button.down ? "#28261f" : "#171817"
                }
            }
        }

        opacity: enabled ? 1.0 : 0.48
    }

    component InfoCard: Rectangle {
        radius: 8
        color: root.panel
        border.width: 1
        border.color: root.panelBorder
    }

    Rectangle {
        anchors.fill: parent
        color: "#111211"

        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#111211" }
            GradientStop { position: 0.5; color: "#242521" }
            GradientStop { position: 1.0; color: "#111211" }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Kopfzeile: POWER, Status und Einstellungen.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 82
            color: "#191a18"
            border.color: "#3d3b35"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                anchors.topMargin: 8
                anchors.bottomMargin: 8
                spacing: 12

                ColumnLayout {
                    Layout.preferredWidth: 64
                    spacing: 2

                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: "POWER"
                        color: root.cream
                        font.pixelSize: 10
                        font.bold: true
                    }

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 42
                        height: 42
                        radius: 4
                        color: "#242421"
                        border.color: root.panelBorder

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            y: root.powerEnabled ? 5 : 21
                            width: 22
                            height: 16
                            radius: 2
                            color: root.powerEnabled ? "#4b4334" : "#151513"
                            border.color: root.powerEnabled ? root.amber : "#4b4840"
                            Behavior on y { NumberAnimation { duration: 130 } }
                        }

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            y: root.powerEnabled ? 9 : 27
                            width: 6
                            height: 6
                            radius: 3
                            color: root.powerEnabled ? root.amberBright : "#4a4740"
                            Behavior on y { NumberAnimation { duration: 130 } }
                        }

                        MouseArea {
                            anchors.fill: parent
                            pressAndHoldInterval: 1200
                            property bool held: false

                            onPressed: held = false
                            onPressAndHold: {
                                held = true
                                root.powerQuitRequested()
                            }
                            onClicked: {
                                if (!held)
                                    root.powerToggleRequested()
                            }
                        }
                    }

                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: root.powerEnabled ? "ON" : "OFF"
                        color: root.powerEnabled ? root.amberBright : root.muted
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 46
                    radius: 7
                    border.width: 1
                    border.color: root.ready ? "#7f744e" : root.panelBorder
                    gradient: Gradient {
                        GradientStop { position: 0; color: "#2b2b27" }
                        GradientStop { position: 1; color: "#171817" }
                    }

                    Label {
                        anchors.centerIn: parent
                        text: !root.powerEnabled ? "○ AUS"
                              : root.ready ? "● ONLINE"
                              : root.connected ? "◐ VERBINDUNG"
                              : "○ OFFLINE"
                        color: !root.powerEnabled ? root.muted
                               : root.ready ? root.green
                               : root.connected ? root.amberBright
                               : root.red
                        font.pixelSize: 14
                        font.bold: true
                        font.letterSpacing: 0.8
                    }
                }

                PanelButton {
                    Layout.preferredWidth: 58
                    Layout.preferredHeight: 48
                    implicitHeight: 48
                    text: "⚙"
                    font.pixelSize: 24
                    onClicked: root.settingsRequested()
                }
            }
        }

        StackLayout {
            id: pages
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentPage

            // RADIO
            ScrollView {
                id: radioScroll
                clip: true
                contentWidth: availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    x: 10
                    width: Math.max(0, radioScroll.availableWidth - 20)
                    spacing: 10

                    Item { Layout.preferredHeight: 1 }

                    InfoCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(330,
                            Math.min(405, radioScroll.availableWidth * 1.05))

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 2

                            Label {
                                Layout.alignment: Qt.AlignHCenter
                                text: "FM"
                                color: root.amberBright
                                font.pixelSize: 22
                                font.bold: true
                                font.letterSpacing: 1.5
                            }

                            Label {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                text: root.frequencyText()
                                color: root.amberBright
                                font.family: "monospace"
                                font.pixelSize: Math.max(45,
                                    Math.min(66, radioScroll.availableWidth * 0.16))
                                font.bold: false
                                fontSizeMode: Text.Fit
                                minimumPixelSize: 38
                            }

                            Label {
                                Layout.alignment: Qt.AlignHCenter
                                text: "MHz"
                                color: root.amber
                                font.pixelSize: 17
                                font.bold: true
                                font.letterSpacing: 1.0
                            }

                            Label {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                text: root.controller && root.controller.psText.length > 0
                                      ? root.controller.psText : "XDR FM RADIO"
                                color: root.cream
                                font.pixelSize: 21
                                font.bold: true
                                elide: Text.ElideRight
                            }

                            Rectangle {
                                id: drumFrame
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.minimumHeight: 142
                                Layout.topMargin: 4
                                radius: 8
                                color: "#0d0e0d"
                                border.width: 2
                                border.color: "#6f654d"
                                clip: true

                                Canvas {
                                    id: drumCanvas
                                    anchors.fill: parent
                                    anchors.margins: 4
                                    property int frequencyKhz: root.frequencyKhz
                                    property int minimumKhz: root.minimumKhz
                                    property int maximumKhz: root.maximumKhz

                                    onFrequencyKhzChanged: requestPaint()
                                    onMinimumKhzChanged: requestPaint()
                                    onMaximumKhzChanged: requestPaint()
                                    onWidthChanged: requestPaint()
                                    onHeightChanged: requestPaint()

                                    onPaint: {
                                        const ctx = getContext("2d")
                                        const w = width
                                        const h = height
                                        ctx.clearRect(0, 0, w, h)

                                        // Gewölbte Trommel: dunkle Ränder, hellere Mitte.
                                        const bg = ctx.createLinearGradient(0, 0, w, 0)
                                        bg.addColorStop(0.00, "#080908")
                                        bg.addColorStop(0.10, "#24221d")
                                        bg.addColorStop(0.42, "#423b2d")
                                        bg.addColorStop(0.50, "#514735")
                                        bg.addColorStop(0.58, "#423b2d")
                                        bg.addColorStop(0.90, "#24221d")
                                        bg.addColorStop(1.00, "#080908")
                                        ctx.fillStyle = bg
                                        ctx.fillRect(0, 0, w, h)

                                        const pxPerKhz = w / 2600.0
                                        const first = Math.floor((frequencyKhz - 1500) / 50) * 50
                                        const last = Math.ceil((frequencyKhz + 1500) / 50) * 50
                                        const baseY = Math.round(h * 0.68)

                                        ctx.textAlign = "center"
                                        ctx.textBaseline = "middle"

                                        for (let khz = first; khz <= last; khz += 50) {
                                            if (khz < minimumKhz || khz > maximumKhz)
                                                continue

                                            const x = w / 2 + (khz - frequencyKhz) * pxPerKhz
                                            const major = khz % 500 === 0
                                            const medium = khz % 100 === 0
                                            const tickHeight = major ? 27 : (medium ? 18 : 10)

                                            ctx.strokeStyle = major ? "#f1c879" : "#c69d58"
                                            ctx.lineWidth = major ? 2 : 1
                                            ctx.beginPath()
                                            ctx.moveTo(x, baseY)
                                            ctx.lineTo(x, baseY - tickHeight)
                                            ctx.stroke()

                                            if (major) {
                                                ctx.fillStyle = "#ead8ae"
                                                ctx.font = "600 16px sans-serif"
                                                ctx.fillText((khz / 1000).toFixed(1),
                                                             x, Math.round(h * 0.31))
                                            }
                                        }

                                        // Kleine Übersicht über das ganze FM-Band.
                                        const overviewY = Math.round(h * 0.86)
                                        ctx.strokeStyle = "#a47a3a"
                                        ctx.lineWidth = 1
                                        ctx.beginPath()
                                        ctx.moveTo(12, overviewY)
                                        ctx.lineTo(w - 12, overviewY)
                                        ctx.stroke()

                                        const overviewLabels = [87500, 90000, 95000,
                                                                100000, 105000, 108000]
                                        ctx.font = "600 11px sans-serif"
                                        ctx.fillStyle = "#d1aa68"
                                        for (let i = 0; i < overviewLabels.length; ++i) {
                                            const value = overviewLabels[i]
                                            const ox = 12 + (value - minimumKhz) /
                                                       Math.max(1, maximumKhz - minimumKhz) * (w - 24)
                                            ctx.fillText(value === 87500 ? "87.5"
                                                         : (value / 1000).toFixed(0),
                                                         ox, Math.round(h * 0.94))
                                        }

                                        // Fester Zeiger; die Trommel bewegt sich darunter.
                                        ctx.strokeStyle = "#ffb340"
                                        ctx.lineWidth = 3
                                        ctx.beginPath()
                                        ctx.moveTo(w / 2, 6)
                                        ctx.lineTo(w / 2, baseY + 8)
                                        ctx.stroke()

                                        ctx.fillStyle = "#ffc15d"
                                        ctx.beginPath()
                                        ctx.moveTo(w / 2 - 7, 5)
                                        ctx.lineTo(w / 2 + 7, 5)
                                        ctx.lineTo(w / 2, 16)
                                        ctx.closePath()
                                        ctx.fill()

                                        // Glanz über dem Sichtfenster.
                                        const shine = ctx.createLinearGradient(0, 0, 0, h)
                                        shine.addColorStop(0.0, "rgba(255,255,255,0.12)")
                                        shine.addColorStop(0.28, "rgba(255,255,255,0.02)")
                                        shine.addColorStop(1.0, "rgba(0,0,0,0.18)")
                                        ctx.fillStyle = shine
                                        ctx.fillRect(0, 0, w, h)
                                    }
                                }

                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    width: 14
                                    gradient: Gradient {
                                        orientation: Gradient.Horizontal
                                        GradientStop { position: 0; color: "#070807" }
                                        GradientStop { position: 1; color: "transparent" }
                                    }
                                }

                                Rectangle {
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    width: 14
                                    gradient: Gradient {
                                        orientation: Gradient.Horizontal
                                        GradientStop { position: 0; color: "transparent" }
                                        GradientStop { position: 1; color: "#070807" }
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 48
                                color: "#141513"
                                border.color: "#36352f"
                                radius: 5

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 10
                                    spacing: 8

                                    Label {
                                        text: "SIGNAL"
                                        color: root.cream
                                        font.pixelSize: 11
                                        font.bold: true
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Repeater {
                                            model: 12
                                            Rectangle {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 12
                                                radius: 1
                                                color: index < root.litSignalSegments
                                                       ? root.amber : "#34342f"
                                                border.color: "#4b493f"
                                            }
                                        }
                                    }

                                    Label {
                                        text: "STEREO"
                                        color: root.cream
                                        font.pixelSize: 10
                                        font.bold: true
                                    }
                                    Rectangle {
                                        width: 9
                                        height: 9
                                        radius: 5
                                        color: root.ready && !root.controller.forcedMono
                                               ? root.green : "#44443e"
                                    }

                                    Label {
                                        text: "RDS"
                                        color: root.cream
                                        font.pixelSize: 10
                                        font.bold: true
                                    }
                                    Rectangle {
                                        width: 9
                                        height: 9
                                        radius: 5
                                        color: root.controller && root.controller.rdsActive
                                               ? root.amberBright : "#44443e"
                                    }
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        PanelButton {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 78
                            text: "◀ SEEK\n−" + (root.controller
                                  ? root.controller.smallStepKhz : 100) + " kHz"
                            enabled: root.ready && !root.seeking
                            onClicked: root.controller.startSeek(-1)
                        }

                        ColumnLayout {
                            Layout.preferredWidth: 126
                            spacing: 2

                            TuningKnob {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: 116
                                Layout.preferredHeight: 116
                                enabled: root.ready && !root.seeking

                                onStepRequested: function(direction) {
                                    root.controller.stepSmall(direction)
                                }
                            }

                            Label {
                                Layout.alignment: Qt.AlignHCenter
                                text: "TUNING"
                                color: root.amberBright
                                font.pixelSize: 12
                                font.bold: true
                                font.letterSpacing: 1.0
                            }
                        }

                        PanelButton {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 78
                            text: "SEEK ▶\n+" + (root.controller
                                  ? root.controller.smallStepKhz : 100) + " kHz"
                            enabled: root.ready && !root.seeking
                            onClicked: root.controller.startSeek(1)
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 8
                        rowSpacing: 8

                        PanelButton {
                            Layout.fillWidth: true
                            text: root.seeking ? "■ STOP" : "◎ SCAN"
                            enabled: root.ready
                            onClicked: {
                                if (root.seeking)
                                    root.controller.stopSeek()
                                else
                                    root.controller.startSeek(1)
                            }
                        }

                        PanelButton {
                            Layout.fillWidth: true
                            text: "FM\nBAND"
                            onClicked: root.bandRequested()
                        }

                        PanelButton {
                            Layout.fillWidth: true
                            text: root.controller && root.controller.forcedMono
                                  ? "MONO\nERZWUNGEN" : "STEREO\nAUTO"
                            enabled: root.ready && !root.seeking
                            onClicked: root.controller.setForcedMono(
                                           !root.controller.forcedMono)
                        }

                        PanelButton {
                            Layout.fillWidth: true
                            text: "MUTE"
                            onClicked: root.muteRequested()
                        }

                        PanelButton {
                            Layout.fillWidth: true
                            text: root.controller &&
                                  root.controller.bandwidthSettingHz === 0
                                  ? "BW AUTO\nAKTIV" : "BW\nAUTO"
                            enabled: root.ready && !root.seeking
                            onClicked: root.controller.setBandwidth(0)
                        }

                        PanelButton {
                            Layout.fillWidth: true
                            text: "MODE"
                            onClicked: root.modeRequested()
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: root.controller ? root.controller.statusText : "Tuner bereit"
                        color: root.muted
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                }
            }

            // QUALITÄT
            ScrollView {
                id: qualityScroll
                clip: true
                contentWidth: availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    x: 12
                    width: Math.max(0, qualityScroll.availableWidth - 24)
                    spacing: 10

                    Item { Layout.preferredHeight: 1 }

                    Label {
                        Layout.fillWidth: true
                        text: "EMPFANGSQUALITÄT"
                        color: root.amberBright
                        font.pixelSize: 22
                        font.bold: true
                    }

                    InfoCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 118

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14

                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: "SIGNAL"; color: root.cream; font.bold: true }
                                Label {
                                    Layout.fillWidth: true
                                    horizontalAlignment: Text.AlignRight
                                    text: root.signalLevel.toFixed(2)
                                    color: root.amberBright
                                    font.family: "monospace"
                                    font.pixelSize: 22
                                    font.bold: true
                                }
                            }

                            ProgressBar {
                                Layout.fillWidth: true
                                from: 0
                                to: 80
                                value: root.signalLevel
                            }
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 10
                        rowSpacing: 10

                        Repeater {
                            model: [
                                { title: "CCI", value: root.controller && root.controller.cci >= 0
                                                        ? root.controller.cci : "–" },
                                { title: "ACI", value: root.controller && root.controller.aci >= 0
                                                        ? root.controller.aci : "–" },
                                { title: "QUALITÄT", value: Math.round(root.qualityValue()) + " %" },
                                { title: "BANDBREITE", value: root.controller
                                        ? root.bandwidthText(root.controller.bandwidthHz) : "–" }
                            ]

                            delegate: InfoCard {
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.preferredHeight: 102

                                ColumnLayout {
                                    anchors.centerIn: parent
                                    spacing: 6
                                    Label {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: modelData.title
                                        color: root.muted
                                        font.pixelSize: 12
                                        font.bold: true
                                    }
                                    Label {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: modelData.value
                                        color: root.amberBright
                                        font.family: "monospace"
                                        font.pixelSize: 23
                                        font.bold: true
                                    }
                                }
                            }
                        }
                    }

                    InfoCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 112

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            Label {
                                text: "EMPFANGSMODUS"
                                color: root.muted
                                font.pixelSize: 12
                                font.bold: true
                            }
                            Label {
                                Layout.fillWidth: true
                                text: root.controller
                                      ? root.controller.receptionModeText : "–"
                                color: root.cream
                                font.pixelSize: 19
                                font.bold: true
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }

            // RDS
            ScrollView {
                id: rdsScroll
                clip: true
                contentWidth: availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    x: 12
                    width: Math.max(0, rdsScroll.availableWidth - 24)
                    spacing: 10

                    Item { Layout.preferredHeight: 1 }

                    Label {
                        Layout.fillWidth: true
                        text: "RDS"
                        color: root.amberBright
                        font.pixelSize: 22
                        font.bold: true
                    }

                    InfoCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 125

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            Label {
                                Layout.fillWidth: true
                                text: root.controller && root.controller.psText.length > 0
                                      ? root.controller.psText : "PROGRAM SERVICE"
                                color: root.controller && root.controller.rdsActive
                                       ? root.amberBright : root.muted
                                font.family: "monospace"
                                font.pixelSize: 28
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.alignment: Qt.AlignHCenter
                                text: root.controller && root.controller.rdsActive
                                      ? "● RDS AKTIV" : "○ KEIN RDS"
                                color: root.controller && root.controller.rdsActive
                                       ? root.green : root.muted
                                font.pixelSize: 12
                                font.bold: true
                            }
                        }
                    }

                    InfoCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 190

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            Label {
                                text: "RADIOTEXT"
                                color: root.muted
                                font.pixelSize: 12
                                font.bold: true
                            }
                            Label {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                text: root.controller && root.controller.radioText.length > 0
                                      ? root.controller.radioText
                                      : "Radiotext wird empfangen …"
                                color: root.cream
                                font.family: "monospace"
                                font.pixelSize: 17
                                wrapMode: Text.WordWrap
                                verticalAlignment: Text.AlignTop
                            }
                        }
                    }

                    InfoCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 190

                        GridLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 12

                            Label { text: "PI"; color: root.muted }
                            Label {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignRight
                                text: root.controller ? root.controller.piCode : "–"
                                color: root.cream
                                font.family: "monospace"
                                font.bold: true
                            }
                            Label { text: "PTY"; color: root.muted }
                            Label {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignRight
                                text: root.controller && root.controller.ptyCode >= 0
                                      ? root.controller.ptyText : "–"
                                color: root.cream
                                elide: Text.ElideRight
                            }
                            Label { text: "BW"; color: root.muted }
                            Label {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignRight
                                text: root.controller
                                      ? root.bandwidthText(root.controller.bandwidthHz) : "–"
                                color: root.cream
                            }
                            Label { text: "GRUPPEN"; color: root.muted }
                            Label {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignRight
                                text: root.controller
                                      ? root.controller.rdsGroupCount : "0"
                                color: root.cream
                                font.family: "monospace"
                                font.bold: true
                            }
                        }
                    }
                }
            }
        }

        // Untere Navigation.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 72
            color: "#151614"
            border.color: "#393832"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                spacing: 0

                Repeater {
                    model: [
                        { label: "RADIO", symbol: "▣", page: 0 },
                        { label: "QUALITÄT", symbol: "▥", page: 1 },
                        { label: "RDS", symbol: "RDS", page: 2 },
                        { label: "EINSTELL.", symbol: "⚙", page: 3 }
                    ]

                    delegate: Button {
                        id: navButton
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        flat: true

                        contentItem: ColumnLayout {
                            spacing: 1
                            Label {
                                Layout.alignment: Qt.AlignHCenter
                                text: navButton.modelData.symbol
                                color: (navButton.modelData.page < 3 &&
                                        root.currentPage === navButton.modelData.page)
                                       ? root.amberBright : root.muted
                                font.pixelSize: navButton.modelData.page === 2 ? 11 : 19
                                font.bold: true
                            }
                            Label {
                                Layout.alignment: Qt.AlignHCenter
                                text: navButton.modelData.label
                                color: (navButton.modelData.page < 3 &&
                                        root.currentPage === navButton.modelData.page)
                                       ? root.amberBright : root.muted
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }

                        background: Rectangle {
                            color: navButton.down ? "#282720" : "transparent"

                            Rectangle {
                                anchors.top: parent.top
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: parent.width * 0.72
                                height: 2
                                visible: navButton.modelData.page < 3 &&
                                         root.currentPage === navButton.modelData.page
                                color: root.amberBright
                            }
                        }

                        onClicked: {
                            if (modelData.page === 3)
                                root.settingsRequested()
                            else
                                root.currentPage = modelData.page
                        }
                    }
                }
            }
        }
    }
}
