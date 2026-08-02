import QtQuick
import QtQuick.Controls

Button {
    id: control
    implicitWidth: 116
    implicitHeight: 42
    font.pixelSize: 14
    font.bold: true

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.enabled ? "#202020" : "#777772"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        fontSizeMode: Text.Fit
        minimumPixelSize: 9
        wrapMode: Text.NoWrap
        maximumLineCount: 1
        clip: true
    }

    background: Rectangle {
        radius: 2
        border.width: 1
        border.color: control.down ? "#555550" : "#8b8b85"
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: control.down ? "#a9a9a4" : (control.hovered ? "#f7f7f2" : "#e2e2dc")
            }
            GradientStop {
                position: 1.0
                color: control.down ? "#d5d5cf" : "#aaa9a3"
            }
        }
    }
}
