import QtQuick

Item {
    id: root
    property int frequencyKhz: 87500
    property int minimumKhz: 87500
    property int maximumKhz: 108000
    property color scaleColor: "#d7d7ca"
    property color textColor: "#24241f"
    property color pointerColor: "#b66c27"

    // Nur belegte Speicherplätze (> 0) werden gezeichnet.
    property var presetFrequenciesKhz: []
    property color presetPointerColor: "#725224"
    property color activePresetPointerColor: pointerColor

    signal presetActivated(int index, int frequencyKhz)

    implicitHeight: 190

    onFrequencyKhzChanged: canvas.requestPaint()
    onMinimumKhzChanged: canvas.requestPaint()
    onMaximumKhzChanged: canvas.requestPaint()
    onPresetFrequenciesKhzChanged: canvas.requestPaint()

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

            // Kurze Speicherzeiger unterhalb der oberen Hilfsskala.
            // Leere Speicherplätze (0) bleiben vollständig unsichtbar.
            for (let i = 0; i < root.presetFrequenciesKhz.length; ++i) {
                const presetKhz = Number(root.presetFrequenciesKhz[i])

                if (!Number.isFinite(presetKhz)
                        || presetKhz < root.minimumKhz
                        || presetKhz > root.maximumKhz) {
                    continue
                }

                const normalized =
                    (presetKhz - root.minimumKhz) / span
                const presetX = x0 + normalized * (x1 - x0)
                const active =
                    Math.abs(presetKhz - root.frequencyKhz) <= 25
                const markerTop = yTop -20
                const markerBottom = markerTop + (active ? 18 : 14)

                ctx.strokeStyle = active
                                  ? root.activePresetPointerColor
                                  : root.presetPointerColor
                ctx.fillStyle = ctx.strokeStyle
                ctx.lineWidth = active ? 3 : 2

                ctx.beginPath()
                ctx.moveTo(presetX, markerTop)
                ctx.lineTo(presetX, markerBottom)
                ctx.stroke()

                // Kleine nach unten zeigende Spitze wie bei älteren Tunern.
                ctx.beginPath()
                ctx.moveTo(presetX - 4, markerTop - 5)
                ctx.lineTo(presetX + 4, markerTop - 5)
                ctx.lineTo(presetX, markerTop + 1)
                ctx.closePath()
                ctx.fill()
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

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton

        onClicked: function(mouse) {
            const x0 = 38
            const x1 = width - 38
            const span = root.maximumKhz - root.minimumKhz

            if (span <= 0 || x1 <= x0)
                return

            let nearestIndex = -1
            let nearestDistance = 15

            for (let i = 0;
                 i < root.presetFrequenciesKhz.length;
                 ++i) {
                const presetKhz =
                    Number(root.presetFrequenciesKhz[i])

                if (!Number.isFinite(presetKhz)
                        || presetKhz < root.minimumKhz
                        || presetKhz > root.maximumKhz) {
                    continue
                }

                const presetX =
                    x0
                    + (presetKhz - root.minimumKhz)
                    / span * (x1 - x0)
                const distance = Math.abs(mouse.x - presetX)

                if (distance < nearestDistance) {
                    nearestDistance = distance
                    nearestIndex = i
                }
            }

            if (nearestIndex >= 0) {
                root.presetActivated(
                    nearestIndex,
                    Number(root.presetFrequenciesKhz[
                               nearestIndex]))
            }
        }
    }
}
