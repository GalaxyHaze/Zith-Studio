#include <QApplication>
#include <QPalette>

#include "editor/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Zith Studio");
    app.setOrganizationName("Zith");
    app.setWindowIcon(createZithIcon());

    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor("#11111b"));
    darkPalette.setColor(QPalette::WindowText, QColor("#c6d0f5"));
    darkPalette.setColor(QPalette::Base, QColor("#11111b"));
    darkPalette.setColor(QPalette::AlternateBase, QColor("#1e1e2e"));
    darkPalette.setColor(QPalette::ToolTipBase, QColor("#1e1e2e"));
    darkPalette.setColor(QPalette::ToolTipText, QColor("#c6d0f5"));
    darkPalette.setColor(QPalette::Text, QColor("#c6d0f5"));
    darkPalette.setColor(QPalette::Button, QColor("#1e1e2e"));
    darkPalette.setColor(QPalette::ButtonText, QColor("#c6d0f5"));
    darkPalette.setColor(QPalette::BrightText, QColor("#e64553"));
    darkPalette.setColor(QPalette::Link, QColor("#7287fd"));
    darkPalette.setColor(QPalette::Highlight, QColor("#209fb5"));
    darkPalette.setColor(QPalette::HighlightedText, QColor("#c6d0f5"));
    app.setPalette(darkPalette);

    MainWindow mainWin;
    mainWin.show();

    return app.exec();
}
