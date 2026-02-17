#include "runner.h"
#include <QDebug>
#include <QFile>
#include <QProcess>
#include <QThread>

Runner::Runner(QObject* parent) : QObject(parent) {
    mWorker = new RunnerWorker();
    mWorker->moveToThread(&mWorkerThread);
    connect(this, &Runner::runRequested, mWorker, &RunnerWorker::run, Qt::QueuedConnection);
    connect(mWorker, &RunnerWorker::finished, this, &Runner::onWorkerFinished, Qt::QueuedConnection);
    mWorkerThread.start();

    if(mSettings.contains("autorunWallpapers")) {
        mAutorunWallpapers = mSettings.value("autorunWallpapers").toMap();
    }
}

Runner::~Runner() {
    mWorkerThread.quit();
    mWorkerThread.wait();
}

void Runner::run(const QString& path, const QString& display) {
    if(mWorking) {
        return;
    }
    mWorking = true;

    mAutorunWallpapers.insert(display, path);
    mSettings.setValue("autorunWallpapers", mAutorunWallpapers);

    Q_EMIT workingChanged();
    Q_EMIT runRequested(path, display);
}

void Runner::autorun() {
    QProcess::execute("killall", {"wallpaperd"});
    for(const auto& [display, path] : mAutorunWallpapers.asKeyValueRange()) {
        mWorker->run(path.toString(), display);
    }
}

void Runner::onWorkerFinished() {
    mWorking = false;
    Q_EMIT workingChanged();
}
