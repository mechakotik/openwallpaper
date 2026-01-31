import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root

    title: "OpenWallpaper UI"

    minimumWidth: Kirigami.Units.gridUnit * 20
    minimumHeight: Kirigami.Units.gridUnit * 20
    width: minimumWidth
    height: minimumHeight

    pageStack.initialPage: initPage

    Component {
        id: optionsPage
        OptionsPage {}
    }

    Component {
        id: initPage

        Kirigami.Page {
            id: page

            title: ""

            padding: 0
            topPadding: 0
            bottomPadding: 0
            leftPadding: 0
            rightPadding: 0

            property alias selectedIndex: wallpaperGrid.selectedIndex
            property string selectedWallpaperPath: selectedIndex >= 0 ? wallpaperList.wallpapers[selectedIndex].path : ""
            property string selectedDisplay: ""

            Keys.priority: Keys.AfterItem
            Keys.onPressed: (event) => {
                if (wallpaperGrid.handleNavKey(event.key)) {
                    event.accepted = true
                }
            }

            Kirigami.Action {
                id: runAction
                icon.name: "media-playback-start"
                text: "Run wallpaper"
                enabled: !runner.working && page.selectedIndex >= 0
                onTriggered: {
                    runner.run(page.selectedWallpaperPath, page.selectedDisplay)
                    wallpaperGrid.forceActiveFocus()
                }
            }

            titleDelegate: RowLayout {
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Icon {
                    source: "monitor-symbolic"
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                    Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                }

                Controls.ComboBox {
                    id: displaySelector
                    model: displayList.displays
                    Layout.alignment: Qt.AlignVCenter
                    onCurrentTextChanged: page.selectedDisplay = currentText
                    Component.onCompleted: page.selectedDisplay = currentText
                }

                Controls.ToolButton {
                    action: runAction
                    display: Controls.AbstractButton.TextBesideIcon
                    Layout.alignment: Qt.AlignVCenter
                }
            }

            actions: [
                Kirigami.Action {
                    icon.name: "list-add"
                    text: "Add wallpaper"
                    displayHint: Kirigami.DisplayHint.KeepVisible
                    onTriggered: showPassiveNotification("Add wallpaper")
                },
                Kirigami.Action {
                    icon.name: "settings"
                    text: "Options"
                    displayHint: Kirigami.DisplayHint.KeepVisible
                    onTriggered: root.pageStack.layers.push(optionsPage)
                }
            ]

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                WallpaperGrid {
                    id: wallpaperGrid
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: wallpaperList.wallpapers
                }
            }
        }
    }
}
