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
    explicit TerminalSession(QSettings *settings, QWidget *parent = nullptr);

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
    void loadSettings();
    void loadMacros();
    void emitLabelChanged();

    QSettings *m_settings;
    QSerialPort m_serial;
    QTimer m_reconnectTimer;
    quint64 m_txBytes = 0;
    quint64 m_rxBytes = 0;
    bool m_disconnectRequested = false;
    bool m_reconnecting = false;
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
    QTextEdit *m_output;
    QLabel *m_status;
    QCheckBox *m_timestamps;
    QCheckBox *m_localEcho;
    QCheckBox *m_autoReconnect;
    QPushButton *m_connectButton;
};
