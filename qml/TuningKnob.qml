import QtQuick

Item {
    id: root
    property bool enabled: true
    signal stepRequested(int direction)

    implicitWidth: 164
    implicitHeight: 164

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true
        opacity: root.enabled ? 1.0 : 0.55
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            const cx = width / 2
            const cy = height / 2
            const radius = Math.min(width, height) * 0.46

            ctx.strokeStyle = "#4c4c48"
            ctx.lineWidth = 1
            for (let i = 0; i < 48; ++i) {
                const angle = Math.PI * 2 * i / 48
                const x1 = cx + Math.cos(angle) * (radius - 3)
                const y1 = cy + Math.sin(angle) * (radius - 3)
                const x2 = cx + Math.cos(angle) * (radius - 12)
                const y2 = cy + Math.sin(angle) * (radius - 12)
                ctx.beginPath()
                ctx.moveTo(x1, y1)
                ctx.lineTo(x2, y2)
                ctx.stroke()
            }

            const gradient = ctx.createRadialGradient(cx - radius * 0.28,
                                                       cy - radius * 0.30,
                                                       radius * 0.08,
                                                       cx, cy, radius)
            gradient.addColorStop(0.0, "#f7f7f2")
            gradient.addColorStop(0.45, "#c9c9c3")
            gradient.addColorStop(0.75, "#a2a29c")
            gradient.addColorStop(1.0, "#666661")
            ctx.fillStyle = gradient
            ctx.beginPath()
            ctx.arc(cx, cy, radius - 13, 0, Math.PI * 2)
            ctx.fill()

            ctx.strokeStyle = "#777772"
            ctx.lineWidth = 2
            ctx.beginPath()
            ctx.arc(cx, cy, radius - 13, 0, Math.PI * 2)
            ctx.stroke()

            ctx.strokeStyle = "rgba(255,255,255,0.75)"
            ctx.lineWidth = 1
            ctx.beginPath()
            ctx.arc(cx - 2, cy - 2, radius - 22, Math.PI * 1.05, Math.PI * 1.75)
            ctx.stroke()
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        enabled: root.enabled
        hoverEnabled: true
        property real lastX: 0
        property real accumulated: 0

        onPressed: {
            lastX = mouse.x
            accumulated = 0
        }
        onPositionChanged: {
            if (!pressed)
                return
            accumulated += mouse.x - lastX
            lastX = mouse.x
            while (accumulated >= 12) {
                root.stepRequested(1)
                accumulated -= 12
            }
            while (accumulated <= -12) {
                root.stepRequested(-1)
                accumulated += 12
            }
        }
        onWheel: function(wheel) {
            root.stepRequested(wheel.angleDelta.y >= 0 ? 1 : -1)
            wheel.accepted = true
        }
        onClicked: {
            root.stepRequested(mouse.x >= width / 2 ? 1 : -1)
        }
    }
}
