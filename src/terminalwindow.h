#pragma once

#include <QMainWindow>
#include <QSerialPort>
#include <QTimer>

class QCheckBox;
class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSettings;
class QTextEdit;

class TerminalWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit TerminalWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

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
    void saveSettings();
    void loadMacros();

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
