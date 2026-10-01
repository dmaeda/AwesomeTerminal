#include "terminalwindow.h"

#include "terminalsession.h"

#include <QAction>
#include <QCloseEvent>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QTabWidget>
#include <QToolButton>

TerminalWindow::TerminalWindow(QWidget *parent)
    : QMainWindow(parent),
      m_settings(new QSettings(this))
{
    setWindowTitle(tr("AwesomeTerminal"));
    resize(1000, 700);
    buildUi();
    addSession(/*restoreOnLoad=*/true);
}

void TerminalWindow::buildUi()
{
    m_tabs = new QTabWidget(this);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    setCentralWidget(m_tabs);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &TerminalWindow::closeTab);

    auto *newTabButton = new QToolButton(m_tabs);
    newTabButton->setText(tr("+"));
    newTabButton->setToolTip(tr("New tab"));
    connect(newTabButton, &QToolButton::clicked, this, &TerminalWindow::newTab);
    m_tabs->setCornerWidget(newTabButton, Qt::TopRightCorner);

    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    auto *newTabAction = fileMenu->addAction(tr("&New Tab"), this, &TerminalWindow::newTab);
    newTabAction->setShortcut(QKeySequence::AddTab);
    auto *closeTabAction = fileMenu->addAction(tr("&Close Tab"), this, [this] {
        if (m_tabs->count() > 0)
            closeTab(m_tabs->currentIndex());
    });
    closeTabAction->setShortcut(QKeySequence::Close);
    fileMenu->addSeparator();
    auto *quitAction = fileMenu->addAction(tr("&Quit"), this, &TerminalWindow::close);
    quitAction->setShortcut(QKeySequence::Quit);
}

void TerminalWindow::newTab()
{
    addSession(/*restoreOnLoad=*/false);
}

void TerminalWindow::addSession(bool restoreOnLoad)
{
    // Only the very first tab created at startup loads its connection and
    // display options from m_settings; additional tabs start from built-in
    // defaults so that opening a new tab never leaks another tab's
    // in-progress state. For saving, TerminalWindow always persists the
    // settings of whichever session is currently at tab index 0, so settings
    // persistence survives that original tab being closed (see closeTab()
    // and closeEvent()).
    auto *session = new TerminalSession(m_settings, restoreOnLoad, m_tabs);
    const int index = m_tabs->addTab(session, session->tabLabel());
    connect(session, &TerminalSession::labelChanged, this, [this, session](const QString &label) {
        const int tabIndex = m_tabs->indexOf(session);
        if (tabIndex != -1)
            m_tabs->setTabText(tabIndex, label);
    });
    m_tabs->setCurrentIndex(index);
}

void TerminalWindow::closeTab(int index)
{
    auto *session = qobject_cast<TerminalSession *>(m_tabs->widget(index));
    if (!session)
        return;
    session->disconnectAndClose();
    if (index == 0)
        session->saveSettings();
    m_tabs->removeTab(index);
    session->deleteLater();

    // Always keep at least one tab open so the window remains usable.
    if (m_tabs->count() == 0)
        addSession(/*restoreOnLoad=*/false);
}

void TerminalWindow::closeEvent(QCloseEvent *event)
{
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (auto *session = qobject_cast<TerminalSession *>(m_tabs->widget(i))) {
            session->disconnectAndClose();
            if (i == 0)
                session->saveSettings();
        }
    }
    event->accept();
}
