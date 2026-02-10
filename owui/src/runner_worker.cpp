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
    pgrep.setProgram("pgrep");
    pgrep.setArguments({"-f", cmd});
    pgrep.start();
    if(!pgrep.waitForStarted()) {
        qWarning() << "failed to start pgrep";
        return {};
    }

    pgrep.waitForFinished();
    QByteArray outBytes = pgrep.readAllStandardOutput();
    QString outStr = QString::fromUtf8(outBytes);
    outStr.chop(outStr.endsWith("\n"));
    QStringList pids = outStr.split("\n");

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
    QString wallpaperdPath = mSettings.value("wallpaperdPath").toString();
    if(wallpaperdPath.isEmpty()) {
        wallpaperdPath = "wallpaperd";
    }

    QStringList args;
    args.append("--owui-tag");
    args.append(QString("--display=%1").arg(display));
    if(!mSettings.value("vSync").toBool()) {
        args.append(QString("--fps=%1").arg(mSettings.value("fpsLimit").toInt()));
    }
    if(mSettings.value("preferDiscreteGPU").toBool()) {
        args.append("--prefer-dgpu");
    }
    if(mSettings.value("pauseHidden").toBool()) {
        args.append("--pause-hidden");
    }
    if(mSettings.value("pauseOnBat").toBool()) {
        args.append("--pause-on-bat");
    }
    if(!mSettings.value("audioVisualization").toBool()) {
        args.append("--no-audio");
    } else {
        switch(mSettings.value("audioBackend").toInt()) {
            case 1:
                args.append("--pipewire");
                break;
            case 2:
                args.append("--pulse");
                break;
            case 3:
                args.append("--portaudio");
                break;
            default:
                break;
        }
        QString audioSource = mSettings.value("audioSource").toString();
        if(!audioSource.isEmpty()) {
            args.append(QString("--audio-source=%1").arg(mSettings.value("audioSource").toString()));
        }
    }
    args.append(path);

    QStringList pids = getRunningPids(display);
    QProcess wallpaperd;
    wallpaperd.setProgram(wallpaperdPath);
    wallpaperd.setArguments(args);

    qint64 pid = 0;
    wallpaperd.startDetached(&pid);
    QString readyFile = QString("/tmp/wallpaperd-%1.ready").arg(pid);

    while(true) {
        if(QFile::exists(readyFile)) {
            break;
        }
        QThread::msleep(100);
    }

    for(const QString& pidStr : pids) {
        int64_t pid = pidStr.toLongLong();
        if(kill(pid, SIGTERM) != 0 && errno != ESRCH) {
            qWarning() << QString("failed to kill wallpaperd (errno %1)").arg(errno);
        }
    }

    Q_EMIT finished();
}
