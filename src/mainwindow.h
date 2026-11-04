#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QList>
#include <QPair>
#include <QThread>
#include "stereoprocessor.h"
#include "videoworker.h"

class QLabel;
class QPushButton;
class QComboBox;
class QCheckBox;
class QSlider;
class QSpinBox;
class QDoubleSpinBox;
class QLineEdit;
class QProgressBar;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void browseFolder();
    void browseAudio();
    void clearAudio();
    void updatePreview();
    void onFrameSliderChanged(int value);
    void setGuideCenter();
    void adjustWindow(int delta);
    void resetWindow();
    void saveProject();
    void openProject();
    void processFolder();
    void onRenderProgress(int percentage, const QString &statusText);
    void onRenderFinished(bool success, const QString &message);

public:
    bool saveProjectToFile(const QString &filePath);
    bool loadProjectFromFile(const QString &filePath, QString *errorMsg = nullptr);

private:
    void setupUi();
    void setUiEnabled(bool enabled);

    // Data State
    QList<FramePair> m_framePairs;
    int m_originalWidth = 0;
    int m_originalHeight = 0;
    QString m_folderPath;
    QString m_audioPath;

    // Worker thread
    QThread *m_workerThread = nullptr;
    VideoWorker *m_worker = nullptr;

    // UI Widgets
    QLabel *m_lblFolderPath = nullptr;
    QLabel *m_lblAudioPath = nullptr;

    QComboBox *m_comboPreviewMode = nullptr;
    QCheckBox *m_chkShowGuide = nullptr;
    QLabel *m_lblPreviewImage = nullptr;
    QSlider *m_sliderFrame = nullptr;
    QLabel *m_lblFrameInfo = nullptr;
    QSlider *m_sliderGuide = nullptr;

    QSpinBox *m_spinWindowOffset = nullptr;

    QLineEdit *m_txtTopText = nullptr;
    QLineEdit *m_txtBottomText = nullptr;
    QSpinBox *m_spinStartDur = nullptr;
    QSpinBox *m_spinEndDur = nullptr;
    QCheckBox *m_chkUseText = nullptr;

    QCheckBox *m_chkParallel = nullptr;
    QCheckBox *m_chkCross = nullptr;
    QCheckBox *m_chkColorAna = nullptr;
    QCheckBox *m_chkGrayAna = nullptr;
    QCheckBox *m_chkVr360 = nullptr;
    QCheckBox *m_chkVr360Mono = nullptr;
    QCheckBox *m_chkPingPong = nullptr;
    QDoubleSpinBox *m_spinSpeed = nullptr;
    QSpinBox *m_spinCrf = nullptr;

    QPushButton *m_btnGenerate = nullptr;
    QLabel *m_lblStatus = nullptr;
    QProgressBar *m_progressBar = nullptr;
};

#endif // MAINWINDOW_H
