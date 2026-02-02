#include "preview_provider.h"

PreviewProvider::PreviewProvider(WallpaperList* list) : QQuickImageProvider(QQuickImageProvider::Image), mList(list) {}

QImage PreviewProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize) {
    QImage image = mList->getPreview(id);
    if(requestedSize.isValid() && !image.isNull()) {
        image = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    if(size != nullptr) {
        *size = image.size();
    }
    return image;
}
