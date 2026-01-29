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
                            id: displaySelector
                            model: displayList.displays
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Controls.Button {
                            id: runButton
                            enabled: !runner.working && wallpaperGrid.selectedIndex >= 0
                            Layout.alignment: Qt.AlignVCenter
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 10
                            onClicked: runner.run(wallpaperList.wallpapers[wallpaperGrid.selectedIndex].path, displaySelector.currentText)

                            contentItem: Item {
                                anchors.fill: parent

                                Row {
                                    id: contentRow
                                    anchors.centerIn: parent
                                    spacing: Kirigami.Units.smallSpacing

                                    Loader {
                                        id: stateIcon
                                        width: Kirigami.Units.iconSizes.smallMedium
                                        height: Kirigami.Units.iconSizes.smallMedium
                                        anchors.verticalCenter: parent.verticalCenter
                                        sourceComponent: runner.working ? busyComp : playComp
                                    }

                                    Controls.Label {
                                        text: runner.working ? "Running wallpaper" : "Run wallpaper"
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                            }

                            Component {
                                id: playComp
                                Kirigami.Icon {
                                    source: "media-playback-start"
                                    width: Kirigami.Units.iconSizes.smallMedium
                                    height: Kirigami.Units.iconSizes.smallMedium
                                }
                            }

                            Component {
                                id: busyComp
                                Kirigami.Icon {
                                    source: "process-working"
                                    width: Kirigami.Units.iconSizes.smallMedium
                                    height: Kirigami.Units.iconSizes.smallMedium
                                    transformOrigin: Item.Center

                                    NumberAnimation on rotation {
                                        from: 0
                                        to: 360
                                        duration: 1500
                                        loops: Animation.Infinite
                                        running: true
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
