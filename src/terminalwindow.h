#pragma once

#include <QMainWindow>

class QCloseEvent;
class QSettings;
class QTabWidget;
class TerminalSession;

// Hosts one or more TerminalSession widgets in a tabbed interface so that
// multiple terminal connections can be open and managed at the same time.
class TerminalWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit TerminalWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void newTab();
    void closeTab(int index);

private:
    void buildUi();
    void addSession(bool restoreOnLoad);

    QSettings *m_settings;
    QTabWidget *m_tabs;
};
