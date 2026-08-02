import QtQuick
import QtQuick.Controls

Button {
    id: control

    implicitWidth: 132
    implicitHeight: 36

    leftInset: 0
    rightInset: 0
    topInset: 0
    bottomInset: 0

    leftPadding: 10
    rightPadding: 10
    topPadding: 4
    bottomPadding: 4

    font.pixelSize: Math.max(12, Math.min(14, height * 0.40))
    font.bold: true

    contentItem: Text {
        text: control.text
        color: control.enabled ? "#202020" : "#777772"

        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter

        font.family: control.font.family
        font.bold: control.font.bold
        font.pixelSize: control.font.pixelSize

        fontSizeMode: Text.Fit
        minimumPixelSize: 11

        wrapMode: Text.NoWrap
        maximumLineCount: 1
        elide: Text.ElideNone
        clip: true
    }

    background: Rectangle {
        radius: 2
        border.width: 1
        border.color: control.down ? "#555550" : "#8b8b85"

        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: control.down
                       ? "#a9a9a4"
                       : (control.hovered ? "#f7f7f2" : "#e2e2dc")
            }

            GradientStop {
                position: 1.0
                color: control.down ? "#d5d5cf" : "#aaa9a3"
            }
        }
    }
}
