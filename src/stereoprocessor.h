#ifndef STEREOPROCESSOR_H
#define STEREOPROCESSOR_H

#include <QString>
#include <QList>
#include <QPair>
#include <QImage>

struct FramePair {
    QString leftPath;
    QString rightPath;
    int frameNumber;
};

class StereoProcessor {
public:
    static int extractFrameNumber(const QString &filename);
    static QList<FramePair> loadFramePairs(const QString &folder,
                                           int &originalWidth,
                                           int &originalHeight,
                                           QString &errorMsg,
                                           QString &warningMsg);

    static QPair<QImage, QImage> applyStereoWindow(const QImage &imgL,
                                                   const QImage &imgR,
                                                   int offset);

    static void add3DText(QImage &imgL,
                          QImage &imgR,
                          const QString &topStr,
                          const QString &bottomStr);

    static QImage combineStereo(const QImage &imgL,
                                const QImage &imgR,
                                const QString &mode);

    static QImage createPreview(const QImage &imgL,
                                const QImage &imgR,
                                const QString &mode,
                                int windowOffset,
                                bool showGuide,
                                double guidePosition,
                                int maxW = 700,
                                int maxH = 320);
};

#endif // STEREOPROCESSOR_H
