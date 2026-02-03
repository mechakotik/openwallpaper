import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.kde.kirigami.layouts as KirigamiLayouts
import org.kde.kirigamiaddons.formcard as FormCard
import Qt5Compat.GraphicalEffects as Effects

Kirigami.Page {
    id: selectedWallpaperPage

    property var mainWindow: null
    property var wallpaperGridObj: null

    property int previewColumnWidth: Kirigami.Units.gridUnit * 24
    property string selectedWallpaperPath: ""
    property string selectedWallpaperName: ""
    property string selectedPreviewID: ""
    property string selectedWallpaperDescription: ""

    property string selectedDisplay: ""

    title: ""

    KirigamiLayouts.ColumnView.fillWidth: false
    KirigamiLayouts.ColumnView.minimumWidth: previewColumnWidth
    KirigamiLayouts.ColumnView.maximumWidth: previewColumnWidth

    padding: Kirigami.Units.gridUnit * 0.75

    actions: [
        Kirigami.Action {
            displayHint: Kirigami.DisplayHint.KeepVisible
            displayComponent: Component {
                RowLayout {
                    spacing: Kirigami.Units.smallSpacing

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
                        onCurrentTextChanged: selectedWallpaperPage.selectedDisplay = currentText
                        Component.onCompleted: selectedWallpaperPage.selectedDisplay = currentText
                    }
                }
            }
        },
        Kirigami.Action {
            icon.name: "media-playback-start"
            text: "Run wallpaper"
            enabled: !runner.working
            onTriggered: {
                runner.run(selectedWallpaperPath, selectedDisplay)
                if (wallpaperGridObj) wallpaperGridObj.forceActiveFocus()
            }
        }
    ]

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.largeSpacing

        Item {
            id: previewHolder
            Layout.fillWidth: true
            Layout.preferredHeight: Math.round(previewHolder.width / (16 / 9))

            Rectangle {
                id: largePreviewBox
                anchors.fill: parent
                radius: Kirigami.Units.smallSpacing
                color: Kirigami.Theme.alternateBackgroundColor
                clip: true

                Rectangle {
                    id: largePreviewMask
                    anchors.fill: parent
                    radius: largePreviewBox.radius
                    visible: false
                }

                Image {
                    id: largePreviewImg
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectCrop
                    cache: true
                    sourceSize.width: width
                    sourceSize.height: height
                    source: selectedPreviewID.length > 0 ? ("image://preview/" + selectedPreviewID) : ""
                    layer.enabled: true
                    layer.effect: Effects.OpacityMask { maskSource: largePreviewMask }
                }

                Controls.Label {
                    anchors.centerIn: parent
                    visible: largePreviewImg.source === "" || largePreviewImg.status !== Image.Ready
                    text: largePreviewImg.status === Image.Loading ? "Loading..." : "No preview"
                    color: Kirigami.Theme.textColor
                }
            }
        }

        Kirigami.Heading {
            Layout.fillWidth: true
            text: selectedWallpaperName.length > 0 ? selectedWallpaperName : "Wallpaper"
            level: 2
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        Controls.ScrollView {
            Layout.fillWidth: true
            Layout.maximumHeight: Kirigami.Units.gridUnit * 10

            Controls.TextArea {
                text: selectedWallpaperDescription
                wrapMode: Text.Wrap
                textFormat: Text.RichText
                readOnly: true
            }
        }

        FormCard.FormHeader {
            maximumWidth: Kirigami.Units.gridUnit * 21.5
            title: "General options"
        }

        FormCard.FormCard {
            maximumWidth: Kirigami.Units.gridUnit * 21.5

            FormCard.AbstractFormDelegate {
                background: null

                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Controls.Label {
                        text: "Speed"
                        Layout.fillWidth: true
                        wrapMode: Text.NoWrap
                        elide: Text.ElideRight
                    }

                    Controls.Slider {
                        Layout.fillWidth: true
                        from: 0.1
                        to: 3.0
                        value: 1.0
                        stepSize: 0
                    }
                }
            }

            FormCard.FormButtonDelegate {
                icon.name: "utilities-terminal-symbolic"
                text: "Custom commands"
                onClicked: {
                    if (mainWindow) mainWindow.showPassiveNotification("Custom commands")
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
