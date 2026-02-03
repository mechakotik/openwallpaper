import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.kirigami.layouts as KirigamiLayouts

Kirigami.ApplicationWindow {
    id: root

    title: "OpenWallpaper UI"

    minimumWidth: Kirigami.Units.gridUnit * 20
    minimumHeight: Kirigami.Units.gridUnit * 20
    width: Kirigami.Units.gridUnit * 60
    height: minimumHeight

    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.ToolBar
    pageStack.columnView.columnResizeMode: KirigamiLayouts.ColumnView.DynamicColumns

    readonly property int previewColumnWidth: Kirigami.Units.gridUnit * 24

    property var wallpaperGridObj: null

    property int selectedIndex: wallpaperGridObj ? wallpaperGridObj.selectedIndex : 0
    property var selectedWallpaper: (wallpaperList.wallpapers && wallpaperList.wallpapers.length > 0)
        ? wallpaperList.wallpapers[Math.min(selectedIndex, wallpaperList.wallpapers.length - 1)]
        : ({})
    property string selectedWallpaperPath: selectedWallpaper.path ? selectedWallpaper.path : ""
    property string selectedWallpaperName: selectedWallpaper.name ? selectedWallpaper.name : ""
    property string selectedPreviewID: selectedWallpaper.previewID ? selectedWallpaper.previewID : ""
    property string selectedWallpaperDescription: selectedWallpaper.description ? selectedWallpaper.description : ""

    Component {
        id: optionsPage
        OptionsPage {}
    }

    Keys.priority: Keys.AfterItem
    Keys.onPressed: (event) => {
        if (root.wallpaperGridObj && root.wallpaperGridObj.handleNavKey(event.key)) {
            event.accepted = true
        }
    }

    Component {
        id: selectedWallpaperPageComponent

        SelectedWallpaperPage {
            id: selectedWallpaperPage
            mainWindow: root
            previewColumnWidth: root.previewColumnWidth
            selectedWallpaperPath: root.selectedWallpaperPath
            selectedWallpaperName: root.selectedWallpaperName
            selectedPreviewID: root.selectedPreviewID
            selectedWallpaperDescription: root.selectedWallpaperDescription
            wallpaperGridObj: root.wallpaperGridObj
        }
    }

    Component {
        id: libraryPageComponent

        LibraryPage {
            id: libraryPage
            mainWindow: root
            optionsPageComponent: optionsPage
            onGridReady: root.wallpaperGridObj = grid
        }
    }

    pageStack.initialPage: libraryPageComponent

    Component.onCompleted: {
        pageStack.push(selectedWallpaperPageComponent)
        pageStack.currentIndex = 0
        Qt.callLater(() => {
            if (root.wallpaperGridObj) root.wallpaperGridObj.forceActiveFocus()
        })
    }
}
