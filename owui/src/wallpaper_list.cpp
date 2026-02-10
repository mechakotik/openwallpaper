#include "wallpaper_list.h"
#include <kzip.h>
#include <qdir.h>
#include <qimage.h>
#include <qstandardpaths.h>
#include <exception>
#include <filesystem>
#include <toml.hpp>
#include "wallpaper_list_item.h"

WallpaperList::WallpaperList(QObject* parent) : QObject(parent) {
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dataPath);

    for(const auto& entry : std::filesystem::directory_iterator(dataPath.toStdString())) {
        if(entry.is_regular_file() && entry.path().filename().string().ends_with(".owf")) {
            WallpaperListItem* item = new WallpaperListItem(this);
            readWallpaperListItem(item, entry.path());
            mWallpapers.push_back(item);
        }
    }

    wallpapersChanged();
}

void WallpaperList::readWallpaperListItem(WallpaperListItem* item, const std::filesystem::path& path) {
    item->setName(QString::fromStdString(path.filename().string()));
    item->setPath(QString::fromStdString(path.string()));

    KZip zip(QString::fromStdString(path.string()));
    if(!zip.open(QIODevice::ReadOnly)) {
        qWarning() << "failed to open zip file" << path;
        return;
    }

    const KArchiveEntry* metaEntry = zip.directory()->entry("metadata.toml");
    if(metaEntry == nullptr) {
        return;
    }

    const KArchiveFile* metaFile = static_cast<const KArchiveFile*>(metaEntry);
    QByteArray metaBytes = metaFile->data();

    toml::value meta;
    try {
        meta = toml::parse_str(metaBytes.data());
    } catch(std::exception& e) {
        qWarning() << "failed to parse metadata.toml" << path << e.what();
        return;
    }

    if(meta.contains("info")) {
        const toml::value& info = meta.at("info");

        if(info.contains("name") && info.at("name").is_string()) {
            item->setName(QString::fromStdString(info.at("name").as_string()));
        }
        if(info.contains("description") && info.at("description").is_string()) {
            item->setDescription(QString::fromStdString(info.at("description").as_string()));
        }

        if(info.contains("preview") && info.at("preview").is_string()) {
            QString previewPath = QString::fromStdString(info.at("preview").as_string());
            const KArchiveEntry* previewEntry = zip.directory()->entry(previewPath);
            if(previewEntry != nullptr) {
                const KArchiveFile* previewFile = static_cast<const KArchiveFile*>(previewEntry);
                QByteArray previewBytes = previewFile->data();
                QImage previewImage;
                previewImage.loadFromData(previewBytes);
                if(!previewImage.isNull()) {
                    QString pathQStr = QString::fromStdString(path.string());
                    mPreviews.insert(pathQStr, previewImage);
                    item->setPreviewID(pathQStr);
                }
            }
        }
    }
}

QImage WallpaperList::getPreview(const QString& id) {
    return mPreviews.value(id);
}
