#include "toolchain_checker.h"
#include <QProcess>
#include <QRegularExpression>

ToolchainChecker::ToolchainChecker(QObject* parent) : QObject(parent) {
    check();
}

void ToolchainChecker::check() {
    checkWallpaperd();
}

void ToolchainChecker::checkWallpaperd() {
    mWallpaperdValid = false;
    mWallpaperdVersion = "";

    QString wallpaperdPath = mSettings.value("wallpaperdPath").toString();
    if(wallpaperdPath.isEmpty()) {
        wallpaperdPath = "wallpaperd";
    }

    QProcess process;
    process.start(wallpaperdPath, {"--version"});
    process.waitForFinished();
    if(process.exitCode() == 0) {
        mWallpaperdValid = true;
    }

    if(mWallpaperdValid) {
        QByteArray outBytes = process.readAllStandardOutput();
        QString outStr = QString::fromUtf8(outBytes);
        outStr.chop(outStr.endsWith("\n"));
        QStringList words = outStr.split(QRegularExpression("[ \\n]+"), Qt::SkipEmptyParts);
        if(words.size() < 2 || words[0] != "wallpaperd") {
            mWallpaperdValid = false;
        } else {
            mWallpaperdVersion = words[1];
        }
    }

    Q_EMIT wallpaperdValidChanged();
    Q_EMIT wallpaperdVersionChanged();
}
