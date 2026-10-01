#pragma once

#include <QSerialPort>
#include <QTimer>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSettings;
class QTextEdit;

// Encapsulates a single serial connection along with its UI. One instance of
// TerminalSession is hosted per tab so that multiple connections can be
// managed independently within the same window.
class TerminalSession : public QWidget
{
    Q_OBJECT

public:
    // When restoreOnLoad is true, the session's connection and display
    // options are initialized from the shared settings object at
    // construction time. Additional tabs created during the session pass
    // false so that each new connection starts from sane defaults instead of
    // silently inheriting another tab's in-progress settings. The macro
    // library is always loaded/shared regardless of this flag. Saving is not
    // gated by this flag: callers decide which single session's state should
    // be persisted (see TerminalWindow, which always persists the tab at
    // index 0) so that settings persistence survives that tab being closed.
    explicit TerminalSession(QSettings *settings, bool restoreOnLoad,
                             QWidget *parent = nullptr);

    // Returns a short, human readable label describing this session,
    // suitable for display on its tab (for example the port name or
    // connection state).
    QString tabLabel() const;

    // Stops any pending reconnect attempts and closes the serial port, if
    // open. Should be called before the session's tab is removed or the
    // application exits.
    void disconnectAndClose();

    // Persists the current UI state (connection options, display
    // preferences, fonts, colors) to the shared settings object.
    void saveSettings();

signals:
    void labelChanged(const QString &label);

private slots:
    void connectSerial();
    void receiveData();
    void handleSerialError(QSerialPort::SerialPortError error);
    void sendInput();
    void sendFile();
    void saveLog();
    void saveMacro();
    void selectMacro(int index);

private:
    void buildUi();
    void refreshPorts();
    bool serialOptionsValid();
    bool encodeInput(const QString &text, QByteArray *data, QString *error) const;
    QString formatData(const QByteArray &data, bool hex) const;
    void updateStatus(const QString &state);
    void scheduleReconnect();
    void chooseFont();
    void chooseColor();
    void writeAutomaticLog(const QString &tag, const QByteArray &data);
    void loadSettings();
    void loadMacros();
    void emitLabelChanged();

    QSettings *m_settings;
    bool m_restoreOnLoad;
    int m_sessionId;
    QSerialPort m_serial;
    QTimer m_reconnectTimer;
    quint64 m_txBytes = 0;
    quint64 m_rxBytes = 0;
    bool m_disconnectRequested = false;
    bool m_reconnecting = false;
    bool m_autoLogErrorShown = false;
    QString m_connectionState;

    QComboBox *m_port;
    QComboBox *m_baud;
    QComboBox *m_dataBits;
    QComboBox *m_stopBits;
    QComboBox *m_parity;
    QComboBox *m_flowControl;
    QComboBox *m_displayMode;
    QComboBox *m_inputMode;
    QComboBox *m_lineEnding;
    QComboBox *m_macros;
    QLineEdit *m_input;
    QLineEdit *m_macroName;
    QLineEdit *m_macroText;
    QLineEdit *m_logFilePath;
    QLineEdit *m_rxTag;
    QLineEdit *m_txTag;
    QLineEdit *m_logTimestampFormat;
    QTextEdit *m_output;
    QLabel *m_status;
    QCheckBox *m_timestamps;
    QCheckBox *m_localEcho;
    QCheckBox *m_autoReconnect;
    QCheckBox *m_autoLog;
    QCheckBox *m_logTimestamps;
    QPushButton *m_connectButton;
};
