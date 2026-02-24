#ifndef OWUI_TOOLCHAIN_CHECKER_H
#define OWUI_TOOLCHAIN_CHECKER_H

#include <QObject>
#include <QSettings>

class ToolchainChecker : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool wallpaperdValid MEMBER mWallpaperdValid NOTIFY wallpaperdValidChanged)
    Q_PROPERTY(QString wallpaperdVersion MEMBER mWallpaperdVersion NOTIFY wallpaperdVersionChanged)

public:
    ToolchainChecker(QObject* parent = nullptr);

    Q_INVOKABLE void check();

Q_SIGNALS:
    void wallpaperdValidChanged();
    void wallpaperdVersionChanged();

private:
    void checkWallpaperd();

    QSettings mSettings;
    bool mWallpaperdValid = false;
    QString mWallpaperdVersion;
};

#endif
