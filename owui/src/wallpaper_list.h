#ifndef OWUI_WALLPAPER_LIST_H
#define OWUI_WALLPAPER_LIST_H

#include <QObject>

class WallpaperList : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObjectList wallpapers MEMBER mWallpapers NOTIFY wallpapersChanged)

public:
    WallpaperList(QObject* parent = nullptr);

Q_SIGNALS:
    void wallpapersChanged();

private:
    QObjectList mWallpapers;
};

#endif
