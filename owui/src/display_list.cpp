#include "display_list.h"
#include <QProcess>

DisplayList::DisplayList(QObject* parent) : QObject(parent) {
    QProcess proc;
    proc.setProgram(QStringLiteral("wallpaperd"));
    proc.setArguments({QStringLiteral("--list-displays")});
    proc.start();

    if(!proc.waitForStarted()) {
        qWarning() << "failed to start wallpaperd --list-displays";
        return;
    }

    proc.waitForFinished();
    QByteArray outBytes = proc.readAllStandardOutput();
    QString outStr = QString::fromUtf8(outBytes);
    outStr.chop(outStr.endsWith(QStringLiteral("\n")));

    mDisplays = outStr.split(QStringLiteral("\n"));
    displaysChanged();
}
