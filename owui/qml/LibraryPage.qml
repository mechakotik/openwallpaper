import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigami.layouts as KirigamiLayouts

Kirigami.Page {
    id: libraryPage

    property var mainWindow: null
    property var optionsPageComponent: null

    signal gridReady(var grid)

    title: "OpenWallpaper UI"

    KirigamiLayouts.ColumnView.fillWidth: true

    padding: 0
    topPadding: 0
    bottomPadding: 0
    leftPadding: 0
    rightPadding: 0

    actions: [
        Kirigami.Action {
            icon.source: "qrc:/icons/add.svg"
            icon.color: Kirigami.Theme.textColor
            text: "Add wallpaper"
            displayHint: Kirigami.DisplayHint.KeepVisible
            onTriggered: {
                if (mainWindow) mainWindow.showPassiveNotification("Add wallpaper")
            }
        },
        Kirigami.Action {
            icon.source: "qrc:/icons/settings.svg"
            icon.color: Kirigami.Theme.textColor
            text: "Options"
            displayHint: Kirigami.DisplayHint.KeepVisible
            onTriggered: {
                if (mainWindow && optionsPageComponent) {
                    mainWindow.pageStack.layers.push(optionsPageComponent)
                }
            }
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

    Component.onCompleted: {
        gridReady(wallpaperGrid)
        wallpaperGrid.forceActiveFocus()
    }
}
