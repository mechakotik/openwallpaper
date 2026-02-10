import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard
import QtQuick.Controls as Controls

FormCard.FormCardPage {
    title: "Options"
    readonly property real maxWidth: Kirigami.Units.gridUnit * 36

    FormCard.FormHeader {
        maximumWidth: maxWidth
        title: "Renderer"
    }

    FormCard.FormCard {
        maximumWidth: maxWidth

        FormCard.FormTextFieldDelegate {
            label: "wallpaperd path"
            placeholderText: "wallpaperd"
            text: optionsManager.wallpaperdPath
            onEditingFinished: optionsManager.wallpaperdPath = text
            trailing: RowLayout {
                Item {
                    Layout.preferredWidth: Kirigami.Units.smallSpacing
                    Layout.preferredHeight: 1
                }
                Kirigami.Icon {
                    source: "qrc:/icons/check.svg"
                    implicitWidth: Kirigami.Units.iconSizes.smallMedium
                    implicitHeight: Kirigami.Units.iconSizes.smallMedium
                }
                Controls.Label {
                    text: "0.1.0"
                    color: Kirigami.Theme.textColor
                }
            }
        }

        FormCard.FormSwitchDelegate {
            text: "V-Sync"
            checked: optionsManager.vSync
            onCheckedChanged: optionsManager.vSync = checked
            leading: Kirigami.Icon {
                source: "qrc:/icons/vsync.svg"
                color: Kirigami.Theme.textColor
                implicitWidth: Kirigami.Units.iconSizes.smallMedium
                implicitHeight: Kirigami.Units.iconSizes.smallMedium
            }
        }

        FormCard.FormSpinBoxDelegate {
            label: "FPS limit"
            from: 0
            to: 720
            value: optionsManager.fpsLimit
            onValueChanged: optionsManager.fpsLimit = value
            enabled: !optionsManager.vSync
        }

        FormCard.FormSwitchDelegate {
            text: "Prefer discrete GPU"
            checked: optionsManager.preferDiscreteGPU
            onCheckedChanged: optionsManager.preferDiscreteGPU = checked
            leading: Kirigami.Icon {
                source: "qrc:/icons/chip.svg"
                color: Kirigami.Theme.textColor
                implicitWidth: Kirigami.Units.iconSizes.smallMedium
                implicitHeight: Kirigami.Units.iconSizes.smallMedium
            }
        }

        FormCard.FormSwitchDelegate {
            text: "Pause when wallpaper is hidden"
            checked: optionsManager.pauseHidden
            onCheckedChanged: optionsManager.pauseHidden = checked
            leading: Kirigami.Icon {
                source: "qrc:/icons/hidden.svg"
                color: Kirigami.Theme.textColor
                implicitWidth: Kirigami.Units.iconSizes.smallMedium
                implicitHeight: Kirigami.Units.iconSizes.smallMedium
            }
        }

        FormCard.FormSwitchDelegate {
            text: "Pause when laptop is on battery power"
            checked: optionsManager.pauseOnBat
            onCheckedChanged: optionsManager.pauseOnBat = checked
            leading: Kirigami.Icon {
                source: "qrc:/icons/battery.svg"
                color: Kirigami.Theme.textColor
                implicitWidth: Kirigami.Units.iconSizes.smallMedium
                implicitHeight: Kirigami.Units.iconSizes.smallMedium
            }
        }
    }

    FormCard.FormHeader {
        maximumWidth: maxWidth
        title: "Audio visualization"
    }

    FormCard.FormCard {
        maximumWidth: maxWidth

        FormCard.FormSwitchDelegate {
            text: "Enable audio visualization"
            checked: optionsManager.audioVisualization
            onCheckedChanged: optionsManager.audioVisualization = checked
            leading: Kirigami.Icon {
                source: "qrc:/icons/audio.svg"
                color: Kirigami.Theme.textColor
                implicitWidth: Kirigami.Units.iconSizes.smallMedium
                implicitHeight: Kirigami.Units.iconSizes.smallMedium
            }
        }

        FormCard.FormComboBoxDelegate {
            text: "Audio backend"
            model: ["Default", "PipeWire", "PulseAudio", "PortAudio"]
            currentIndex: optionsManager.audioBackend
            onCurrentIndexChanged: optionsManager.audioBackend = currentIndex
            enabled: optionsManager.audioVisualization
        }

        FormCard.FormTextFieldDelegate {
            label: "Audio source"
            placeholderText: "Default"
            text: optionsManager.audioSource
            onEditingFinished: optionsManager.audioSource = text
            enabled: optionsManager.audioVisualization
        }
    }

    FormCard.FormHeader {
        maximumWidth: maxWidth
        title: "Wallpaper Engine converter"
    }

    FormCard.FormCard {
        maximumWidth: maxWidth

        FormCard.FormTextFieldDelegate {
            label: "wpe-compile path"
            placeholderText: "wpe-compile"
            text: optionsManager.wpeCompilePath
            onEditingFinished: optionsManager.wpeCompilePath = text
        }

        FormCard.FormTextFieldDelegate {
            label: "WASM C compiler path"
            placeholderText: "/opt/wasi-sdk/bin/clang"
            text: optionsManager.wasmCCPath
            onEditingFinished: optionsManager.wasmCCPath = text
        }

        FormCard.FormTextFieldDelegate {
            label: "glslc path"
            placeholderText: "glslc"
            text: optionsManager.glslcPath
            onEditingFinished: optionsManager.glslcPath = text
        }

        FormCard.FormTextFieldDelegate {
            label: "Wallpaper Engine assets path"
            placeholderText: "~/.local/share/Steam/steamapps/common/wallpaper_engine/assets"
            text: optionsManager.wpeAssetsPath
            onEditingFinished: optionsManager.wpeAssetsPath = text
        }

        FormCard.FormSwitchDelegate {
            text: "Automatically import wallpapers from library"
            checked: optionsManager.wpeAutoImport
            onCheckedChanged: optionsManager.wpeAutoImport = checked
            leading: Kirigami.Icon {
                source: "qrc:/icons/wallpaper.svg"
                color: Kirigami.Theme.textColor
                implicitWidth: Kirigami.Units.iconSizes.smallMedium
                implicitHeight: Kirigami.Units.iconSizes.smallMedium
            }
        }

        FormCard.FormTextFieldDelegate {
            label: "Wallpaper Engine library path"
            placeholderText: "~/.local/share/Steam/steamapps/workshop/content/431960"
            text: optionsManager.wpeLibraryPath
            onEditingFinished: optionsManager.wpeLibraryPath = text
        }
    }
}
