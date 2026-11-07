#include "videoworker.h"
#include "spherical.h"
#include <QProcess>
#include <QRegularExpression>
#include <new>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <cmath>
#include <algorithm>

VideoWorker::VideoWorker(const RenderOptions &options, QObject *parent)
    : QObject(parent), m_options(options) {}

// FFmpeg's error lines: its last few hundred characters are encoder
// statistics, which hid the actual cause
QString VideoWorker::ffmpegErrors(const QByteArray &stderrBytes) {
    const QStringList lines = QString::fromUtf8(stderrBytes).split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts);
    QStringList errors;
    for (const QString &line : lines) {
        if (line.contains(QStringLiteral("rror")) || line.contains(QStringLiteral("Cannot")) ||
            line.contains(QStringLiteral("Invalid")) || line.contains(QStringLiteral("failed"))) {
            errors << line.trimmed();
        }
    }
    if (errors.isEmpty()) return QString::fromUtf8(stderrBytes).right(400);
    return errors.mid(std::max<qsizetype>(0, errors.size() - 8)).join(QLatin1Char('\n'));
}

void VideoWorker::process() {
    // an allocation that fails (a frame is ~100 MB at 5760x5760) throws;
    // uncaught in this thread it would close the app without a word
    try {
        render();
    } catch (const std::bad_alloc &) {
        emit finished(false, QStringLiteral("Not enough memory to make the video. Close other programs "
                                            "(browsers, databases, virtual machines) and try again."));
    }
}

void VideoWorker::cancel() {
    m_cancelled.store(true);
}

void VideoWorker::render() {
    if (m_options.framePairs.isEmpty()) {
        emit finished(false, QStringLiteral("No frames available for processing."));
        return;
    }

    if (m_options.modes.isEmpty()) {
        emit finished(false, QStringLiteral("No output view modes selected."));
        return;
    }

    QString ffmpegExe = m_options.ffmpegPath;
    if (ffmpegExe.isEmpty()) {
        ffmpegExe = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    }
    if (ffmpegExe.isEmpty() && QFileInfo::exists(QStringLiteral("d:/utils/ffmpeg/ffmpeg.exe"))) {
        ffmpegExe = QStringLiteral("d:/utils/ffmpeg/ffmpeg.exe");
    }
    if (ffmpegExe.isEmpty()) {
        emit finished(false, QStringLiteral("ffmpeg.exe could not be found. Please ensure FFmpeg is in PATH or in d:\\utils\\ffmpeg\\."));
        return;
    }

    // Ping-pong sequence
    QList<FramePair> sequence = m_options.framePairs;
    if (m_options.doReverse && m_options.framePairs.size() > 2) {
        for (int i = m_options.framePairs.size() - 2; i > 0; --i) {
            sequence.append(m_options.framePairs[i]);
        }
    }

    double fps = 30.0 * m_options.speedFactor;
    if (fps <= 0.0) fps = 30.0;

    int totalFrames = sequence.size();
    double duration = (fps > 0.0) ? (static_cast<double>(totalFrames) / fps) : 0.0;
    int startThreshold = static_cast<int>(m_options.startDur * fps);
    int endThreshold = totalFrames - static_cast<int>(m_options.endDur * fps);

    // Determine dimensions from sample first frame
    QImage sampleL(sequence[0].leftPath);
    QImage sampleR(sequence[0].rightPath);
    if (sampleL.isNull() || sampleR.isNull()) {
        emit finished(false, QStringLiteral("Failed to read initial frame pair to determine video dimensions."));
        return;
    }

    QPair<QImage, QImage> sampleAdjusted = StereoProcessor::applyStereoWindow(sampleL, sampleR, m_options.windowOffset);
    int adjW = sampleAdjusted.first.width();
    int adjH = sampleAdjusted.first.height();

    int totalModes = m_options.modes.size();

    for (int modeIdx = 0; modeIdx < totalModes; ++modeIdx) {
        if (m_cancelled.load()) {
            emit finished(false, QStringLiteral("Processing cancelled by user."));
            return;
        }

        const QString &mode = m_options.modes[modeIdx];
        emit progress(0, QStringLiteral("Preparing %1...").arg(mode));

        const bool vr = StereoProcessor::is360(mode);
        // (a panorama is not cropped by the stereo window: its picture keeps
        // its size)
        int eyeW = vr ? sampleL.width() - (sampleL.width() % 2) : adjW;
        int eyeH = vr ? sampleL.height() - (sampleL.height() % 2) : adjH;
        int outW = (mode == QStringLiteral("parallel") || mode == QStringLiteral("cross")) ? (eyeW * 2) : eyeW;
        int outH = (mode == QStringLiteral("vr360_tb")) ? (eyeH * 2) : eyeH;
        outW = outW - (outW % 2);
        outH = outH - (outH % 2);

        // 360 players without the metadata go by the name: _360, _360_TB
        QString outputFilename = (mode == QStringLiteral("vr360_tb"))   ? QStringLiteral("vr360_final_360_TB.mp4")
                                 : (mode == QStringLiteral("vr360_mono")) ? QStringLiteral("vr360_mono_final_360.mp4")
                                                                        : QStringLiteral("%1_final.mp4").arg(mode);
        QString outputPath = QDir(m_options.folderPath).filePath(outputFilename);

        QStringList args;
        args << QStringLiteral("-y")
             << QStringLiteral("-f") << QStringLiteral("rawvideo")
             << QStringLiteral("-vcodec") << QStringLiteral("rawvideo")
             << QStringLiteral("-s") << QStringLiteral("%1x%2").arg(outW).arg(outH)
             << QStringLiteral("-pix_fmt") << QStringLiteral("rgb24")
             << QStringLiteral("-r") << QString::number(fps, 'f', 4)
             << QStringLiteral("-i") << QStringLiteral("-");

        bool hasAudio = !m_options.audioPath.isEmpty() && QFileInfo::exists(m_options.audioPath);
        if (hasAudio) {
            args << QStringLiteral("-i") << m_options.audioPath
                 << QStringLiteral("-map") << QStringLiteral("0:v:0")
                 << QStringLiteral("-map") << QStringLiteral("1:a:0")
                 << QStringLiteral("-c:v") << QStringLiteral("libx264")
                 << QStringLiteral("-crf") << QString::number(m_options.crf)
                 << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p")
                 << QStringLiteral("-c:a") << QStringLiteral("aac")
                 << QStringLiteral("-b:a") << QStringLiteral("192k")
                 << QStringLiteral("-af") << QStringLiteral("afade=t=out:st=%1:d=3").arg(std::max(0.0, duration - 3.0), 0, 'f', 2)
                 << QStringLiteral("-shortest");
        } else {
            args << QStringLiteral("-c:v") << QStringLiteral("libx264")
                 << QStringLiteral("-crf") << QString::number(m_options.crf)
                 << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p");
        }

        // x264's own threads and look-ahead hold dozens of frames: at
        // 5760x5760 that is ~15 GB (measured), more than a PC often has
        // spare, and FFmpeg then ends the video early. 6 threads and a
        // 10-frame look-ahead: ~5 GB, same speed, same file size.
        if (static_cast<qint64>(outW) * outH > 3840LL * 2160) {
            args << QStringLiteral("-threads") << QStringLiteral("6")
                 << QStringLiteral("-x264-params") << QStringLiteral("rc-lookahead=10");
        }

        if (mode == QStringLiteral("parallel") || mode == QStringLiteral("cross")) {
            args << QStringLiteral("-metadata:s:v:0") << QStringLiteral("stereo_mode=left_right");
        }

        args << outputPath;

        QProcess ffmpeg;
        ffmpeg.start(ffmpegExe, args);

        if (!ffmpeg.waitForStarted(5000)) {
            emit finished(false, QStringLiteral("Failed to start FFmpeg at: %1").arg(ffmpegExe));
            return;
        }

        int totalFrames = sequence.size();
        QByteArray packedBuffer;

        for (int frameIdx = 0; frameIdx < totalFrames; ++frameIdx) {
            if (m_cancelled.load()) {
                ffmpeg.kill();
                ffmpeg.waitForFinished(2000);
                QFile::remove(outputPath);
                emit finished(false, QStringLiteral("Processing cancelled by user."));
                return;
            }

            // FFmpeg can end before the last frame (out of memory, disk
            // full): stop and say why, rather than feed a closed pipe
            if (ffmpeg.state() != QProcess::Running) {
                emit finished(false, QStringLiteral("FFmpeg stopped after %1 of %2 frames of %3, so the video is incomplete.\n"
                                                    "If it says it could not allocate memory: close other programs and try again.\n\n%4")
                                         .arg(frameIdx).arg(totalFrames).arg(mode)
                                         .arg(ffmpegErrors(ffmpeg.readAllStandardError())));
                return;
            }

            const FramePair &pair = sequence[frameIdx];
            QImage imgL(pair.leftPath);
            QImage imgR(pair.rightPath);

            if (imgL.isNull() || imgR.isNull()) {
                continue;
            }

            QPair<QImage, QImage> adjusted = vr ? StereoProcessor::applyStereoWindow360(imgL, imgR, m_options.windowOffset)
                                                : StereoProcessor::applyStereoWindow(imgL, imgR, m_options.windowOffset);

            if (m_options.useText && (frameIdx < startThreshold || frameIdx >= endThreshold)) {
                if (vr) {
                    StereoProcessor::add3DText360(adjusted.first, adjusted.second, m_options.topText, m_options.bottomText);
                } else {
                    StereoProcessor::add3DText(adjusted.first, adjusted.second, m_options.topText, m_options.bottomText);
                }
            }

            QImage combined = StereoProcessor::combineStereo(adjusted.first, adjusted.second, mode);
            if (combined.format() != QImage::Format_RGB888) {
                combined = combined.convertToFormat(QImage::Format_RGB888);
            }
            if (combined.width() != outW || combined.height() != outH) {
                // (a frame of another size than the first would shear the video)
                combined = combined.scaled(outW, outH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            }

            int w = combined.width();
            int h = combined.height();
            int bytesPerRow = w * 3;
            qint64 totalFrameBytes = static_cast<qint64>(bytesPerRow) * h;

            const char *data = nullptr;
            if (combined.bytesPerLine() == bytesPerRow) {
                data = reinterpret_cast<const char *>(combined.constBits());
            } else {
                if (packedBuffer.size() != totalFrameBytes) {
                    packedBuffer.resize(totalFrameBytes);
                }
                char *dst = packedBuffer.data();
                for (int y = 0; y < h; ++y) {
                    memcpy(dst + static_cast<size_t>(y) * bytesPerRow,
                           combined.constScanLine(y),
                           bytesPerRow);
                }
                data = packedBuffer.constData();
            }

            qint64 bytesWritten = 0;
            while (bytesWritten < totalFrameBytes) {
                qint64 chunk = ffmpeg.write(data + bytesWritten, totalFrameBytes - bytesWritten);
                if (chunk < 0) {
                    emit finished(false, QStringLiteral("Error piping frame data to FFmpeg: %1").arg(ffmpeg.errorString()));
                    ffmpeg.kill();
                    return;
                }
                bytesWritten += chunk;
            }

            // Keep buffer bounded
            if (ffmpeg.bytesToWrite() > totalFrameBytes * 4) {
                ffmpeg.waitForBytesWritten(500);
            }

            if (frameIdx % 5 == 0 || frameIdx == totalFrames - 1) {
                int framePercent = static_cast<int>((static_cast<double>(frameIdx + 1) / totalFrames) * 100.0);
                int overallPercent = static_cast<int>(((static_cast<double>(modeIdx) + static_cast<double>(frameIdx + 1) / totalFrames) / totalModes) * 100.0);
                emit progress(overallPercent, QStringLiteral("Rendering %1: %2%").arg(mode).arg(framePercent));
            }
        }

        ffmpeg.closeWriteChannel();

        emit progress(static_cast<int>(((modeIdx + 1.0) / totalModes) * 100.0),
                      QStringLiteral("Finalizing %1 encode...").arg(mode));

        if (!ffmpeg.waitForFinished(-1)) {
            emit finished(false, QStringLiteral("FFmpeg encoding timed out or failed."));
            return;
        }

        if (ffmpeg.exitStatus() != QProcess::NormalExit || ffmpeg.exitCode() != 0) {
            emit finished(false, QStringLiteral("FFmpeg failed with exit code %1:\n%2").arg(ffmpeg.exitCode()).arg(ffmpegErrors(ffmpeg.readAllStandardError())));
            return;
        }

        if (vr) {
            // the Spherical Video V2 metadata: shown as a sphere (stereo
            // top-bottom or mono) by YouTube, Meta Quest and VLC
            emit progress(static_cast<int>(((modeIdx + 1.0) / totalModes) * 100.0),
                          QStringLiteral("Writing 360 metadata..."));
            QString err;
            Spherical::Stereo st = (mode == QStringLiteral("vr360_tb")) ? Spherical::Stereo::TopBottom : Spherical::Stereo::Mono;
            if (!Spherical::injectFile(outputPath, st, &err)) {
                emit finished(false, QStringLiteral("The 360 video was made but its VR metadata could not be written: %1").arg(err));
                return;
            }
        }
    }

    emit progress(100, QStringLiteral("Status: Complete"));
    emit finished(true, QStringLiteral("All stereo videos generated successfully."));
}
