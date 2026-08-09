import QtQuick

Item {
    id: root
    property string title: "SIGNAL"
    property real value: 0
    property real minimumValue: 0
    property real maximumValue: 100
    property string valueText: ""
    property color needleColor: "#22221f"

    implicitWidth: 128
    implicitHeight: 128

    onValueChanged: canvas.requestPaint()
    onMinimumValueChanged: canvas.requestPaint()
    onMaximumValueChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            const cx = width / 2
            const cy = height * 0.86
            const radius = Math.min(width * 0.42, height * 0.72)
            const start = Math.PI * 1.15
            const end = Math.PI * 1.85

            ctx.fillStyle = "#deddd2"
            ctx.fillRect(0, 0, width, height)
            ctx.strokeStyle = "#77776f"
            ctx.lineWidth = 1
            ctx.strokeRect(0.5, 0.5, width - 1, height - 1)

            ctx.fillStyle = "#24241f"
            ctx.font = "600 12px sans-serif"
            ctx.textAlign = "center"
            ctx.textBaseline = "middle"
            ctx.fillText(root.title, cx, 19)

            ctx.strokeStyle = "#34342f"
            ctx.fillStyle = "#34342f"
            for (let i = 0; i <= 10; ++i) {
                const angle = start + (end - start) * i / 10
                const outerX = cx + Math.cos(angle) * radius
                const outerY = cy + Math.sin(angle) * radius
                const tickLength = i % 5 === 0 ? 13 : 8
                const innerX = cx + Math.cos(angle) * (radius - tickLength)
                const innerY = cy + Math.sin(angle) * (radius - tickLength)
                ctx.lineWidth = i % 5 === 0 ? 1.5 : 1
                ctx.beginPath()
                ctx.moveTo(innerX, innerY)
                ctx.lineTo(outerX, outerY)
                ctx.stroke()

                if (i % 2 === 0) {
                    const labelX = cx + Math.cos(angle) * (radius - 26)
                    const labelY = cy + Math.sin(angle) * (radius - 26)
                    ctx.font = "10px sans-serif"
                    ctx.fillText(String(i), labelX, labelY)
                }
            }

            const safeSpan = Math.max(0.0001, root.maximumValue - root.minimumValue)
            const normalized = Math.max(0, Math.min(1, (root.value - root.minimumValue) / safeSpan))
            const needleAngle = start + (end - start) * normalized
            const nx = cx + Math.cos(needleAngle) * (radius - 15)
            const ny = cy + Math.sin(needleAngle) * (radius - 15)

            ctx.strokeStyle = root.needleColor
            ctx.lineWidth = 2.5
            ctx.beginPath()
            ctx.moveTo(cx, cy)
            ctx.lineTo(nx, ny)
            ctx.stroke()

            ctx.fillStyle = "#4a4a45"
            ctx.beginPath()
            ctx.arc(cx, cy, 6, 0, Math.PI * 2)
            ctx.fill()

            if (root.valueText.length > 0) {
                ctx.fillStyle = "#292924"
                ctx.font = "600 11px sans-serif"
                ctx.fillText(root.valueText, cx, height - 10)
            }
        }
    }
}
