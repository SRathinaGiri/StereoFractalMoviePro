#include <QApplication>
#include <QLocale>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    // Force universal standard English (C) locale for numbers, punctuation, and widgets
    QLocale englishLocale(QLocale::English, QLocale::UnitedStates);
    englishLocale.setNumberOptions(QLocale::OmitGroupSeparator | QLocale::RejectLeadingZeroInExponent);
    QLocale::setDefault(englishLocale);

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("StereoMoviePro"));
    app.setApplicationDisplayName(QStringLiteral("Stereo Fractal Movie Pro"));

    MainWindow window;
    window.show();

    return app.exec();
}
