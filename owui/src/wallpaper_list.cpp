#include "wallpaper_list.h"
#include <qdir.h>
#include <qstandardpaths.h>
#include <filesystem>
#include "src/wallpaper_list_item.h"

WallpaperList::WallpaperList(QObject* parent) : QObject(parent) {
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dataPath);

    for(const auto& entry : std::filesystem::directory_iterator(dataPath.toStdString())) {
        if(entry.is_regular_file()) {
            WallpaperListItem* item = new WallpaperListItem(this);
            item->setName(QString::fromStdString(entry.path().filename().string()));
            item->setPath(QString::fromStdString(entry.path().string()));
            mWallpapers.push_back(item);
        }
    }

    wallpapersChanged();
}
