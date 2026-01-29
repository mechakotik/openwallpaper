import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

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
        }

        FormCard.FormSwitchDelegate {
            text: "VSync"
        }

        FormCard.FormSpinBoxDelegate {
            label: "FPS cap"
            from: 0
            to: 720
            value: 30
        }

        FormCard.FormSwitchDelegate {
            text: "Prefer discrete GPU"
        }

        FormCard.FormSwitchDelegate {
            text: "Pause when wallpaper is hidden"
        }

        FormCard.FormSwitchDelegate {
            text: "Pause when laptop is on battery power"
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
        }

        FormCard.FormComboBoxDelegate {
            text: "Audio backend"
            model: ["Default", "PipeWire", "PulseAudio", "PortAudio"]
            currentIndex: 0
        }

        FormCard.FormTextFieldDelegate {
            label: "Audio source"
            placeholderText: "Default"
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
        }

        FormCard.FormTextFieldDelegate {
            label: "WASM C compiler path"
            placeholderText: "/opt/wasi-sdk/bin/clang"
        }

        FormCard.FormTextFieldDelegate {
            label: "glslc path"
            placeholderText: "glslc"
        }

        FormCard.FormTextFieldDelegate {
            label: "Wallpaper Engine assets path"
            placeholderText: "~/.local/share/Steam/steamapps/common/wallpaper_engine/assets"
        }

        FormCard.FormSwitchDelegate {
            text: "Automatically import wallpapers from library"
        }

        FormCard.FormTextFieldDelegate {
            label: "Wallpaper Engine library path"
            placeholderText: "~/.local/share/Steam/steamapps/workshop/content/431960"
        }
    }
}
