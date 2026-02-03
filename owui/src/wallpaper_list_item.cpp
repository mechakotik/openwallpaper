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

void WallpaperListItem::setPreviewID(const QString& id) {
    mPreviewID = id;
    previewIDChanged();
}

void WallpaperListItem::setDescription(const QString& description) {
    mDescription = description;
    descriptionChanged();
}
