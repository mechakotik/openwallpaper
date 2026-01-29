#ifndef RUNNER_WORKER_H
#define RUNNER_WORKER_H

#include <QObject>

class RunnerWorker : public QObject {
    Q_OBJECT

public:
    RunnerWorker(QObject* parent = nullptr);

public Q_SLOTS:
    void run(QString path, QString display);

Q_SIGNALS:
    void finished();
};

#endif
