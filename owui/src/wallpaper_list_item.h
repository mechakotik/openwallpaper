#ifndef OWUI_WALLPAPER_LIST_ITEM_H
#define OWUI_WALLPAPER_LIST_ITEM_H

#include <QObject>

class WallpaperListItem : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name MEMBER mName NOTIFY nameChanged)
    Q_PROPERTY(QString path MEMBER mPath NOTIFY pathChanged)

public:
    WallpaperListItem(QObject* parent = nullptr);
    void setName(const QString& name);
    void setPath(const QString& path);

Q_SIGNALS:
    void nameChanged();
    void pathChanged();

private:
    QString mName;
    QString mPath;
};

#endif
