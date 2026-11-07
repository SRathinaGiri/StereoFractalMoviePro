#ifndef VIDEOWORKER_H
#define VIDEOWORKER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <atomic>
#include "stereoprocessor.h"

struct RenderOptions {
    QList<FramePair> framePairs;
    QString folderPath;
    QString audioPath;
    QStringList modes;
    int windowOffset = 0;
    bool doReverse = true;
    bool useText = true;
    QString topText;
    QString bottomText;
    int startDur = 5;
    int endDur = 5;
    double speedFactor = 1.0;
    int crf = 23;
    QString ffmpegPath;
};

class VideoWorker : public QObject {
    Q_OBJECT
public:
    explicit VideoWorker(const RenderOptions &options, QObject *parent = nullptr);
    void cancel();

public slots:
    void process();

signals:
    void progress(int percentage, const QString &statusText);
    void finished(bool success, const QString &message);

private:
    void render();
    static QString ffmpegErrors(const QByteArray &stderrBytes);

    RenderOptions m_options;
    std::atomic<bool> m_cancelled{false};
};

#endif // VIDEOWORKER_H
