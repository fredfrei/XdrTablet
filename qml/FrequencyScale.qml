import QtQuick

Item {
    id: root
    property int frequencyKhz: 87500
    property int minimumKhz: 87500
    property int maximumKhz: 108000
    property color scaleColor: "#d7d7ca"
    property color textColor: "#24241f"
    property color pointerColor: "#b66c27"

    implicitHeight: 190

    onFrequencyKhzChanged: canvas.requestPaint()
    onMinimumKhzChanged: canvas.requestPaint()
    onMaximumKhzChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            const x0 = 38
            const x1 = width - 38
            const yTop = 42
            const yBase = height - 45
            const span = root.maximumKhz - root.minimumKhz

            ctx.fillStyle = root.scaleColor
            ctx.fillRect(0, 0, width, height)

            ctx.strokeStyle = "#77776f"
            ctx.lineWidth = 1
            ctx.strokeRect(0.5, 0.5, width - 1, height - 1)

            ctx.fillStyle = root.textColor
            ctx.font = "600 13px sans-serif"
            ctx.textAlign = "left"
            ctx.fillText("FM", 10, 25)
            ctx.textAlign = "right"
            ctx.fillText("MHz", width - 10, 25)

            ctx.strokeStyle = "#34342f"
            ctx.fillStyle = root.textColor
            ctx.lineWidth = 1
            ctx.textAlign = "center"
            ctx.textBaseline = "top"

            for (let khz = root.minimumKhz; khz <= root.maximumKhz; khz += 250) {
                const normalized = (khz - root.minimumKhz) / span
                const x = x0 + normalized * (x1 - x0)
                const isWhole = khz % 1000 === 0
                const isHalf = khz % 500 === 0
                const tick = isWhole ? 30 : (isHalf ? 20 : 11)

                ctx.beginPath()
                ctx.moveTo(x, yBase)
                ctx.lineTo(x, yBase - tick)
                ctx.stroke()

                const mhz = khz / 1000
                const shouldLabel = khz === root.minimumKhz || khz === root.maximumKhz ||
                                    (isWhole && Math.round(mhz) % 2 === 0)
                if (shouldLabel) {
                    ctx.font = isWhole ? "600 15px sans-serif" : "600 13px sans-serif"
                    const label = khz === root.minimumKhz ? mhz.toFixed(1) : mhz.toFixed(0)
                    ctx.fillText(label, x, yBase + 8)
                }
            }

            // Feine obere Hilfsskala wie bei einem klassischen Tuner.
            ctx.strokeStyle = "#6f6f67"
            for (let i = 0; i <= 40; ++i) {
                const x = x0 + i / 40 * (x1 - x0)
                const tick = i % 5 === 0 ? 10 : 5
                ctx.beginPath()
                ctx.moveTo(x, yTop)
                ctx.lineTo(x, yTop + tick)
                ctx.stroke()
            }

            const bounded = Math.max(root.minimumKhz, Math.min(root.maximumKhz, root.frequencyKhz))
            const pointerX = x0 + (bounded - root.minimumKhz) / span * (x1 - x0)

            ctx.strokeStyle = root.pointerColor
            ctx.lineWidth = 3
            ctx.beginPath()
            ctx.moveTo(pointerX, yTop - 7)
            ctx.lineTo(pointerX, yBase + 1)
            ctx.stroke()

            ctx.fillStyle = root.pointerColor
            ctx.beginPath()
            ctx.moveTo(pointerX - 8, yTop - 8)
            ctx.lineTo(pointerX + 8, yTop - 8)
            ctx.lineTo(pointerX, yTop + 2)
            ctx.closePath()
            ctx.fill()
        }
    }
}
