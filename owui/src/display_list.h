#ifndef OWUI_DISPLAY_LIST_H
#define OWUI_DISPLAY_LIST_H

#include <QQuickItem>

class DisplayList : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList displays MEMBER mDisplays NOTIFY displaysChanged)

public:
    DisplayList(QObject* parent = nullptr);

Q_SIGNALS:
    void displaysChanged();

private:
    QStringList mDisplays;
};

#endif
