#include "terminalwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("AwesomeTerminal"));
    QCoreApplication::setApplicationName(QStringLiteral("AwesomeTerminal"));

    TerminalWindow window;
    window.show();
    return application.exec();
}
