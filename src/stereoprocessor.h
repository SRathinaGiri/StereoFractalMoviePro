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

    // 360-degree VR modes: "vr360_tb" (stereo, left eye on top, right
    // below) and "vr360_mono" (the left eye alone). The frames are
    // equirectangular panoramas (2:1).
    static bool is360(const QString &mode);

    // The stereo window of a 360 panorama: no crop (that would break the
    // seam where the panorama meets itself behind the viewer) but the left
    // eye's picture turned around by `offset` pixels, wrapping round
    // (negative: inward, as applyStereoWindow).
    static QPair<QImage, QImage> applyStereoWindow360(const QImage &imgL,
                                                      const QImage &imgR,
                                                      int offset);

    // add3DText for a 360 panorama: the title above and the credit below
    // the horizon in front of the viewer (the middle of the picture), not
    // at its top and bottom edges (the poles of the sphere).
    static void add3DText360(QImage &imgL,
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
