#include "wallpaper_list_item.h"

WallpaperListItem::WallpaperListItem(QObject* parent) : QObject(parent) {}

void WallpaperListItem::setName(const QString& name) {
    mName = name;
    nameChanged();
}

void WallpaperListItem::setPath(const QString& path) {
    mPath = path;
    pathChanged();
}
