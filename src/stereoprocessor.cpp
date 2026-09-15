#include "stereoprocessor.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QPainter>
#include <QFontMetrics>
#include <algorithm>
#include <cmath>

int StereoProcessor::extractFrameNumber(const QString &filename) {
    QString base = QFileInfo(filename).fileName();
    QRegularExpression re(QStringLiteral("\\d+"));
    QRegularExpressionMatchIterator it = re.globalMatch(base);
    QString lastMatch;
    while (it.hasNext()) {
        lastMatch = it.next().captured(0);
    }
    if (!lastMatch.isEmpty()) {
        return lastMatch.toInt();
    }
    return 0;
}

QList<FramePair> StereoProcessor::loadFramePairs(const QString &folder,
                                                 int &originalWidth,
                                                 int &originalHeight,
                                                 QString &errorMsg,
                                                 QString &warningMsg) {
    originalWidth = 0;
    originalHeight = 0;
    errorMsg.clear();
    warningMsg.clear();

    QDir dir(folder);
    if (!dir.exists()) {
        errorMsg = QStringLiteral("Folder does not exist.");
        return {};
    }

    QStringList leftFilters = {
        QStringLiteral("*Left*.jpg"), QStringLiteral("*Left*.jpeg"),
        QStringLiteral("*Left*.png"), QStringLiteral("*left*.jpg"),
        QStringLiteral("*left*.jpeg"), QStringLiteral("*left*.png")
    };
    QStringList rightFilters = {
        QStringLiteral("*Right*.jpg"), QStringLiteral("*Right*.jpeg"),
        QStringLiteral("*Right*.png"), QStringLiteral("*right*.jpg"),
        QStringLiteral("*right*.jpeg"), QStringLiteral("*right*.png")
    };

    QFileInfoList leftList = dir.entryInfoList(leftFilters, QDir::Files | QDir::Readable);
    QFileInfoList rightList = dir.entryInfoList(rightFilters, QDir::Files | QDir::Readable);

    // Deduplicate by canonical path
    auto deduplicate = [](QFileInfoList &list) {
        QFileInfoList unique;
        QSet<QString> seen;
        for (const QFileInfo &fi : list) {
            QString path = fi.absoluteFilePath();
            if (!seen.contains(path)) {
                seen.insert(path);
                unique.append(fi);
            }
        }
        list = unique;
    };

    deduplicate(leftList);
    deduplicate(rightList);

    auto sortFunc = [](const QFileInfo &a, const QFileInfo &b) {
        int na = extractFrameNumber(a.fileName());
        int nb = extractFrameNumber(b.fileName());
        if (na != nb) return na < nb;
        return a.fileName() < b.fileName();
    };

    std::sort(leftList.begin(), leftList.end(), sortFunc);
    std::sort(rightList.begin(), rightList.end(), sortFunc);

    if (leftList.isEmpty() || rightList.isEmpty()) {
        errorMsg = QStringLiteral("Could not find Left/Right JPG/PNG images in the selected folder.");
        return {};
    }

    if (leftList.size() != rightList.size()) {
        warningMsg = QString(QStringLiteral("Frame Count Difference:\nLeft frames: %1\nRight frames: %2\n\nUsing %3 pairs."))
                         .arg(leftList.size())
                         .arg(rightList.size())
                         .arg(std::min(leftList.size(), rightList.size()));
    }

    int count = std::min(leftList.size(), rightList.size());
    QList<FramePair> pairs;
    pairs.reserve(count);

    for (int i = 0; i < count; ++i) {
        FramePair pair;
        pair.leftPath = leftList[i].absoluteFilePath();
        pair.rightPath = rightList[i].absoluteFilePath();
        pair.frameNumber = extractFrameNumber(leftList[i].fileName());
        pairs.append(pair);
    }

    QImage first(pairs[0].leftPath);
    if (first.isNull()) {
        errorMsg = QStringLiteral("Could not read first frame: ") + pairs[0].leftPath;
        return {};
    }

    originalWidth = first.width();
    originalHeight = first.height();
    return pairs;
}

QPair<QImage, QImage> StereoProcessor::applyStereoWindow(const QImage &imgL,
                                                         const QImage &imgR,
                                                         int offset) {
    if (imgL.isNull() || imgR.isNull()) {
        return qMakePair(imgL, imgR);
    }

    int width = std::min(imgL.width(), imgR.width());
    int height = std::min(imgL.height(), imgR.height());
    height = height - (height % 2);

    if (offset == 0) {
        int evenW = width - (width % 2);
        QImage croppedL = (imgL.width() == evenW && imgL.height() == height)
                              ? imgL : imgL.copy(0, 0, evenW, height);
        QImage croppedR = (imgR.width() == evenW && imgR.height() == height)
                              ? imgR : imgR.copy(0, 0, evenW, height);
        return qMakePair(croppedL, croppedR);
    }

    int amount = std::abs(offset);
    if (amount >= width - 1) {
        amount = width - 2;
    }

    if (amount <= 0) {
        int evenW = width - (width % 2);
        return qMakePair(imgL.copy(0, 0, evenW, height), imgR.copy(0, 0, evenW, height));
    }

    int croppedWidth = width - amount;
    croppedWidth = croppedWidth - (croppedWidth % 2);
    QImage outL, outR;

    if (offset < 0) {
        // Inward:
        // Left image  -> crop left edge (amount to end)
        // Right image -> crop right edge (0 to width - amount)
        outL = imgL.copy(amount, 0, croppedWidth, height);
        outR = imgR.copy(0, 0, croppedWidth, height);
    } else {
        // Outward:
        // Left image  -> crop right edge (0 to width - amount)
        // Right image -> crop left edge (amount to end)
        outL = imgL.copy(0, 0, croppedWidth, height);
        outR = imgR.copy(amount, 0, croppedWidth, height);
    }

    return qMakePair(outL, outR);
}

void StereoProcessor::add3DText(QImage &imgL,
                                QImage &imgR,
                                const QString &topStr,
                                const QString &bottomStr) {
    if (imgL.isNull() || imgR.isNull()) return;

    if (imgL.format() != QImage::Format_RGB888 && imgL.format() != QImage::Format_ARGB32_Premultiplied) {
        imgL = imgL.convertToFormat(QImage::Format_RGB888);
    }
    if (imgR.format() != QImage::Format_RGB888 && imgR.format() != QImage::Format_ARGB32_Premultiplied) {
        imgR = imgR.convertToFormat(QImage::Format_RGB888);
    }

    int width = imgL.width();
    int height = imgL.height();

    // Scale font size proportionally to image height (reference height ~ 1080 -> 28pt)
    int pointSize = std::max(12, int(height * 28.0 / 1080.0));
    QFont font(QStringLiteral("Arial"), pointSize, QFont::Bold);
    QFontMetrics fm(font);

    const int topOffset = 6;
    const int bottomOffset = -6;

    int topSizeW = fm.horizontalAdvance(topStr);
    int topX = (width - topSizeW) / 2;
    int topY = std::max(fm.height() + 10, int(height * 0.08));

    int bottomSizeW = fm.horizontalAdvance(bottomStr);
    int bottomX = (width - bottomSizeW) / 2;
    int bottomY = height - std::max(20, int(height * 0.05));

    auto drawText = [&](QImage &img, int tX, int tY, int bX, int bY) {
        QPainter painter(&img);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        painter.setFont(font);

        // Draw subtle dark drop shadow for maximum legibility on bright fractals
        painter.setPen(QColor(0, 0, 0, 180));
        painter.drawText(tX + 2, tY + 2, topStr);
        painter.drawText(bX + 2, bY + 2, bottomStr);

        painter.setPen(Qt::white);
        painter.drawText(tX, tY, topStr);
        painter.drawText(bX, bY, bottomStr);
        painter.end();
    };

    // Left image: top_x + top_offset, bottom_x + bottom_offset
    drawText(imgL, topX + topOffset, topY, bottomX + bottomOffset, bottomY);

    // Right image: top_x - top_offset, bottom_x - bottom_offset
    drawText(imgR, topX - topOffset, topY, bottomX - bottomOffset, bottomY);
}

QImage StereoProcessor::combineStereo(const QImage &imgL,
                                      const QImage &imgR,
                                      const QString &mode) {
    if (imgL.isNull() || imgR.isNull()) return {};

    int w = std::min(imgL.width(), imgR.width());
    int h = std::min(imgL.height(), imgR.height());

    if (mode == QStringLiteral("parallel")) {
        QImage out(w * 2, h, QImage::Format_RGB888);
        QPainter p(&out);
        p.drawImage(0, 0, imgL, 0, 0, w, h);
        p.drawImage(w, 0, imgR, 0, 0, w, h);
        p.end();
        return out;
    } else if (mode == QStringLiteral("cross")) {
        QImage out(w * 2, h, QImage::Format_RGB888);
        QPainter p(&out);
        p.drawImage(0, 0, imgR, 0, 0, w, h);
        p.drawImage(w, 0, imgL, 0, 0, w, h);
        p.end();
        return out;
    } else if (mode == QStringLiteral("color_anaglyph")) {
        QImage leftRGB = (imgL.format() == QImage::Format_RGB888) ? imgL : imgL.convertToFormat(QImage::Format_RGB888);
        QImage rightRGB = (imgR.format() == QImage::Format_RGB888) ? imgR : imgR.convertToFormat(QImage::Format_RGB888);

        QImage out(w, h, QImage::Format_RGB888);
        for (int y = 0; y < h; ++y) {
            const uchar *lScan = leftRGB.constScanLine(y);
            const uchar *rScan = rightRGB.constScanLine(y);
            uchar *outScan = out.scanLine(y);

            for (int x = 0; x < w; ++x) {
                // Qt Format_RGB888: Byte 0 is R, Byte 1 is G, Byte 2 is B
                // Red <- Left, Green <- Right, Blue <- Right
                outScan[x * 3 + 0] = lScan[x * 3 + 0];
                outScan[x * 3 + 1] = rScan[x * 3 + 1];
                outScan[x * 3 + 2] = rScan[x * 3 + 2];
            }
        }
        return out;
    } else if (mode == QStringLiteral("gray_anaglyph")) {
        QImage leftRGB = (imgL.format() == QImage::Format_RGB888) ? imgL : imgL.convertToFormat(QImage::Format_RGB888);
        QImage rightRGB = (imgR.format() == QImage::Format_RGB888) ? imgR : imgR.convertToFormat(QImage::Format_RGB888);

        QImage out(w, h, QImage::Format_RGB888);
        for (int y = 0; y < h; ++y) {
            const uchar *lScan = leftRGB.constScanLine(y);
            const uchar *rScan = rightRGB.constScanLine(y);
            uchar *outScan = out.scanLine(y);

            for (int x = 0; x < w; ++x) {
                uchar gl = static_cast<uchar>(0.299f * lScan[x * 3 + 0] +
                                              0.587f * lScan[x * 3 + 1] +
                                              0.114f * lScan[x * 3 + 2]);
                uchar gr = static_cast<uchar>(0.299f * rScan[x * 3 + 0] +
                                              0.587f * rScan[x * 3 + 1] +
                                              0.114f * rScan[x * 3 + 2]);

                outScan[x * 3 + 0] = gl;
                outScan[x * 3 + 1] = gr;
                outScan[x * 3 + 2] = gr;
            }
        }
        return out;
    }

    // Default to parallel
    QImage out(w * 2, h, QImage::Format_RGB888);
    QPainter p(&out);
    p.drawImage(0, 0, imgL, 0, 0, w, h);
    p.drawImage(w, 0, imgR, 0, 0, w, h);
    p.end();
    return out;
}

QImage StereoProcessor::createPreview(const QImage &imgL,
                                      const QImage &imgR,
                                      const QString &mode,
                                      int windowOffset,
                                      bool showGuide,
                                      double guidePosition,
                                      int maxW,
                                      int maxH) {
    if (imgL.isNull() || imgR.isNull()) return {};

    QPair<QImage, QImage> adjusted = applyStereoWindow(imgL, imgR, windowOffset);
    QImage combined = combineStereo(adjusted.first, adjusted.second, mode);
    if (combined.isNull()) return {};

    // Scale to preview bounding box preserving aspect ratio
    QImage preview = combined.scaled(maxW, maxH, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    if (showGuide) {
        int pW = preview.width();
        int pH = preview.height();
        double pos = std::max(0.0, std::min(1.0, guidePosition));

        QPainter p(&preview);
        QPen guidePen(QColor(255, 255, 0), 2);
        QPen dividerPen(QColor(255, 255, 255), 1);

        if (mode == QStringLiteral("parallel") || mode == QStringLiteral("cross")) {
            int half = pW / 2;
            int x1 = int(half * pos);
            int x2 = half + int(half * pos);

            p.setPen(guidePen);
            p.drawLine(x1, 0, x1, pH);
            p.drawLine(x2, 0, x2, pH);

            p.setPen(dividerPen);
            p.drawLine(half, 0, half, pH);
        } else {
            int x = int(pW * pos);
            p.setPen(guidePen);
            p.drawLine(x, 0, x, pH);
        }
        p.end();
    }

    return preview;
}
