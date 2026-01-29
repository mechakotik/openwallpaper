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
#include "src/runner.h"
#include "src/wallpaper_list.h"

int main(int argc, char* argv[]) {
    KIconTheme::initTheme();
    QApplication app(argc, argv);

    QApplication::setStyle(QStringLiteral("breeze"));
    QIcon::setThemeName(QStringLiteral("breeze"));
    if(qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextObject(new KLocalizedContext(&engine));

    DisplayList displayList;
    WallpaperList wallpaperList;
    Runner runner;
    engine.rootContext()->setContextProperty(QStringLiteral("displayList"), &displayList);
    engine.rootContext()->setContextProperty(QStringLiteral("wallpaperList"), &wallpaperList);
    engine.rootContext()->setContextProperty(QStringLiteral("runner"), &runner);

    engine.loadFromModule("org.openwallpaper.ui", "Main");
    if(engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
