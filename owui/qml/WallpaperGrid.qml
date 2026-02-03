import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import Qt5Compat.GraphicalEffects as Effects

FocusScope {
    id: root

    property var model: null
    property int selectedIndex: 0

    property int tileW: Kirigami.Units.gridUnit * 10
    property real previewAspect: 16 / 9
    property int tileInnerPad: Kirigami.Units.smallSpacing
    property int tileGapMin: Kirigami.Units.largeSpacing

    property real previewH: (tileW - tileInnerPad * 2) / previewAspect
    property int tileH: Math.ceil(
        tileInnerPad + previewH + Kirigami.Units.smallSpacing + Kirigami.Units.gridUnit * 1.4 + tileInnerPad
    )

    focus: true

    function clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)) }

    function normalizeSelection() {
        const n = gridRepeater.count
        if (n <= 0) return
        if (selectedIndex < 0 || selectedIndex >= n) selectedIndex = 0
        ensureIndexVisible(selectedIndex)
    }

    function ensureIndexVisible(i) {
        const n = gridRepeater.count
        if (n <= 0) return
        i = clamp(i, 0, n - 1)

        const flick = scroll.contentItem
        const viewW = scroll.availableWidth
        const viewH = scroll.availableHeight

        const cols = Math.max(1, contentWrap.columns)
        const col = i % cols
        const row = Math.floor(i / cols)

        const hGap = contentWrap.hGap
        const vGap = contentWrap.vGap
        const edgeH = contentWrap.edgeH

        const x = edgeH + col * (root.tileW + hGap)
        const y = vGap + row * (root.tileH + vGap)
        const w = root.tileW
        const h = root.tileH
        const maxY = Math.max(0, contentWrap.contentH - viewH)

        let newY = flick.contentY

        if (y < newY + vGap) {
            newY = y - vGap
        } else if (y + h > newY + viewH - vGap) {
            newY = y + h + vGap - viewH
        }

        newY = clamp(newY, 0, maxY)

        if (y < newY) {
            newY = y
        } else if (y + h > newY + viewH) {
            newY = y + h - viewH
        }

        flick.contentY = clamp(newY, 0, maxY)

        const maxX = Math.max(0, contentWrap.contentW - viewW)
        let newX = flick.contentX

        if (x < newX) newX = x
        else if (x + w > newX + viewW) newX = x + w - viewW

        flick.contentX = clamp(newX, 0, maxX)
    }

    function setSelectionAndReveal(next) {
        const n = gridRepeater.count
        if (n <= 0) return

        next = clamp(next, 0, n - 1)
        if (next === selectedIndex) return

        ensureIndexVisible(next)
        selectedIndex = next
    }

    function moveSelection(delta) {
        const n = gridRepeater.count
        if (n <= 0) return
        setSelectionAndReveal(selectedIndex + delta)
    }

    function handleNavKey(key) {
        const n = gridRepeater.count
        if (n <= 0) return false

        const cols = Math.max(1, contentWrap.columns)
        const idx = selectedIndex
        const row = Math.floor(idx / cols)
        const rows = Math.ceil(n / cols)

        if (key === Qt.Key_Left) { setSelectionAndReveal(idx - 1); return true }
        if (key === Qt.Key_Right) { setSelectionAndReveal(idx + 1); return true }
        if (key === Qt.Key_Up) { if (row > 0) setSelectionAndReveal(idx - cols); return true }
        if (key === Qt.Key_Down) { if (row < rows - 1) setSelectionAndReveal(idx + cols); return true }
        if (key === Qt.Key_Home) { setSelectionAndReveal(0); return true }
        if (key === Qt.Key_End) { setSelectionAndReveal(n - 1); return true }

        return false
    }

    Keys.onPressed: (event) => {
        if (handleNavKey(event.key)) event.accepted = true
    }

    Controls.ScrollView {
        id: scroll
        anchors.fill: parent
        padding: 0
        clip: true

        background: Rectangle { color: Kirigami.Theme.backgroundColor }

        contentWidth: contentWrap.contentW
        contentHeight: contentWrap.contentH

        Controls.ScrollBar.horizontal.policy: Controls.ScrollBar.AlwaysOff

        Item {
            id: contentWrap
            width: scroll.availableWidth

            readonly property int vGap: root.tileGapMin

            readonly property int columns: Math.max(1,
                Math.floor(scroll.availableWidth / (root.tileW + root.tileGapMin))
            )

            readonly property real hGap: Math.max(root.tileGapMin,
                (scroll.availableWidth / columns) - root.tileW
            )

            readonly property real edgeH: hGap / 2

            readonly property int rows: gridRepeater.count > 0 ? Math.ceil(gridRepeater.count / columns) : 0

            readonly property real contentH: rows > 0
                ? (2 * vGap + rows * root.tileH + Math.max(0, rows - 1) * vGap)
                : 0

            readonly property real contentW: scroll.availableWidth

            implicitHeight: contentH
            height: contentH

            onColumnsChanged: {
                if (gridRepeater.count > 0) root.ensureIndexVisible(root.selectedIndex)
            }

            GridLayout {
                id: grid

                width: parent.width
                height: implicitHeight

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top

                anchors.leftMargin: contentWrap.edgeH
                anchors.rightMargin: contentWrap.edgeH
                anchors.topMargin: contentWrap.vGap

                columns: contentWrap.columns
                columnSpacing: contentWrap.hGap
                rowSpacing: contentWrap.vGap

                Repeater {
                    id: gridRepeater
                    model: root.model
                    onCountChanged: root.normalizeSelection()

                    Item {
                        id: tileItem
                        required property int index
                        required property var modelData

                        Layout.preferredWidth: root.tileW
                        Layout.preferredHeight: root.tileH

                        property bool hovered: mouse.containsMouse
                        property bool selected: root.selectedIndex === tileItem.index
                        property bool pressed: mouse.pressed

                        Rectangle {
                            anchors.fill: parent
                            radius: Kirigami.Units.smallSpacing
                            visible: tileItem.pressed
                            color: Kirigami.Theme.highlightColor
                            opacity: 0.85
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: Kirigami.Units.smallSpacing
                            visible: tileItem.selected && !tileItem.pressed
                            color: Kirigami.Theme.highlightColor
                            opacity: 0.18
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: Kirigami.Units.smallSpacing
                            visible: tileItem.hovered || tileItem.selected || tileItem.pressed
                            color: "transparent"
                            border.width: 2
                            border.color: Kirigami.Theme.highlightColor
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: root.tileInnerPad
                            spacing: Kirigami.Units.smallSpacing

                            Rectangle {
                                id: previewBox
                                Layout.fillWidth: true
                                Layout.preferredHeight: root.previewH
                                radius: Kirigami.Units.smallSpacing
                                color: Kirigami.Theme.alternateBackgroundColor
                                clip: true

                                Rectangle {
                                    id: previewMask
                                    anchors.fill: parent
                                    radius: previewBox.radius
                                    visible: false
                                }

                                Image {
                                    id: previewImg
                                    anchors.fill: parent
                                    fillMode: Image.PreserveAspectCrop
                                    cache: true
                                    sourceSize.width: width
                                    sourceSize.height: height
                                    source: (modelData && modelData.previewID && modelData.previewID.length > 0)
                                        ? ("image://preview/" + modelData.previewID)
                                        : ""
                                    layer.enabled: true
                                    layer.effect: Effects.OpacityMask { maskSource: previewMask }
                                }

                                Controls.Label {
                                    anchors.centerIn: parent
                                    visible: previewImg.source === "" || previewImg.status !== Image.Ready
                                    text: "No preview"
                                    color: tileItem.pressed ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                                }
                            }

                            Controls.Label {
                                Layout.fillWidth: true
                                text: (modelData && modelData.name) ? modelData.name : "Wallpaper"
                                elide: Text.ElideRight
                                horizontalAlignment: Text.AlignHCenter
                                color: tileItem.pressed ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                            }
                        }

                        MouseArea {
                            id: mouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onPressed: root.forceActiveFocus()
                            onClicked: root.setSelectionAndReveal(tileItem.index)
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: normalizeSelection()
}
