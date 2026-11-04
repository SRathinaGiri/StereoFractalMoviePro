#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QProgressBar>
#include <QScrollArea>
#include <QFileDialog>
#include <QMessageBox>
#include <QPixmap>
#include <QFileInfo>
#include <QTabWidget>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QFile>
#include <QLocale>
#include <algorithm>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    setLocale(QLocale::c());
    setupUi();
}

MainWindow::~MainWindow() {
    if (m_workerThread && m_workerThread->isRunning()) {
        if (m_worker) {
            m_worker->cancel();
        }
        m_workerThread->quit();
        m_workerThread->wait(3000);
    }
}

void MainWindow::setupUi() {
    setWindowTitle(QStringLiteral("Stereo Fractal Movie Pro"));
    resize(800, 900);
    setMinimumSize(740, 700);

    // Menu Bar
    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    QAction *actOpen = fileMenu->addAction(QStringLiteral("&Import Project (JSON)..."), this, &MainWindow::openProject, QKeySequence::Open);
    QAction *actSave = fileMenu->addAction(QStringLiteral("&Save Project (JSON)..."), this, &MainWindow::saveProject, QKeySequence::Save);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("E&xit"), this, &QWidget::close, QKeySequence::Quit);

    QWidget *centralContainer = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralContainer);
    mainLayout->setContentsMargins(16, 8, 16, 10);
    mainLayout->setSpacing(6);

    // Title & Project Action Buttons Bar
    QHBoxLayout *topBarLayout = new QHBoxLayout();
    QLabel *titleLabel = new QLabel(QStringLiteral("Stereo Fractal Movie Pro"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(15);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    QPushButton *btnImportProject = new QPushButton(QStringLiteral("📂 Import Project (JSON)"), this);
    QPushButton *btnSaveProject = new QPushButton(QStringLiteral("💾 Save Project (JSON)"), this);
    btnImportProject->setStyleSheet(QStringLiteral("padding: 4px 10px; font-weight: bold;"));
    btnSaveProject->setStyleSheet(QStringLiteral("padding: 4px 10px; font-weight: bold;"));

    topBarLayout->addWidget(titleLabel);
    topBarLayout->addStretch();
    topBarLayout->addWidget(btnImportProject);
    topBarLayout->addWidget(btnSaveProject);
    mainLayout->addLayout(topBarLayout);

    connect(btnImportProject, &QPushButton::clicked, this, &MainWindow::openProject);
    connect(btnSaveProject, &QPushButton::clicked, this, &MainWindow::saveProject);

    // =========================================================
    // 1. SETUP SOURCE & AUDIO (Single row, two half-width columns)
    // =========================================================
    QGroupBox *sourceGroup = new QGroupBox(QStringLiteral("1. Setup Source & Audio"), this);
    QHBoxLayout *sourceLayout = new QHBoxLayout(sourceGroup);
    sourceLayout->setContentsMargins(10, 8, 10, 8);
    sourceLayout->setSpacing(12);

    // Left Column: Frames Folder (Half Width)
    QVBoxLayout *colFolder = new QVBoxLayout();
    colFolder->setSpacing(4);
    QPushButton *btnBrowseFolder = new QPushButton(QStringLiteral("Browse Frames Folder"), this);
    m_lblFolderPath = new QLabel(QStringLiteral("No folder selected"), this);
    m_lblFolderPath->setStyleSheet(QStringLiteral("color: gray;"));
    m_lblFolderPath->setWordWrap(true);
    m_lblFolderPath->setMaximumHeight(34);
    m_lblFolderPath->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    colFolder->addWidget(btnBrowseFolder);
    colFolder->addWidget(m_lblFolderPath);

    // Right Column: Background Audio (Half Width)
    QVBoxLayout *colAudio = new QVBoxLayout();
    colAudio->setSpacing(4);
    QHBoxLayout *audioRow = new QHBoxLayout();
    QPushButton *btnBrowseAudio = new QPushButton(QStringLiteral("Set Background Audio"), this);
    QPushButton *btnClearAudio = new QPushButton(QStringLiteral("Clear Audio"), this);
    btnClearAudio->setFixedWidth(90);
    audioRow->addWidget(btnBrowseAudio);
    audioRow->addWidget(btnClearAudio);

    m_lblAudioPath = new QLabel(QStringLiteral("(Optional) No audio file selected"), this);
    m_lblAudioPath->setStyleSheet(QStringLiteral("color: gray;"));
    m_lblAudioPath->setWordWrap(true);
    m_lblAudioPath->setMaximumHeight(34);
    m_lblAudioPath->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    colAudio->addLayout(audioRow);
    colAudio->addWidget(m_lblAudioPath);

    sourceLayout->addLayout(colFolder, 1);
    sourceLayout->addLayout(colAudio, 1);
    mainLayout->addWidget(sourceGroup);

    connect(btnBrowseFolder, &QPushButton::clicked, this, &MainWindow::browseFolder);
    connect(btnBrowseAudio, &QPushButton::clicked, this, &MainWindow::browseAudio);
    connect(btnClearAudio, &QPushButton::clicked, this, &MainWindow::clearAudio);

    // =========================================================
    // 2. STEREO PREVIEW
    // =========================================================
    QGroupBox *previewGroup = new QGroupBox(QStringLiteral("2. Stereo Preview"), this);
    QVBoxLayout *previewLayout = new QVBoxLayout(previewGroup);
    previewLayout->setContentsMargins(10, 6, 10, 6);
    previewLayout->setSpacing(4);

    QHBoxLayout *modeRow = new QHBoxLayout();
    QLabel *lblMode = new QLabel(QStringLiteral("Preview Mode:"), this);
    m_comboPreviewMode = new QComboBox(this);
    m_comboPreviewMode->addItem(QStringLiteral("parallel"), QStringLiteral("parallel"));
    m_comboPreviewMode->addItem(QStringLiteral("cross"), QStringLiteral("cross"));
    m_comboPreviewMode->addItem(QStringLiteral("color_anaglyph"), QStringLiteral("color_anaglyph"));
    m_comboPreviewMode->addItem(QStringLiteral("gray_anaglyph"), QStringLiteral("gray_anaglyph"));
    m_comboPreviewMode->addItem(QStringLiteral("360 VR top-bottom"), QStringLiteral("vr360_tb"));
    m_comboPreviewMode->addItem(QStringLiteral("360 VR mono"), QStringLiteral("vr360_mono"));

    m_chkShowGuide = new QCheckBox(QStringLiteral("Show Stereo Window Guide"), this);
    m_chkShowGuide->setChecked(true);

    modeRow->addWidget(lblMode);
    modeRow->addWidget(m_comboPreviewMode);
    modeRow->addSpacing(15);
    modeRow->addWidget(m_chkShowGuide);
    modeRow->addStretch();
    previewLayout->addLayout(modeRow);

    m_lblPreviewImage = new QLabel(this);
    m_lblPreviewImage->setFixedSize(580, 210);
    m_lblPreviewImage->setAlignment(Qt::AlignCenter);
    m_lblPreviewImage->setStyleSheet(QStringLiteral("background-color: black; border: 1px solid #333; border-radius: 4px;"));
    previewLayout->addWidget(m_lblPreviewImage, 0, Qt::AlignCenter);

    m_sliderFrame = new QSlider(Qt::Horizontal, this);
    m_sliderFrame->setRange(0, 0);
    m_sliderFrame->setEnabled(false);
    m_sliderFrame->setLocale(QLocale::c());
    previewLayout->addWidget(m_sliderFrame);

    m_lblFrameInfo = new QLabel(QStringLiteral("Frame: -"), this);
    m_lblFrameInfo->setAlignment(Qt::AlignCenter);
    previewLayout->addWidget(m_lblFrameInfo);

    QHBoxLayout *guideRow = new QHBoxLayout();
    QLabel *lblGuide = new QLabel(QStringLiteral("Guide Position:"), this);
    m_sliderGuide = new QSlider(Qt::Horizontal, this);
    m_sliderGuide->setRange(5, 95);
    m_sliderGuide->setValue(50);
    m_sliderGuide->setLocale(QLocale::c());
    QPushButton *btnCenterGuide = new QPushButton(QStringLiteral("Center"), this);

    guideRow->addWidget(lblGuide);
    guideRow->addWidget(m_sliderGuide, 1);
    guideRow->addWidget(btnCenterGuide);
    previewLayout->addLayout(guideRow);

    mainLayout->addWidget(previewGroup);

    connect(m_comboPreviewMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { updatePreview(); });
    connect(m_chkShowGuide, &QCheckBox::toggled, this, [this]() { updatePreview(); });
    connect(m_sliderFrame, &QSlider::valueChanged, this, &MainWindow::onFrameSliderChanged);
    connect(m_sliderGuide, &QSlider::valueChanged, this, [this]() { updatePreview(); });
    connect(btnCenterGuide, &QPushButton::clicked, this, &MainWindow::setGuideCenter);

    // =========================================================
    // TABS: 3. STEREO WINDOW | 4. 3D TEXT | 5. OUTPUT OPTIONS
    // =========================================================
    QTabWidget *settingsTabs = new QTabWidget(this);
    settingsTabs->setLocale(QLocale::c());
    settingsTabs->setStyleSheet(QStringLiteral("QTabBar::tab { font-weight: bold; padding: 4px 12px; }"));

    // --- Tab 1: Stereo Window Adjustment ---
    QWidget *tabWindow = new QWidget();
    QVBoxLayout *windowLayout = new QVBoxLayout(tabWindow);
    windowLayout->setContentsMargins(10, 8, 10, 8);
    windowLayout->setSpacing(6);

    QLabel *lblWindowHint = new QLabel(QStringLiteral("Negative = Inward   |   Positive = Outward   |   Values are source-image pixels"), tabWindow);
    lblWindowHint->setStyleSheet(QStringLiteral("color: gray;"));
    lblWindowHint->setAlignment(Qt::AlignCenter);
    windowLayout->addWidget(lblWindowHint);

    QHBoxLayout *windowBtnRow = new QHBoxLayout();
    QPushButton *btnMinus10 = new QPushButton(QStringLiteral("◀◀ -10"), tabWindow);
    QPushButton *btnMinus1 = new QPushButton(QStringLiteral("◀ -1"), tabWindow);
    QLabel *lblWindow = new QLabel(QStringLiteral("Window:"), tabWindow);

    m_spinWindowOffset = new QSpinBox(tabWindow);
    m_spinWindowOffset->setLocale(QLocale::c());
    m_spinWindowOffset->setRange(-9999, 9999);
    m_spinWindowOffset->setValue(0);
    m_spinWindowOffset->setSuffix(QStringLiteral(" px"));
    m_spinWindowOffset->setAlignment(Qt::AlignCenter);
    m_spinWindowOffset->setFixedWidth(110);

    QPushButton *btnPlus1 = new QPushButton(QStringLiteral("+1 ▶"), tabWindow);
    QPushButton *btnPlus10 = new QPushButton(QStringLiteral("+10 ▶▶"), tabWindow);
    QPushButton *btnReset = new QPushButton(QStringLiteral("Reset"), tabWindow);

    windowBtnRow->addStretch();
    windowBtnRow->addWidget(btnMinus10);
    windowBtnRow->addWidget(btnMinus1);
    windowBtnRow->addSpacing(8);
    windowBtnRow->addWidget(lblWindow);
    windowBtnRow->addWidget(m_spinWindowOffset);
    windowBtnRow->addSpacing(8);
    windowBtnRow->addWidget(btnPlus1);
    windowBtnRow->addWidget(btnPlus10);
    windowBtnRow->addSpacing(10);
    windowBtnRow->addWidget(btnReset);
    windowBtnRow->addStretch();
    windowLayout->addLayout(windowBtnRow);

    connect(btnMinus10, &QPushButton::clicked, this, [this]() { adjustWindow(-10); });
    connect(btnMinus1, &QPushButton::clicked, this, [this]() { adjustWindow(-1); });
    connect(btnPlus1, &QPushButton::clicked, this, [this]() { adjustWindow(1); });
    connect(btnPlus10, &QPushButton::clicked, this, [this]() { adjustWindow(10); });
    connect(btnReset, &QPushButton::clicked, this, &MainWindow::resetWindow);
    connect(m_spinWindowOffset, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { updatePreview(); });

    settingsTabs->addTab(tabWindow, QStringLiteral("3. Stereo Window"));

    // --- Tab 2: 3D Text Settings ---
    QWidget *tabText = new QWidget();
    QVBoxLayout *textLayout = new QVBoxLayout(tabText);
    textLayout->setContentsMargins(10, 8, 10, 8);
    textLayout->setSpacing(4);

    QHBoxLayout *textRow1 = new QHBoxLayout();
    QLabel *lblTop = new QLabel(QStringLiteral("Top (Front):"), tabText);
    m_txtTopText = new QLineEdit(QStringLiteral("Stereo Fractal Animation"), tabText);
    textRow1->addWidget(lblTop);
    textRow1->addWidget(m_txtTopText);
    textLayout->addLayout(textRow1);

    QHBoxLayout *textRow2 = new QHBoxLayout();
    QLabel *lblBottom = new QLabel(QStringLiteral("Bottom (Back):"), tabText);
    m_txtBottomText = new QLineEdit(QStringLiteral("Created by S. Rathinagiri"), tabText);
    textRow2->addWidget(lblBottom);
    textRow2->addWidget(m_txtBottomText);
    textLayout->addLayout(textRow2);

    QHBoxLayout *durRow = new QHBoxLayout();
    QLabel *lblStartDur = new QLabel(QStringLiteral("Start Sec:"), tabText);
    m_spinStartDur = new QSpinBox(tabText);
    m_spinStartDur->setLocale(QLocale::c());
    m_spinStartDur->setRange(0, 600);
    m_spinStartDur->setValue(5);
    m_spinStartDur->setFixedWidth(70);

    QLabel *lblEndDur = new QLabel(QStringLiteral("End Sec:"), tabText);
    m_spinEndDur = new QSpinBox(tabText);
    m_spinEndDur->setLocale(QLocale::c());
    m_spinEndDur->setRange(0, 600);
    m_spinEndDur->setValue(5);
    m_spinEndDur->setFixedWidth(70);

    m_chkUseText = new QCheckBox(QStringLiteral("Enable 3D Text Overlay"), tabText);
    m_chkUseText->setChecked(true);

    durRow->addWidget(lblStartDur);
    durRow->addWidget(m_spinStartDur);
    durRow->addSpacing(20);
    durRow->addWidget(lblEndDur);
    durRow->addWidget(m_spinEndDur);
    durRow->addSpacing(25);
    durRow->addWidget(m_chkUseText);
    durRow->addStretch();
    textLayout->addLayout(durRow);

    settingsTabs->addTab(tabText, QStringLiteral("4. 3D Text Overlay"));

    // --- Tab 3: View & Output Options ---
    QWidget *tabOptions = new QWidget();
    QVBoxLayout *optLayout = new QVBoxLayout(tabOptions);
    optLayout->setContentsMargins(10, 8, 10, 8);
    optLayout->setSpacing(4);

    QGridLayout *gridModes = new QGridLayout();
    gridModes->setVerticalSpacing(4);
    m_chkParallel = new QCheckBox(QStringLiteral("Parallel View ||"), tabOptions);
    m_chkParallel->setChecked(true);
    m_chkCross = new QCheckBox(QStringLiteral("Cross View X"), tabOptions);
    m_chkColorAna = new QCheckBox(QStringLiteral("Color Anaglyph"), tabOptions);
    m_chkGrayAna = new QCheckBox(QStringLiteral("Grayscale Anaglyph"), tabOptions);
    m_chkVr360 = new QCheckBox(QStringLiteral("360° VR Stereo (Top-Bottom)"), tabOptions);
    m_chkVr360->setToolTip(QStringLiteral(
        "For VR headsets (Meta Quest) and YouTube VR: the frames must be 360° panoramas (2:1).\n"
        "Left eye on top, right eye below; the MP4 gets the 360 stereo metadata."));
    m_chkVr360Mono = new QCheckBox(QStringLiteral("360° VR Mono"), tabOptions);
    m_chkVr360Mono->setToolTip(QStringLiteral(
        "For Facebook 360 and phones: the left eye's 360° panorama alone, with the 360 metadata."));

    gridModes->addWidget(m_chkParallel, 0, 0);
    gridModes->addWidget(m_chkCross, 0, 1);
    gridModes->addWidget(m_chkColorAna, 1, 0);
    gridModes->addWidget(m_chkGrayAna, 1, 1);
    gridModes->addWidget(m_chkVr360, 2, 0);
    gridModes->addWidget(m_chkVr360Mono, 2, 1);
    optLayout->addLayout(gridModes);

    QHBoxLayout *optBottomRow = new QHBoxLayout();
    m_chkPingPong = new QCheckBox(QStringLiteral("Ping-Pong (Reverse & Append)"), tabOptions);
    m_chkPingPong->setChecked(true);
    m_chkPingPong->setStyleSheet(QStringLiteral("color: #0055cc; font-weight: bold;"));

    QLabel *lblSpeed = new QLabel(QStringLiteral("Speed:"), tabOptions);
    m_spinSpeed = new QDoubleSpinBox(tabOptions);
    m_spinSpeed->setLocale(QLocale::c());
    m_spinSpeed->setRange(0.1, 10.0);
    m_spinSpeed->setSingleStep(0.1);
    m_spinSpeed->setValue(1.0);
    m_spinSpeed->setFixedWidth(75);

    QLabel *lblCrf = new QLabel(QStringLiteral("CRF:"), tabOptions);
    m_spinCrf = new QSpinBox(tabOptions);
    m_spinCrf->setLocale(QLocale::c());
    m_spinCrf->setRange(0, 51);
    m_spinCrf->setValue(23);
    m_spinCrf->setFixedWidth(65);

    optBottomRow->addWidget(m_chkPingPong);
    optBottomRow->addStretch();
    optBottomRow->addWidget(lblSpeed);
    optBottomRow->addWidget(m_spinSpeed);
    optBottomRow->addSpacing(12);
    optBottomRow->addWidget(lblCrf);
    optBottomRow->addWidget(m_spinCrf);
    optLayout->addLayout(optBottomRow);

    settingsTabs->addTab(tabOptions, QStringLiteral("5. Output Options"));

    mainLayout->addWidget(settingsTabs);

    // =========================================================
    // GENERATE SECTION
    // =========================================================
    m_btnGenerate = new QPushButton(QStringLiteral("GENERATE 3D MOVIES"), this);
    m_btnGenerate->setMinimumHeight(38);
    m_btnGenerate->setStyleSheet(
        QStringLiteral("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; font-size: 14px; border-radius: 4px; }"
                       "QPushButton:hover { background-color: #45a049; }"
                       "QPushButton:pressed { background-color: #3d8b40; }"
                       "QPushButton:disabled { background-color: #a5d6a7; color: #f0f0f0; }")
    );
    mainLayout->addWidget(m_btnGenerate);

    m_lblStatus = new QLabel(QStringLiteral("Status: Idle"), this);
    m_lblStatus->setAlignment(Qt::AlignCenter);
    m_lblStatus->setStyleSheet(QStringLiteral("color: #0055cc; font-weight: bold;"));
    mainLayout->addWidget(m_lblStatus);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setFixedHeight(16);
    m_progressBar->setTextVisible(true);
    mainLayout->addWidget(m_progressBar);

    connect(m_btnGenerate, &QPushButton::clicked, this, &MainWindow::processFolder);

    // Embed everything inside a QScrollArea so it adapts smoothly to any display scaling
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidget(centralContainer);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setCentralWidget(scrollArea);
}

void MainWindow::browseFolder() {
    QString folder = QFileDialog::getExistingDirectory(this, QStringLiteral("Select Frames Folder"), m_folderPath);
    if (folder.isEmpty()) return;

    QString errorMsg, warningMsg;
    int origW = 0, origH = 0;
    QList<FramePair> pairs = StereoProcessor::loadFramePairs(folder, origW, origH, errorMsg, warningMsg);

    if (!errorMsg.isEmpty()) {
        QMessageBox::critical(this, QStringLiteral("Error"), errorMsg);
        return;
    }

    if (!warningMsg.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Frame Count Difference"), warningMsg);
    }

    m_folderPath = folder;
    m_lblFolderPath->setText(folder);
    m_lblFolderPath->setToolTip(folder);
    m_framePairs = pairs;
    m_originalWidth = origW;
    m_originalHeight = origH;

    m_sliderFrame->setEnabled(true);
    m_sliderFrame->setRange(0, std::max(0, static_cast<int>(m_framePairs.size()) - 1));
    m_sliderFrame->setValue(0);

    // Limit window spinbox bounds to image width - 2
    int maxOffset = std::max(10, origW - 2);
    m_spinWindowOffset->setRange(-maxOffset, maxOffset);

    updatePreview();
}

void MainWindow::browseAudio() {
    QString file = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Select Background Audio"),
        m_folderPath,
        QStringLiteral("Audio Files (*.mp3 *.wav *.aac *.m4a);;All Files (*.*)")
    );
    if (!file.isEmpty()) {
        m_audioPath = file;
        m_lblAudioPath->setText(file);
        m_lblAudioPath->setToolTip(file);
    }
}

void MainWindow::clearAudio() {
    m_audioPath.clear();
    m_lblAudioPath->setText(QStringLiteral("(Optional) No audio file selected"));
    m_lblAudioPath->setToolTip(QString());
}

void MainWindow::setGuideCenter() {
    m_sliderGuide->setValue(50);
}

void MainWindow::onFrameSliderChanged(int value) {
    Q_UNUSED(value);
    updatePreview();
}

void MainWindow::adjustWindow(int delta) {
    int current = m_spinWindowOffset->value();
    m_spinWindowOffset->setValue(current + delta);
}

void MainWindow::resetWindow() {
    m_spinWindowOffset->setValue(0);
}

void MainWindow::updatePreview() {
    if (m_framePairs.isEmpty()) {
        m_lblPreviewImage->clear();
        m_lblFrameInfo->setText(QStringLiteral("Frame: -"));
        return;
    }

    int idx = m_sliderFrame->value();
    idx = std::max(0, std::min(idx, static_cast<int>(m_framePairs.size()) - 1));

    const FramePair &pair = m_framePairs[idx];
    QImage imgL(pair.leftPath);
    QImage imgR(pair.rightPath);

    if (imgL.isNull() || imgR.isNull()) {
        return;
    }

    QString mode = m_comboPreviewMode->currentData().toString();
    int offset = m_spinWindowOffset->value();
    bool showGuide = m_chkShowGuide->isChecked();
    double guidePos = static_cast<double>(m_sliderGuide->value()) / 100.0;

    QImage preview = StereoProcessor::createPreview(imgL, imgR, mode, offset, showGuide, guidePos, 580, 210);

    if (!preview.isNull()) {
        m_lblPreviewImage->setPixmap(QPixmap::fromImage(preview));
    }

    QString modeDisplay;
    if (mode == QStringLiteral("parallel")) modeDisplay = QStringLiteral("Parallel ||");
    else if (mode == QStringLiteral("cross")) modeDisplay = QStringLiteral("Cross X");
    else if (mode == QStringLiteral("color_anaglyph")) modeDisplay = QStringLiteral("Color Anaglyph");
    else if (mode == QStringLiteral("gray_anaglyph")) modeDisplay = QStringLiteral("Gray Anaglyph");
    else if (mode == QStringLiteral("vr360_tb")) modeDisplay = QStringLiteral("360 VR Top-Bottom");
    else if (mode == QStringLiteral("vr360_mono")) modeDisplay = QStringLiteral("360 VR Mono");

    m_lblFrameInfo->setText(
        QStringLiteral("Frame %1/%2    |    %3    |    Window %+1dpx")
            .arg(idx + 1)
            .arg(m_framePairs.size())
            .arg(modeDisplay)
            .arg(offset)
    );
}

void MainWindow::setUiEnabled(bool enabled) {
    m_btnGenerate->setEnabled(enabled);
}

void MainWindow::processFolder() {
    if (m_folderPath.isEmpty()) {
        QMessageBox::critical(this, QStringLiteral("Error"), QStringLiteral("Please select the frames folder."));
        return;
    }

    if (!m_audioPath.isEmpty() && !QFileInfo::exists(m_audioPath)) {
        QMessageBox::critical(this, QStringLiteral("Error"), QStringLiteral("Specified audio file does not exist."));
        return;
    }

    if (m_framePairs.isEmpty()) {
        QString errorMsg, warningMsg;
        int origW = 0, origH = 0;
        m_framePairs = StereoProcessor::loadFramePairs(m_folderPath, origW, origH, errorMsg, warningMsg);
        m_originalWidth = origW;
        m_originalHeight = origH;
        if (!errorMsg.isEmpty()) {
            QMessageBox::critical(this, QStringLiteral("Error"), errorMsg);
            return;
        }
    }

    int offset = m_spinWindowOffset->value();
    if (std::abs(offset) >= (m_originalWidth - 1)) {
        QMessageBox::critical(this, QStringLiteral("Invalid Stereo Window"), QStringLiteral("Window offset is too large."));
        return;
    }

    QStringList selectedModes;
    if (m_chkParallel->isChecked()) selectedModes << QStringLiteral("parallel");
    if (m_chkCross->isChecked()) selectedModes << QStringLiteral("cross");
    if (m_chkColorAna->isChecked()) selectedModes << QStringLiteral("color_anaglyph");
    if (m_chkGrayAna->isChecked()) selectedModes << QStringLiteral("gray_anaglyph");
    if (m_chkVr360->isChecked()) selectedModes << QStringLiteral("vr360_tb");
    if (m_chkVr360Mono->isChecked()) selectedModes << QStringLiteral("vr360_mono");

    if ((m_chkVr360->isChecked() || m_chkVr360Mono->isChecked()) &&
        m_originalHeight > 0 && std::abs(m_originalWidth - 2 * m_originalHeight) > 2) {
        auto answer = QMessageBox::question(
            this, QStringLiteral("Not a 360° panorama"),
            QStringLiteral("The frames are %1 x %2, but a 360° panorama is twice as wide as high.\n"
                           "VR players will show them stretched around the sphere.\n\nMake the 360 videos anyway?")
                .arg(m_originalWidth).arg(m_originalHeight));
        if (answer != QMessageBox::Yes) return;
    }

    if (selectedModes.isEmpty()) {
        QMessageBox::critical(this, QStringLiteral("Error"), QStringLiteral("Select at least one output type."));
        return;
    }

    RenderOptions opt;
    opt.framePairs = m_framePairs;
    opt.folderPath = m_folderPath;
    opt.audioPath = m_audioPath;
    opt.modes = selectedModes;
    opt.windowOffset = offset;
    opt.doReverse = m_chkPingPong->isChecked();
    opt.useText = m_chkUseText->isChecked();
    opt.topText = m_txtTopText->text();
    opt.bottomText = m_txtBottomText->text();
    opt.startDur = m_spinStartDur->value();
    opt.endDur = m_spinEndDur->value();
    opt.speedFactor = m_spinSpeed->value();
    opt.crf = m_spinCrf->value();

    setUiEnabled(false);
    m_progressBar->setValue(0);
    m_lblStatus->setStyleSheet(QStringLiteral("color: #0055cc; font-weight: bold;"));
    m_lblStatus->setText(QStringLiteral("Status: Starting..."));

    m_workerThread = new QThread(this);
    m_worker = new VideoWorker(opt);
    m_worker->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::started, m_worker, &VideoWorker::process);
    connect(m_worker, &VideoWorker::progress, this, &MainWindow::onRenderProgress);
    connect(m_worker, &VideoWorker::finished, this, &MainWindow::onRenderFinished);
    connect(m_worker, &VideoWorker::finished, m_workerThread, &QThread::quit);
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_workerThread, &QThread::finished, m_workerThread, &QObject::deleteLater);

    m_workerThread->start();
}

void MainWindow::onRenderProgress(int percentage, const QString &statusText) {
    m_progressBar->setValue(percentage);
    m_lblStatus->setText(statusText);
}

void MainWindow::onRenderFinished(bool success, const QString &message) {
    setUiEnabled(true);
    m_workerThread = nullptr;
    m_worker = nullptr;

    if (success) {
        m_progressBar->setValue(100);
        m_lblStatus->setStyleSheet(QStringLiteral("color: #2e7d32; font-weight: bold;"));
        m_lblStatus->setText(QStringLiteral("Status: Complete"));
        QMessageBox::information(this, QStringLiteral("Success"), message);
    } else {
        m_lblStatus->setStyleSheet(QStringLiteral("color: #c62828; font-weight: bold;"));
        m_lblStatus->setText(QStringLiteral("Status: Error"));
        QMessageBox::critical(this, QStringLiteral("Error"), message);
    }
}

void MainWindow::saveProject() {
    QString defaultPath = m_folderPath.isEmpty() ? QDir::currentPath() : m_folderPath;
    QString defaultFile = QDir(defaultPath).filePath(QStringLiteral("stereo_project.json"));

    QString file = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Save Project (JSON)"),
        defaultFile,
        QStringLiteral("Stereo Project Files (*.json);;All Files (*.*)")
    );

    if (file.isEmpty()) return;

    if (!file.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
        file += QStringLiteral(".json");
    }

    if (saveProjectToFile(file)) {
        QMessageBox::information(
            this,
            QStringLiteral("Project Saved"),
            QStringLiteral("Project parameters saved successfully to:\n%1").arg(file)
        );
    } else {
        QMessageBox::critical(
            this,
            QStringLiteral("Save Error"),
            QStringLiteral("Failed to write project file:\n%1").arg(file)
        );
    }
}

void MainWindow::openProject() {
    QString defaultPath = m_folderPath.isEmpty() ? QDir::currentPath() : m_folderPath;

    QString file = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Import Project (JSON)"),
        defaultPath,
        QStringLiteral("Stereo Project Files (*.json);;All Files (*.*)")
    );

    if (file.isEmpty()) return;

    QString errorMsg;
    if (loadProjectFromFile(file, &errorMsg)) {
        QMessageBox::information(
            this,
            QStringLiteral("Project Imported"),
            QStringLiteral("Project parameters imported successfully from:\n%1").arg(file)
        );
    } else {
        QMessageBox::critical(
            this,
            QStringLiteral("Import Error"),
            errorMsg
        );
    }
}

bool MainWindow::saveProjectToFile(const QString &filePath) {
    QJsonObject root;
    root[QStringLiteral("format")] = QStringLiteral("StereoFractalMoviePro");
    root[QStringLiteral("version")] = QStringLiteral("1.0");

    // 1. Source & Audio
    root[QStringLiteral("folderPath")] = m_folderPath;
    root[QStringLiteral("audioPath")] = m_audioPath;

    // 2. Stereo Preview
    root[QStringLiteral("previewMode")] = m_comboPreviewMode->currentData().toString();
    root[QStringLiteral("showGuide")] = m_chkShowGuide->isChecked();
    root[QStringLiteral("guidePosition")] = m_sliderGuide->value();

    // 3. Stereo Window
    root[QStringLiteral("windowOffset")] = m_spinWindowOffset->value();

    // 4. 3D Text Settings
    root[QStringLiteral("topText")] = m_txtTopText->text();
    root[QStringLiteral("bottomText")] = m_txtBottomText->text();
    root[QStringLiteral("startDur")] = m_spinStartDur->value();
    root[QStringLiteral("endDur")] = m_spinEndDur->value();
    root[QStringLiteral("useText")] = m_chkUseText->isChecked();

    // 5. View & Sequence Options
    root[QStringLiteral("doParallel")] = m_chkParallel->isChecked();
    root[QStringLiteral("doCross")] = m_chkCross->isChecked();
    root[QStringLiteral("doColorAna")] = m_chkColorAna->isChecked();
    root[QStringLiteral("doGrayAna")] = m_chkGrayAna->isChecked();
    root[QStringLiteral("doVr360")] = m_chkVr360->isChecked();
    root[QStringLiteral("doVr360Mono")] = m_chkVr360Mono->isChecked();
    root[QStringLiteral("doPingPong")] = m_chkPingPong->isChecked();
    root[QStringLiteral("speedFactor")] = m_spinSpeed->value();
    root[QStringLiteral("crf")] = m_spinCrf->value();

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool MainWindow::loadProjectFromFile(const QString &filePath, QString *errorMsg) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMsg) *errorMsg = QStringLiteral("Could not open file for reading: %1").arg(filePath);
        return false;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (doc.isNull() || !doc.isObject()) {
        if (errorMsg) *errorMsg = QStringLiteral("Invalid JSON syntax: %1").arg(parseError.errorString());
        return false;
    }

    QJsonObject root = doc.object();

    // 1. Source & Audio
    if (root.contains(QStringLiteral("folderPath"))) {
        QString folder = root.value(QStringLiteral("folderPath")).toString();
        m_folderPath = folder;
        m_lblFolderPath->setText(folder.isEmpty() ? QStringLiteral("No folder selected") : folder);
        m_lblFolderPath->setToolTip(folder);

        if (!folder.isEmpty() && QDir(folder).exists()) {
            QString err, warn;
            int w = 0, h = 0;
            QList<FramePair> pairs = StereoProcessor::loadFramePairs(folder, w, h, err, warn);
            if (err.isEmpty()) {
                m_framePairs = pairs;
                m_originalWidth = w;
                m_originalHeight = h;
                m_sliderFrame->setEnabled(true);
                m_sliderFrame->setRange(0, std::max(0, static_cast<int>(m_framePairs.size()) - 1));
                m_sliderFrame->setValue(0);
                int maxOffset = std::max(10, w - 2);
                m_spinWindowOffset->setRange(-maxOffset, maxOffset);
            }
        }
    }

    if (root.contains(QStringLiteral("audioPath"))) {
        m_audioPath = root.value(QStringLiteral("audioPath")).toString();
        m_lblAudioPath->setText(m_audioPath.isEmpty() ? QStringLiteral("(Optional) No audio file selected") : m_audioPath);
        m_lblAudioPath->setToolTip(m_audioPath);
    }

    // 2. Stereo Preview
    if (root.contains(QStringLiteral("previewMode"))) {
        QString mode = root.value(QStringLiteral("previewMode")).toString();
        int idx = m_comboPreviewMode->findData(mode);
        if (idx >= 0) {
            m_comboPreviewMode->setCurrentIndex(idx);
        }
    }

    if (root.contains(QStringLiteral("showGuide"))) {
        m_chkShowGuide->setChecked(root.value(QStringLiteral("showGuide")).toBool(true));
    }

    if (root.contains(QStringLiteral("guidePosition"))) {
        m_sliderGuide->setValue(root.value(QStringLiteral("guidePosition")).toInt(50));
    }

    // 3. Stereo Window
    if (root.contains(QStringLiteral("windowOffset"))) {
        m_spinWindowOffset->setValue(root.value(QStringLiteral("windowOffset")).toInt(0));
    }

    // 4. 3D Text Settings
    if (root.contains(QStringLiteral("topText"))) {
        m_txtTopText->setText(root.value(QStringLiteral("topText")).toString());
    }

    if (root.contains(QStringLiteral("bottomText"))) {
        m_txtBottomText->setText(root.value(QStringLiteral("bottomText")).toString());
    }

    if (root.contains(QStringLiteral("startDur"))) {
        m_spinStartDur->setValue(root.value(QStringLiteral("startDur")).toInt(5));
    }

    if (root.contains(QStringLiteral("endDur"))) {
        m_spinEndDur->setValue(root.value(QStringLiteral("endDur")).toInt(5));
    }

    if (root.contains(QStringLiteral("useText"))) {
        m_chkUseText->setChecked(root.value(QStringLiteral("useText")).toBool(true));
    }

    // 5. View & Sequence Options
    if (root.contains(QStringLiteral("doParallel"))) {
        m_chkParallel->setChecked(root.value(QStringLiteral("doParallel")).toBool(true));
    }

    if (root.contains(QStringLiteral("doCross"))) {
        m_chkCross->setChecked(root.value(QStringLiteral("doCross")).toBool(false));
    }

    if (root.contains(QStringLiteral("doColorAna"))) {
        m_chkColorAna->setChecked(root.value(QStringLiteral("doColorAna")).toBool(false));
    }

    if (root.contains(QStringLiteral("doGrayAna"))) {
        m_chkGrayAna->setChecked(root.value(QStringLiteral("doGrayAna")).toBool(false));
    }

    if (root.contains(QStringLiteral("doVr360"))) {
        m_chkVr360->setChecked(root.value(QStringLiteral("doVr360")).toBool(false));
    }

    if (root.contains(QStringLiteral("doVr360Mono"))) {
        m_chkVr360Mono->setChecked(root.value(QStringLiteral("doVr360Mono")).toBool(false));
    }

    if (root.contains(QStringLiteral("doPingPong"))) {
        m_chkPingPong->setChecked(root.value(QStringLiteral("doPingPong")).toBool(true));
    }

    if (root.contains(QStringLiteral("speedFactor"))) {
        m_spinSpeed->setValue(root.value(QStringLiteral("speedFactor")).toDouble(1.0));
    }

    if (root.contains(QStringLiteral("crf"))) {
        m_spinCrf->setValue(root.value(QStringLiteral("crf")).toInt(23));
    }

    updatePreview();
    return true;
}

