#ifndef OWUI_WALLPAPER_LIST_H
#define OWUI_WALLPAPER_LIST_H

#include <QHash>
#include <QImage>
#include <QObject>
#include <filesystem>
#include "src/wallpaper_list_item.h"

class WallpaperList : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObjectList wallpapers MEMBER mWallpapers NOTIFY wallpapersChanged)

public:
    WallpaperList(QObject* parent = nullptr);
    QImage getPreview(const QString& id);

Q_SIGNALS:
    void wallpapersChanged();

private:
    void readWallpaperListItem(WallpaperListItem* item, const std::filesystem::path& path);

    QObjectList mWallpapers;
    QHash<QString, QImage> mPreviews;
};

#endif
