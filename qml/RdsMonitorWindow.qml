import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: rdsInfoWindow

    required property var client

    property color aluminiumMid: "#c9c9c3"
    property color ink: "#262620"
    property color mutedInk: "#686861"

    width: 1180
    height: 820
    minimumWidth: 850
    minimumHeight: 600

    visible: false
    title: "RDS-Monitor"

    color: rdsInfoWindow.aluminiumMid

    flags: Qt.Dialog
           | Qt.WindowTitleHint
           | Qt.WindowCloseButtonHint
           | Qt.WindowMaximizeButtonHint

    modality: Qt.NonModal

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

                    text: rdsInfoWindow.client.psText.length > 0
                          ? rdsInfoWindow.client.psText
                          : "RDS-Monitor"

                    color: rdsInfoWindow.ink
                    font.pixelSize: 22
                    font.bold: true
                }

                Label {
                    text: "RDS-Monitor"
                    color: rdsInfoWindow.mutedInk
                    font.pixelSize: 13
                    font.bold: true
                }
            }

            Label {
                Layout.fillWidth: true
                text: "Nur Daten aus dem empfangenen RDS-Signal"
                color: rdsInfoWindow.mutedInk
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
                                color: rdsInfoWindow.ink
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
                                    color: rdsInfoWindow.mutedInk
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: rdsInfoWindow.client.piCode
                                    color: rdsInfoWindow.ink
                                    font.family: "monospace"
                                    font.bold: true
                                }

                                Label {
                                    text: "PS"
                                    color: rdsInfoWindow.mutedInk
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: rdsInfoWindow.client.psText.length > 0
                                          ? rdsInfoWindow.client.psText
                                          : "–"
                                    color: rdsInfoWindow.ink
                                }

                                Label {
                                    text: "PTY"
                                    color: rdsInfoWindow.mutedInk
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: rdsInfoWindow.client.ptyCode >= 0
                                          ? rdsInfoWindow.client.ptyCode
                                            + "  "
                                            + rdsInfoWindow.client.ptyText
                                          : "–"
                                    color: rdsInfoWindow.ink
                                }

                                Label {
                                    text: "ECC"
                                    color: rdsInfoWindow.mutedInk
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: rdsInfoWindow.client.eccCode !== "--"
                                          ? rdsInfoWindow.client.eccCode
                                          : "–"
                                    color: rdsInfoWindow.ink
                                }

                                Label {
                                    text: "PIN"
                                    color: rdsInfoWindow.mutedInk
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: rdsInfoWindow.client.pinText.length > 0
                                          ? rdsInfoWindow.client.pinText
                                          : "–"
                                    color: rdsInfoWindow.ink
                                }

                                Label {
                                    text: "CT"
                                    color: rdsInfoWindow.mutedInk
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: rdsInfoWindow.client.ctText.length > 0
                                          ? rdsInfoWindow.client.ctText
                                          : "–"
                                    color: rdsInfoWindow.ink
                                }

                                Label {
                                    text: "RDS"
                                    color: rdsInfoWindow.mutedInk
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: rdsInfoWindow.client.rdsActive
                                          ? rdsInfoWindow.client.rdsGroupCount
                                            + " Gruppen"
                                          : "nicht aktiv"
                                    color: rdsInfoWindow.ink
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
                                color: rdsInfoWindow.ink
                                font.pixelSize: 15
                                font.bold: true
                            }

                            Label {
                                Layout.fillWidth: true
                                text: rdsInfoWindow.client.radioText.length > 0
                                      ? rdsInfoWindow.client.radioText
                                      : "–"
                                color: rdsInfoWindow.ink
                                wrapMode: Text.WordWrap
                            }

                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: 14
                                rowSpacing: 4

                                Label {
                                    text: "Titel"
                                    color: rdsInfoWindow.mutedInk
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: rdsInfoWindow.client.rtPlusTitle.length > 0
                                          ? rdsInfoWindow.client.rtPlusTitle
                                          : "–"
                                    color: rdsInfoWindow.ink
                                    wrapMode: Text.WordWrap
                                }

                                Label {
                                    text: "Interpret"
                                    color: rdsInfoWindow.mutedInk
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: rdsInfoWindow.client.rtPlusArtist.length > 0
                                          ? rdsInfoWindow.client.rtPlusArtist
                                          : "–"
                                    color: rdsInfoWindow.ink
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
                                color: rdsInfoWindow.ink
                                font.pixelSize: 15
                                font.bold: true
                            }

                            Label {
                                Layout.fillWidth: true
                                text: rdsInfoWindow.client.rdsFlagsText
                                color: rdsInfoWindow.ink
                                font.family: "monospace"
                            }

                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: 14

                                Label {
                                    text: "PTYN"
                                    color: rdsInfoWindow.mutedInk
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: rdsInfoWindow.client.rdsPtynText
                                    color: rdsInfoWindow.ink
                                }

                                Label {
                                    text: "Sprache"
                                    color: rdsInfoWindow.mutedInk
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: rdsInfoWindow.client.rdsLanguageText
                                    color: rdsInfoWindow.ink
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
                                color: rdsInfoWindow.ink
                                font.pixelSize: 15
                                font.bold: true
                            }

                            Label {
                                Layout.fillWidth: true
                                text: rdsInfoWindow.client.rdsGroupStats
                                color: rdsInfoWindow.ink
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
                                color: rdsInfoWindow.ink
                                font.pixelSize: 15
                                font.bold: true
                            }

                            Label {
                                Layout.fillWidth: true
                                text: rdsInfoWindow.client.rdsErrorStats
                                color: rdsInfoWindow.ink
                                font.family: "monospace"
                                font.pixelSize: 12
                            }

                            Label {
                                Layout.fillWidth: true
                                text:
                                    "0 = fehlerfrei, "
                                    + "1/2 = korrigiert, "
                                    + "3 = unbrauchbar"
                                color: rdsInfoWindow.mutedInk
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
                                color: rdsInfoWindow.ink
                                font.pixelSize: 15
                                font.bold: true
                            }

                            Label {
                                Layout.fillWidth: true

                                text:
                                    rdsInfoWindow.client.rdsRawGroups.length > 0
                                    ? rdsInfoWindow.client.rdsRawGroups
                                    : "–"

                                color: rdsInfoWindow.ink

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
                                color: rdsInfoWindow.ink
                                font.pixelSize: 15
                                font.bold: true
                            }

                            Label {
                                Layout.fillWidth: true

                                text: rdsInfoWindow.client.rdsAfText

                                color: rdsInfoWindow.ink

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
                                color: rdsInfoWindow.ink
                                font.pixelSize: 15
                                font.bold: true
                            }

                            Label {
                                Layout.fillWidth: true

                                text: rdsInfoWindow.client.rdsOdaText

                                color: rdsInfoWindow.ink

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
                                color: rdsInfoWindow.ink
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
                                        rdsInfoWindow.client.rdsEonText

                                    color: rdsInfoWindow.ink

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
                        rdsInfoWindow.mutedInk

                    font.pixelSize: 10

                    elide:
                        Text.ElideMiddle
                }

                Button {
                    text: "Speichern"

                    onClicked: {
                        rdsInfoWindow.saveMessage =
                            rdsInfoWindow.client.saveRdsMonitor()
                    }
                }

                Button {
                    text: "Monitor zurücksetzen"

                    onClicked: {
                        rdsInfoWindow.client.clearRdsMonitor()
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
