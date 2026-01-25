cmake_minimum_required(VERSION 3.20)
project(owui)

find_package(ECM 6.0.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMFindQmlModule)
include(ECMQmlModule)

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
)

ecm_target_qml_sources(owui
    SOURCES
    qml/Main.qml
    qml/WallpaperGrid.qml
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
)

install(TARGETS owui ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS org.openwallpaper.ui.desktop DESTINATION ${KDE_INSTALL_APPDIR})

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

