#ifndef OWUI_WALLPAPER_LIST_ITEM_H
#define OWUI_WALLPAPER_LIST_ITEM_H

#include <QObject>

class WallpaperListItem : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name MEMBER mName NOTIFY nameChanged)
    Q_PROPERTY(QString path MEMBER mPath NOTIFY pathChanged)
    Q_PROPERTY(QString previewID MEMBER mPreviewID NOTIFY previewIDChanged)
    Q_PROPERTY(QString description MEMBER mDescription NOTIFY descriptionChanged)

public:
    WallpaperListItem(QObject* parent = nullptr);
    void setName(const QString& name);
    void setPath(const QString& path);
    void setPreviewID(const QString& id);
    void setDescription(const QString& description);

Q_SIGNALS:
    void nameChanged();
    void pathChanged();
    void previewIDChanged();
    void descriptionChanged();

private:
    QString mName;
    QString mPath;
    QString mPreviewID;
    QString mDescription;
};

#endif
