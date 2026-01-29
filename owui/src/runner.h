#ifndef OWUI_RUNNER_H
#define OWUI_RUNNER_H

#include <QObject>
#include <QThread>
#include "src/runner_worker.h"

class Runner : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool working MEMBER mWorking NOTIFY workingChanged)

public:
    Runner(QObject* parent = nullptr);
    ~Runner();
    Q_INVOKABLE void run(const QString& path, const QString& display);

public Q_SLOTS:
    void onWorkerFinished();

Q_SIGNALS:
    void workingChanged();
    void runRequested(QString path, QString display);

private:
    RunnerWorker* mWorker = nullptr;
    QThread mWorkerThread;
    bool mWorking = false;
};

#endif
