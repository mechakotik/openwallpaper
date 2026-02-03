cmake_minimum_required(VERSION 3.20)
project(owui)

set(CMAKE_CXX_STANDARD 23)

find_package(ECM 6.0.0 REQUIRED NO_MODULE)
find_package(KF6Archive ${KF_VERSION} REQUIRED)

set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMFindQmlModule)
include(ECMQmlModule)

add_subdirectory(external/toml11)
add_compile_options(-Wno-deprecated-literal-operator -fexceptions)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Quick
    Test
    Gui
    QuickControls2
    Widgets
)

find_package(KF6 REQUIRED COMPONENTS
    Kirigami
    I18n
    CoreAddons
    QQC2DesktopStyle
    IconThemes
)

ecm_find_qmlmodule(org.kde.kirigami REQUIRED)

add_executable(owui)

ecm_add_qml_module(owui
    URI
    org.openwallpaper.ui
)

target_sources(owui
    PRIVATE
    src/main.cpp
    src/display_list.cpp
    src/wallpaper_list.cpp
    src/wallpaper_list_item.cpp
    src/runner.cpp
    src/runner_worker.cpp
    src/preview_provider.cpp
)

ecm_target_qml_sources(owui
    SOURCES
    qml/Main.qml
    qml/LibraryPage.qml
    qml/SelectedWallpaperPage.qml
    qml/WallpaperGrid.qml
    qml/OptionsPage.qml
)

target_link_libraries(owui
    PRIVATE
    Qt6::Quick
    Qt6::Qml
    Qt6::Gui
    Qt6::QuickControls2
    Qt6::Widgets
    KF6::I18n
    KF6::CoreAddons
    KF6::IconThemes
    KF6::Archive
    toml11::toml11
)

install(TARGETS owui ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS org.openwallpaper.ui.desktop DESTINATION ${KDE_INSTALL_APPDIR})

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

