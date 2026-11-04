#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <iostream>
#include "../src/stereoprocessor.h"
#include "../src/videoworker.h"
#include "../src/mainwindow.h"
#include "../src/spherical.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    std::cout << "Starting Stereo Movie Maker Verification Tests..." << std::endl;

    QString testDir = QDir::current().filePath("test_temp_frames");
    QDir().mkpath(testDir);

    // 1. Create 6 sample test frame pairs (640x480)
    for (int i = 1; i <= 6; ++i) {
        QImage left(640, 480, QImage::Format_RGB888);
        left.fill(QColor(200, 50, 50)); // Reddish

        QImage right(640, 480, QImage::Format_RGB888);
        right.fill(QColor(50, 50, 200)); // Bluish

        // Draw a circle on both with slight disparity
        QPainter pL(&left);
        pL.setBrush(Qt::white);
        pL.drawEllipse(300 + i * 10, 200, 60, 60);
        pL.end();

        QPainter pR(&right);
        pR.setBrush(Qt::white);
        pR.drawEllipse(290 + i * 10, 200, 60, 60);
        pR.end();

        QString leftName = QString("%1/Frame_%2_Left.jpg").arg(testDir).arg(i, 3, 10, QChar('0'));
        QString rightName = QString("%1/Frame_%2_Right.jpg").arg(testDir).arg(i, 3, 10, QChar('0'));

        left.save(leftName, "JPG", 95);
        right.save(rightName, "JPG", 95);
    }

    // 2. Test loadFramePairs
    int origW = 0, origH = 0;
    QString errMsg, warnMsg;
    QList<FramePair> pairs = StereoProcessor::loadFramePairs(testDir, origW, origH, errMsg, warnMsg);

    if (pairs.size() != 6 || origW != 640 || origH != 480) {
        std::cerr << "FAIL: loadFramePairs failed. Count: " << pairs.size() << ", W: " << origW << ", H: " << origH << std::endl;
        return 1;
    }
    std::cout << "PASS: loadFramePairs loaded " << pairs.size() << " pairs, dimensions " << origW << "x" << origH << std::endl;

    // Verify ordering
    for (int i = 0; i < pairs.size(); ++i) {
        if (pairs[i].frameNumber != (i + 1)) {
            std::cerr << "FAIL: Numeric sorting order incorrect at index " << i << ": " << pairs[i].frameNumber << std::endl;
            return 1;
        }
    }
    std::cout << "PASS: Numeric sorting verified correctly (1 to 6)." << std::endl;

    // 3. Test applyStereoWindow
    QImage tL(640, 480, QImage::Format_RGB888);
    QImage tR(640, 480, QImage::Format_RGB888);
    tL.fill(Qt::red);
    tR.fill(Qt::blue);

    // Inward test: -20
    QPair<QImage, QImage> inward = StereoProcessor::applyStereoWindow(tL, tR, -20);
    if (inward.first.width() != 620 || inward.second.width() != 620) {
        std::cerr << "FAIL: Inward window width mismatch: " << inward.first.width() << std::endl;
        return 1;
    }
    std::cout << "PASS: Inward stereo window (-20px) produces width 620px." << std::endl;

    // Outward test: +20
    QPair<QImage, QImage> outward = StereoProcessor::applyStereoWindow(tL, tR, 20);
    if (outward.first.width() != 620 || outward.second.width() != 620) {
        std::cerr << "FAIL: Outward window width mismatch: " << outward.first.width() << std::endl;
        return 1;
    }
    std::cout << "PASS: Outward stereo window (+20px) produces width 620px." << std::endl;

    // 4. Test combineStereo
    QImage parallel = StereoProcessor::combineStereo(tL, tR, "parallel");
    if (parallel.width() != 1280 || parallel.height() != 480) {
        std::cerr << "FAIL: Parallel combination size mismatch: " << parallel.width() << "x" << parallel.height() << std::endl;
        return 1;
    }
    std::cout << "PASS: Parallel combination verified (1280x480)." << std::endl;

    QImage cross = StereoProcessor::combineStereo(tL, tR, "cross");
    if (cross.width() != 1280 || cross.height() != 480) {
        std::cerr << "FAIL: Cross combination size mismatch." << std::endl;
        return 1;
    }
    std::cout << "PASS: Cross combination verified (1280x480)." << std::endl;

    QImage anaglyph = StereoProcessor::combineStereo(tL, tR, "color_anaglyph");
    if (anaglyph.width() != 640 || anaglyph.height() != 480) {
        std::cerr << "FAIL: Color anaglyph size mismatch." << std::endl;
        return 1;
    }
    // Check channels: Red from left (red=255, green=0, blue=0), Green & Blue from right (red=0, green=0, blue=255)
    QRgb p = anaglyph.pixel(100, 100);
    if (qRed(p) < 200 || qBlue(p) < 200) {
        std::cerr << "FAIL: Color anaglyph channel mix incorrect. R: " << qRed(p) << ", G: " << qGreen(p) << ", B: " << qBlue(p) << std::endl;
        return 1;
    }
    std::cout << "PASS: Color anaglyph channel mapping verified." << std::endl;

    // 5. Test 3D Text Overlay
    StereoProcessor::add3DText(tL, tR, "Test Top", "Test Bottom");
    std::cout << "PASS: 3D text overlay rendered successfully." << std::endl;

    // 6. Test full video generation pipeline using VideoWorker
    QString testAudio = testDir + "/test_audio.wav";
    // Generate a simple 1-second audio tone using ffmpeg
    QProcess::execute("ffmpeg", {"-y", "-f", "lavfi", "-i", "sine=frequency=440:duration=1", testAudio});

    RenderOptions opt;
    opt.framePairs = pairs;
    opt.folderPath = testDir;
    opt.audioPath = testAudio;
    opt.modes = QStringList() << QStringLiteral("parallel") << QStringLiteral("color_anaglyph");
    opt.windowOffset = -10; // 640 - 10 = 630px width. 630*3 = 1890 bytes (% 4 = 2). Tests non-4-byte stride padding stripping!
    opt.doReverse = true;
    opt.useText = true;
    opt.topText = "Fractal 3D";
    opt.bottomText = "Test Author";
    opt.startDur = 1;
    opt.endDur = 1;
    opt.speedFactor = 1.0;
    opt.crf = 23;

    VideoWorker worker(opt);
    bool workerSuccess = false;
    QString workerMsg;

    QObject::connect(&worker, &VideoWorker::progress, [](int pct, const QString &status) {
        std::cout << "Progress [" << pct << "%]: " << status.toStdString() << std::endl;
    });

    QObject::connect(&worker, &VideoWorker::finished, [&](bool ok, const QString &msg) {
        workerSuccess = ok;
        workerMsg = msg;
    });

    worker.process();

    if (!workerSuccess) {
        std::cerr << "FAIL: VideoWorker failed: " << workerMsg.toStdString() << std::endl;
        return 1;
    }
    std::cout << "PASS: VideoWorker generated movies successfully: " << workerMsg.toStdString() << std::endl;

    QString parallelOut = testDir + "/parallel_final.mp4";
    QString colorAnaOut = testDir + "/color_anaglyph_final.mp4";

    if (!QFile::exists(parallelOut) || !QFile::exists(colorAnaOut)) {
        std::cerr << "FAIL: Output MP4 files not found." << std::endl;
        return 1;
    }

    qint64 sizeParallel = QFile(parallelOut).size();
    qint64 sizeColorAna = QFile(colorAnaOut).size();
    std::cout << "PASS: parallel_final.mp4 generated (" << sizeParallel << " bytes)." << std::endl;
    std::cout << "PASS: color_anaglyph_final.mp4 generated (" << sizeColorAna << " bytes)." << std::endl;

    // 6b. 360 VR: the stereo window wraps around, top-bottom layout, metadata
    {
        QImage pl(256, 128, QImage::Format_RGB888);
        pl.fill(Qt::black);
        QPainter pp(&pl);
        pp.fillRect(0, 0, 10, 128, Qt::white); // a stripe at the left edge
        pp.end();
        QImage pr = pl.copy();
        QPair<QImage, QImage> w360 = StereoProcessor::applyStereoWindow360(pl, pr, -20);
        // no crop, and the stripe turned round: pixel x is pixel x + 20 (as the
        // inward crop), so the stripe at 0..9 is at 236..245
        if (w360.first.width() != 256 || w360.second.width() != 256 ||
            QColor(w360.first.pixel(240, 5)) != QColor(Qt::white) ||
            QColor(w360.first.pixel(5, 5)) != QColor(Qt::black) ||
            QColor(w360.second.pixel(5, 5)) != QColor(Qt::white)) {
            std::cerr << "FAIL: 360 stereo window does not wrap around." << std::endl;
            return 1;
        }
        std::cout << "PASS: 360 stereo window turns the left eye round without cropping." << std::endl;

        QImage tb = StereoProcessor::combineStereo(pl, pr, QStringLiteral("vr360_tb"));
        if (tb.width() != 256 || tb.height() != 256) {
            std::cerr << "FAIL: vr360_tb size " << tb.width() << "x" << tb.height() << std::endl;
            return 1;
        }
        std::cout << "PASS: 360 top-bottom frame is 256x256 (left eye on top)." << std::endl;

        QString vrDir = QDir::current().filePath("test_temp_vr");
        QDir().mkpath(vrDir);
        for (int i = 1; i <= 4; ++i) {
            QImage l(256, 128, QImage::Format_RGB888), r(256, 128, QImage::Format_RGB888);
            l.fill(QColor(30, 60, 120));
            r.fill(QColor(30, 60, 120));
            l.save(QString("%1/newLeft%2.png").arg(vrDir).arg(i, 6, 10, QChar('0')));
            r.save(QString("%1/newRight%2.png").arg(vrDir).arg(i, 6, 10, QChar('0')));
        }
        int w = 0, h = 0;
        QString e, wn;
        RenderOptions vo;
        vo.framePairs = StereoProcessor::loadFramePairs(vrDir, w, h, e, wn);
        vo.folderPath = vrDir;
        vo.modes = {QStringLiteral("vr360_tb"), QStringLiteral("vr360_mono")};
        vo.windowOffset = -6;
        vo.useText = true;
        vo.topText = QStringLiteral("Title");
        vo.bottomText = QStringLiteral("Credit");
        vo.startDur = 1;
        vo.endDur = 1;
        VideoWorker vw(vo);
        bool vok = false;
        QString vmsg;
        QObject::connect(&vw, &VideoWorker::finished, [&](bool ok, const QString &m) { vok = ok; vmsg = m; });
        vw.process();
        if (!vok) {
            std::cerr << "FAIL: 360 VideoWorker: " << vmsg.toStdString() << std::endl;
            return 1;
        }
        const QStringList outs = {vrDir + "/vr360_final_360_TB.mp4", vrDir + "/vr360_mono_final_360.mp4"};
        for (const QString &o : outs) {
            QFile f(o);
            if (!f.open(QIODevice::ReadOnly)) {
                std::cerr << "FAIL: missing " << o.toStdString() << std::endl;
                return 1;
            }
            QByteArray d = f.readAll();
            if (!d.contains("st3d") || !d.contains("sv3d") || !d.contains("equi")) {
                std::cerr << "FAIL: no spherical metadata in " << o.toStdString() << std::endl;
                return 1;
            }
            // done again: replaced, not added
            QByteArray again = Spherical::inject(d, Spherical::Stereo::TopBottom);
            if (again.size() != d.size() || again.count("sv3d") != 1) {
                std::cerr << "FAIL: injecting twice changed the size" << std::endl;
                return 1;
            }
        }
        // ffprobe, when there is one, reads the metadata and decodes the file
        QProcess probe;
        probe.start(QStringLiteral("ffprobe"), {QStringLiteral("-v"), QStringLiteral("error"),
                                                QStringLiteral("-show_streams"), outs[0]});
        if (probe.waitForStarted(3000) && probe.waitForFinished(20000)) {
            QString out = QString::fromUtf8(probe.readAllStandardOutput());
            if (!out.contains(QStringLiteral("equirectangular")) || !out.contains(QStringLiteral("top and bottom")) ||
                !out.contains(QStringLiteral("height=256"))) {
                std::cerr << "FAIL: ffprobe does not see a stereo 360 video:\n" << out.toStdString() << std::endl;
                return 1;
            }
            std::cout << "PASS: ffprobe reads 360 equirectangular, top and bottom." << std::endl;
        }
        std::cout << "PASS: 360 VR stereo and mono MP4s carry the Spherical Video V2 metadata." << std::endl;
        QDir(vrDir).removeRecursively();
    }

    // 7. Test Project JSON Save and Import
    QString projectSavePath = testDir + "/test_project.json";
    MainWindow mainWin;
    if (!mainWin.saveProjectToFile(projectSavePath)) {
        std::cerr << "FAIL: saveProjectToFile failed." << std::endl;
        return 1;
    }

    QFile pFile(projectSavePath);
    if (!pFile.open(QIODevice::ReadOnly)) {
        std::cerr << "FAIL: Could not open saved project file." << std::endl;
        return 1;
    }
    QJsonDocument pDoc = QJsonDocument::fromJson(pFile.readAll());
    pFile.close();

    if (!pDoc.isObject() || pDoc.object().value("format").toString() != "StereoFractalMoviePro") {
        std::cerr << "FAIL: Saved project JSON header invalid." << std::endl;
        return 1;
    }

    // Modify settings in JSON and reload
    QJsonObject modifiedObj = pDoc.object();
    modifiedObj["windowOffset"] = -42;
    modifiedObj["topText"] = "Modified Stereo Title";
    modifiedObj["speedFactor"] = 2.5;

    QString modifiedJsonPath = testDir + "/modified_project.json";
    QFile modFile(modifiedJsonPath);
    modFile.open(QIODevice::WriteOnly);
    modFile.write(QJsonDocument(modifiedObj).toJson());
    modFile.close();

    QString loadErr;
    if (!mainWin.loadProjectFromFile(modifiedJsonPath, &loadErr)) {
        std::cerr << "FAIL: loadProjectFromFile failed: " << loadErr.toStdString() << std::endl;
        return 1;
    }

    QString verifySavePath = testDir + "/verify_project.json";
    mainWin.saveProjectToFile(verifySavePath);
    QFile vFile(verifySavePath);
    vFile.open(QIODevice::ReadOnly);
    QJsonDocument vDoc = QJsonDocument::fromJson(vFile.readAll());
    vFile.close();

    if (vDoc.object().value("windowOffset").toInt() != -42 ||
        vDoc.object().value("topText").toString() != "Modified Stereo Title" ||
        vDoc.object().value("speedFactor").toDouble() != 2.5) {
        std::cerr << "FAIL: Imported project parameters did not match modified values." << std::endl;
        return 1;
    }
    std::cout << "PASS: Project JSON save and import serialization verified." << std::endl;

    // Cleanup test temp directory
    QDir(testDir).removeRecursively();

    std::cout << "\nALL TESTS PASSED SUCCESSFULLY!" << std::endl;
    return 0;
}
