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
        id: initPage

        Kirigami.Page {
            id: page
            title: "OpenWallpaper UI"

            padding: 0
            topPadding: 0
            bottomPadding: 0
            leftPadding: 0
            rightPadding: 0

            actions: [
                Kirigami.Action {
                    icon.name: "add"
                    text: "Add wallpaper"
                    onTriggered: showPassiveNotification("Add wallpaper")
                },
                Kirigami.Action {
                    icon.name: "settings"
                    text: "Options"
                    onTriggered: showPassiveNotification("Options")
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

                Rectangle {
                    id: bottomPanel
                    Layout.fillWidth: true
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 2.6
                    color: Kirigami.Theme.alternateBackgroundColor

                    RowLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: Kirigami.Units.largeSpacing
                        anchors.rightMargin: Kirigami.Units.largeSpacing

                        height: implicitHeight
                        spacing: Kirigami.Units.largeSpacing

                        Item { Layout.fillWidth: true }

                        Controls.Label {
                            text: "Display"
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Controls.ComboBox {
                            model: displayList.displays
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Controls.Button {
                            text: "Run wallpaper"
                            enabled: wallpaperGrid.selectedIndex >= 0
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: showPassiveNotification("Set as wallpaper: " + wallpaperGrid.selectedIndex)
                        }
                    }
                }
            }
        }
    }
}
