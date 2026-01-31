import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami

FocusScope {
    id: root

    property var model: null
    property int selectedIndex: -1

    property int tileW: Kirigami.Units.gridUnit * 10
    property real previewAspect: 16 / 9
    property int tileInnerPad: Kirigami.Units.smallSpacing
    property int tileGapMin: Kirigami.Units.largeSpacing

    property real previewH: (tileW - tileInnerPad * 2) / previewAspect
    property real tileH: Math.ceil(
        tileInnerPad + previewH + Kirigami.Units.smallSpacing + Kirigami.Units.gridUnit * 1.4 + tileInnerPad
    )

    focus: true

    function clamp(v, lo, hi) {
        return Math.max(lo, Math.min(hi, v))
    }

    function ensureIndexVisible(i) {
        const it = gridRepeater.itemAt(i)
        if (!it) return

        const p = it.mapToItem(flick.contentItem, 0, 0)
        const x = p.x
        const y = p.y
        const w = it.width
        const h = it.height

        if (y < flick.contentY) flick.contentY = y
        else if (y + h > flick.contentY + flick.height) flick.contentY = y + h - flick.height

        if (x < flick.contentX) flick.contentX = x
        else if (x + w > flick.contentX + flick.width) flick.contentX = x + w - flick.width
    }

    function moveSelection(delta) {
        const n = gridRepeater.count
        if (n <= 0) return

        let idx = selectedIndex
        if (idx < 0) idx = 0

        const next = clamp(idx + delta, 0, n - 1)
        if (next !== selectedIndex) {
            selectedIndex = next
            ensureIndexVisible(next)
        }
    }

    function handleNavKey(key) {
        const n = gridRepeater.count
        if (n <= 0) return false

        const cols = Math.max(1, flick.columns)
        let idx = selectedIndex

        if (idx < 0) {
            if (key === Qt.Key_End) idx = n - 1
            else idx = 0
            selectedIndex = idx
            ensureIndexVisible(idx)
            return true
        }

        const row = Math.floor(idx / cols)
        const rows = Math.ceil(n / cols)

        if (key === Qt.Key_Left) {
            moveSelection(-1)
            return true
        } else if (key === Qt.Key_Right) {
            moveSelection(1)
            return true
        } else if (key === Qt.Key_Up) {
            if (row > 0) moveSelection(-cols)
            return true
        } else if (key === Qt.Key_Down) {
            if (row < rows - 1) moveSelection(cols)
            return true
        } else if (key === Qt.Key_Home) {
            selectedIndex = 0
            ensureIndexVisible(0)
            return true
        } else if (key === Qt.Key_End) {
            selectedIndex = n - 1
            ensureIndexVisible(n - 1)
            return true
        }

        return false
    }

    Keys.onPressed: (event) => {
        if (handleNavKey(event.key)) {
            event.accepted = true
        }
    }

    Controls.ScrollView {
        id: scroll
        anchors.fill: parent
        padding: 0
        clip: true

        background: Rectangle { color: Kirigami.Theme.backgroundColor }

        Controls.ScrollBar.vertical.policy: Controls.ScrollBar.AlwaysOff
        Controls.ScrollBar.horizontal.policy: Controls.ScrollBar.AlwaysOff

        Flickable {
            id: flick
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            contentWidth: width
            contentHeight: contentWrap.height

            property int columns: Math.max(1,
                Math.floor((width + root.tileGapMin) / (root.tileW + root.tileGapMin))
            )
            property real gap: Math.max(root.tileGapMin,
                (width - columns * root.tileW) / (columns + 1)
            )

            Item {
                id: contentWrap
                width: flick.width
                height: grid.implicitHeight + 2 * flick.gap

                GridLayout {
                    id: grid
                    anchors.fill: parent
                    anchors.margins: flick.gap

                    columns: flick.columns
                    columnSpacing: flick.gap
                    rowSpacing: flick.gap

                    Repeater {
                        id: gridRepeater
                        model: root.model

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
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: root.previewH
                                    radius: Kirigami.Units.smallSpacing
                                    color: Kirigami.Theme.alternateBackgroundColor
                                    border.width: 0
                                    clip: true

                                    Controls.Label {
                                        anchors.centerIn: parent
                                        text: "Preview"
                                        color: tileItem.pressed
                                               ? Kirigami.Theme.highlightedTextColor
                                               : Kirigami.Theme.textColor
                                    }
                                }

                                Controls.Label {
                                    Layout.fillWidth: true
                                    text: (modelData && modelData.name) ? modelData.name : "Wallpaper"
                                    elide: Text.ElideRight
                                    horizontalAlignment: Text.AlignHCenter
                                    color: tileItem.pressed
                                           ? Kirigami.Theme.highlightedTextColor
                                           : Kirigami.Theme.textColor
                                }
                            }

                            MouseArea {
                                id: mouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onPressed: root.forceActiveFocus()
                                onClicked: {
                                    root.selectedIndex = tileItem.index
                                    root.ensureIndexVisible(tileItem.index)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
