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
    Q_EMIT workingChanged();
    Q_EMIT runRequested(path, display);
}

void Runner::onWorkerFinished() {
    mWorking = false;
    Q_EMIT workingChanged();
}
