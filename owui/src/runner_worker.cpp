#include "runner_worker.h"
#include <QDebug>
#include <QFile>
#include <QProcess>
#include <QThread>
#include <csignal>

RunnerWorker::RunnerWorker(QObject* parent) : QObject(parent) {}

static QStringList getRunningPids(const QString& display) {
    QString cmd = QString::fromUtf8("wallpaperd --owui-tag --display=%1").arg(display);
    QProcess pgrep;
    pgrep.setProgram(QStringLiteral("pgrep"));
    pgrep.setArguments({QStringLiteral("-f"), cmd});
    pgrep.start();
    if(!pgrep.waitForStarted()) {
        qWarning() << "failed to start pgrep";
        return {};
    }

    pgrep.waitForFinished();
    QByteArray outBytes = pgrep.readAllStandardOutput();
    QString outStr = QString::fromUtf8(outBytes);
    outStr.chop(outStr.endsWith(QStringLiteral("\n")));
    QStringList pids = outStr.split(QStringLiteral("\n"));

    QStringList res;
    for(const QString& pid : pids) {
        QString trimmed = pid.trimmed();
        if(!trimmed.isEmpty()) {
            res.append(trimmed);
        }
    }
    return res;
}

void RunnerWorker::run(QString path, QString display) {
    QStringList pids = getRunningPids(display);

    QProcess wallpaperd;
    wallpaperd.setProgram(QStringLiteral("wallpaperd"));
    wallpaperd.setArguments({
        QStringLiteral("--owui-tag"),
        QStringLiteral("--display=%1").arg(display),
        QStringLiteral("--fps=30"),
        QStringLiteral("--pause-hidden"),
        path,
    });

    qint64 pid = 0;
    wallpaperd.startDetached(&pid);
    QString readyFile = QStringLiteral("/tmp/wallpaperd-%1.ready").arg(pid);

    while(true) {
        if(QFile::exists(readyFile)) {
            break;
        }
        QThread::msleep(100);
    }

    for(const QString& pidStr : pids) {
        int64_t pid = pidStr.toLongLong();
        if(kill(pid, SIGTERM) != 0 && errno != ESRCH) {
            qWarning() << QStringLiteral("failed to kill wallpaperd (errno %1)").arg(errno);
        }
    }

    Q_EMIT finished();
}
