#ifndef OWUI_PREVIEW_PROVIDER_H
#define OWUI_PREVIEW_PROVIDER_H

#include <QQuickImageProvider>
#include "wallpaper_list.h"

class PreviewProvider : public QQuickImageProvider {
public:
    PreviewProvider(WallpaperList* list);
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

private:
    WallpaperList* mList;
};

#endif
