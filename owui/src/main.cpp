#include <KIconTheme>
#include <KLocalizedContext>
#include <KLocalizedString>
#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QUrl>
#include <QtQml>
#include "src/display_list.h"
#include "src/options_manager.h"
#include "src/preview_provider.h"
#include "src/runner.h"
#include "src/wallpaper_list.h"

int main(int argc, char* argv[]) {
    KIconTheme::initTheme();
    QApplication app(argc, argv);

    QApplication::setStyle("breeze");
    QIcon::setThemeName("breeze");
    if(qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle("org.kde.desktop");
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextObject(new KLocalizedContext(&engine));

    DisplayList displayList;
    WallpaperList wallpaperList;
    Runner runner;
    OptionsManager optionsManager;

    engine.rootContext()->setContextProperty("displayList", &displayList);
    engine.rootContext()->setContextProperty("wallpaperList", &wallpaperList);
    engine.rootContext()->setContextProperty("runner", &runner);
    engine.rootContext()->setContextProperty("optionsManager", &optionsManager);
    engine.addImageProvider("preview", new PreviewProvider(&wallpaperList));

    engine.loadFromModule("org.openwallpaper.ui", "Main");
    if(engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
