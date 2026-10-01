#include "terminalwindow.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFontDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QFont>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QSettings>
#include <QStatusBar>
#include <QStringList>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

TerminalWindow::TerminalWindow(QWidget *parent)
    : QMainWindow(parent),
      m_settings(new QSettings(this))
{
    setWindowTitle(tr("AwesomeTerminal"));
    resize(1000, 700);
    buildUi();
    loadSettings();
    refreshPorts();

    m_reconnectTimer.setInterval(3000);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &TerminalWindow::connectSerial);
    connect(&m_serial, &QSerialPort::readyRead, this, &TerminalWindow::receiveData);
    connect(&m_serial, &QSerialPort::errorOccurred,
            this, &TerminalWindow::handleSerialError);
    updateStatus(tr("Disconnected"));
}

void TerminalWindow::buildUi()
{
    auto *root = new QWidget(this);
    auto *layout = new QVBoxLayout(root);
    setCentralWidget(root);

    auto *connectionBox = new QGroupBox(tr("Connection"), root);
    auto *connectionGrid = new QGridLayout(connectionBox);
    m_port = new QComboBox(connectionBox);
    m_port->setEditable(true);
    m_baud = new QComboBox(connectionBox);
    m_baud->setEditable(true);
    m_baud->addItems({QStringLiteral("9600"), QStringLiteral("19200"),
                      QStringLiteral("38400"), QStringLiteral("57600"),
                      QStringLiteral("115200"), QStringLiteral("230400"),
                      QStringLiteral("460800"), QStringLiteral("921600")});
    m_dataBits = new QComboBox(connectionBox);
    m_dataBits->addItems({QStringLiteral("5"), QStringLiteral("6"),
                          QStringLiteral("7"), QStringLiteral("8")});
    m_stopBits = new QComboBox(connectionBox);
    m_stopBits->addItems({QStringLiteral("1"), QStringLiteral("1.5"),
                          QStringLiteral("2")});
    m_parity = new QComboBox(connectionBox);
    m_parity->addItems({tr("None"), tr("Even"), tr("Odd"), tr("Mark"), tr("Space")});
    m_flowControl = new QComboBox(connectionBox);
    m_flowControl->addItems({tr("None"), tr("RTS/CTS"), tr("XON/XOFF")});
    m_connectButton = new QPushButton(tr("Connect"), connectionBox);
    auto *refreshButton = new QPushButton(tr("Refresh"), connectionBox);
    m_autoReconnect = new QCheckBox(tr("Auto-reconnect"), connectionBox);

    const auto addField = [connectionGrid](const QString &label, QWidget *field,
                                           int row, int column) {
        connectionGrid->addWidget(new QLabel(label), row, column * 2);
        connectionGrid->addWidget(field, row, column * 2 + 1);
    };
    addField(tr("Port"), m_port, 0, 0);
    addField(tr("Baud rate"), m_baud, 0, 1);
    addField(tr("Data bits"), m_dataBits, 0, 2);
    addField(tr("Stop bits"), m_stopBits, 0, 3);
    addField(tr("Parity"), m_parity, 1, 0);
    addField(tr("Flow control"), m_flowControl, 1, 1);
    connectionGrid->addWidget(refreshButton, 1, 4);
    connectionGrid->addWidget(m_connectButton, 1, 5);
    connectionGrid->addWidget(m_autoReconnect, 1, 6, 1, 2);
    layout->addWidget(connectionBox);

    auto *displayTools = new QHBoxLayout;
    m_displayMode = new QComboBox(root);
    m_displayMode->addItems({tr("ASCII"), tr("Hex")});
    m_inputMode = new QComboBox(root);
    m_inputMode->addItems({tr("ASCII"), tr("Hex")});
    m_timestamps = new QCheckBox(tr("Timestamps"), root);
    m_localEcho = new QCheckBox(tr("Local echo"), root);
    auto *fontButton = new QPushButton(tr("Font…"), root);
    auto *colorButton = new QPushButton(tr("Text color…"), root);
    auto *saveLogButton = new QPushButton(tr("Save log…"), root);
    auto *clearButton = new QPushButton(tr("Clear"), root);
    displayTools->addWidget(new QLabel(tr("Display"), root));
    displayTools->addWidget(m_displayMode);
    displayTools->addWidget(new QLabel(tr("Input"), root));
    displayTools->addWidget(m_inputMode);
    displayTools->addWidget(m_timestamps);
    displayTools->addWidget(m_localEcho);
    displayTools->addWidget(fontButton);
    displayTools->addWidget(colorButton);
    displayTools->addWidget(saveLogButton);
    displayTools->addWidget(clearButton);
    displayTools->addStretch();
    layout->addLayout(displayTools);

    m_output = new QTextEdit(root);
    m_output->setReadOnly(true);
    m_output->setLineWrapMode(QTextEdit::NoWrap);
    layout->addWidget(m_output, 1);

    auto *sendRow = new QHBoxLayout;
    m_input = new QLineEdit(root);
    m_input->setPlaceholderText(tr("Enter text or hexadecimal bytes"));
    m_lineEnding = new QComboBox(root);
    m_lineEnding->addItems({tr("None"), tr("CR"), tr("LF"), tr("CRLF")});
    auto *sendButton = new QPushButton(tr("Send"), root);
    auto *sendFileButton = new QPushButton(tr("Send file…"), root);
    sendRow->addWidget(m_input, 1);
    sendRow->addWidget(new QLabel(tr("Line ending"), root));
    sendRow->addWidget(m_lineEnding);
    sendRow->addWidget(sendButton);
    sendRow->addWidget(sendFileButton);
    layout->addLayout(sendRow);

    auto *macroBox = new QGroupBox(tr("Macros"), root);
    auto *macroLayout = new QHBoxLayout(macroBox);
    m_macroName = new QLineEdit(macroBox);
    m_macroName->setPlaceholderText(tr("Macro name"));
    m_macroText = new QLineEdit(macroBox);
    m_macroText->setPlaceholderText(tr("Macro text"));
    auto *saveMacroButton = new QPushButton(tr("Save macro"), macroBox);
    m_macros = new QComboBox(macroBox);
    auto *sendMacroButton = new QPushButton(tr("Send macro"), macroBox);
    macroLayout->addWidget(m_macroName);
    macroLayout->addWidget(m_macroText, 1);
    macroLayout->addWidget(saveMacroButton);
    macroLayout->addWidget(m_macros);
    macroLayout->addWidget(sendMacroButton);
    layout->addWidget(macroBox);

    m_status = new QLabel(root);
    statusBar()->addPermanentWidget(m_status, 1);

    connect(refreshButton, &QPushButton::clicked, this, &TerminalWindow::refreshPorts);
    connect(m_connectButton, &QPushButton::clicked, this, [this] {
        if (m_serial.isOpen()) {
            m_disconnectRequested = true;
            m_reconnecting = false;
            m_reconnectTimer.stop();
            m_serial.close();
            m_connectButton->setText(tr("Connect"));
            updateStatus(tr("Disconnected"));
        } else {
            connectSerial();
        }
    });
    connect(m_input, &QLineEdit::returnPressed, this, &TerminalWindow::sendInput);
    connect(sendButton, &QPushButton::clicked, this, &TerminalWindow::sendInput);
    connect(sendFileButton, &QPushButton::clicked, this, &TerminalWindow::sendFile);
    connect(saveLogButton, &QPushButton::clicked, this, &TerminalWindow::saveLog);
    connect(clearButton, &QPushButton::clicked, m_output, &QTextEdit::clear);
    connect(fontButton, &QPushButton::clicked, this, [this] { chooseFont(); });
    connect(colorButton, &QPushButton::clicked, this, [this] { chooseColor(); });
    connect(saveMacroButton, &QPushButton::clicked, this, &TerminalWindow::saveMacro);
    connect(sendMacroButton, &QPushButton::clicked, this, [this] {
        if (m_macroText->text().isEmpty()) {
            QMessageBox::warning(this, tr("Empty macro"), tr("Select or enter a macro first."));
            return;
        }
        m_input->setText(m_macroText->text());
        sendInput();
    });
    connect(m_macros, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &TerminalWindow::selectMacro);
}

void TerminalWindow::refreshPorts()
{
    const QString current = m_port->currentText();
    m_port->clear();
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
        m_port->addItem(info.portName());
    if (!current.isEmpty())
        m_port->setCurrentText(current);
}

bool TerminalWindow::serialOptionsValid()
{
    if (m_port->currentText().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Invalid settings"), tr("Select or enter a serial port."));
        return false;
    }
    bool baudOk = false;
    const int baud = m_baud->currentText().toInt(&baudOk);
    if (!baudOk || baud <= 0) {
        QMessageBox::warning(this, tr("Invalid settings"),
                             tr("Baud rate must be a positive integer."));
        return false;
    }

    m_serial.setPortName(m_port->currentText().trimmed());
    if (!m_serial.setBaudRate(baud)) {
        QMessageBox::warning(this, tr("Invalid settings"), m_serial.errorString());
        return false;
    }
    const QSerialPort::DataBits dataBits[] = {
        QSerialPort::Data5, QSerialPort::Data6, QSerialPort::Data7, QSerialPort::Data8
    };
    if (!m_serial.setDataBits(dataBits[m_dataBits->currentIndex()])) {
        QMessageBox::warning(this, tr("Invalid settings"), m_serial.errorString());
        return false;
    }
    const QString stopBits = m_stopBits->currentText();
    const QSerialPort::StopBits stopValue = stopBits == QStringLiteral("1.5")
            ? QSerialPort::OneAndHalfStop
            : (stopBits == QStringLiteral("2") ? QSerialPort::TwoStop : QSerialPort::OneStop);
    if (!m_serial.setStopBits(stopValue)) {
        QMessageBox::warning(this, tr("Invalid settings"), m_serial.errorString());
        return false;
    }
    const QString parity = m_parity->currentText();
    const QSerialPort::Parity parityValue = parity == tr("Even") ? QSerialPort::EvenParity
            : (parity == tr("Odd") ? QSerialPort::OddParity
            : (parity == tr("Mark") ? QSerialPort::MarkParity
            : (parity == tr("Space") ? QSerialPort::SpaceParity : QSerialPort::NoParity)));
    if (!m_serial.setParity(parityValue)) {
        QMessageBox::warning(this, tr("Invalid settings"), m_serial.errorString());
        return false;
    }
    const QString flow = m_flowControl->currentText();
    const QSerialPort::FlowControl flowValue = flow == tr("RTS/CTS")
            ? QSerialPort::HardwareControl
            : (flow == tr("XON/XOFF") ? QSerialPort::SoftwareControl
                                      : QSerialPort::NoFlowControl);
    if (!m_serial.setFlowControl(flowValue)) {
        QMessageBox::warning(this, tr("Invalid settings"), m_serial.errorString());
        return false;
    }
    return true;
}

void TerminalWindow::connectSerial()
{
    if (m_serial.isOpen())
        return;
    if (!serialOptionsValid()) {
        m_reconnecting = false;
        m_reconnectTimer.stop();
        return;
    }

    m_disconnectRequested = false;
    m_connectButton->setEnabled(false);
    updateStatus(tr("Connecting to %1…").arg(m_serial.portName()));
    if (!m_serial.open(QIODevice::ReadWrite)) {
        const QString error = m_serial.errorString();
        m_connectButton->setEnabled(true);
        updateStatus(tr("Connection error: %1").arg(error));
        if (!m_reconnecting)
            QMessageBox::critical(this, tr("Serial connection failed"), error);
        scheduleReconnect();
        return;
    }
    m_reconnecting = false;
    m_reconnectTimer.stop();
    m_connectButton->setEnabled(true);
    m_connectButton->setText(tr("Disconnect"));
    updateStatus(tr("Connected: %1 @ %2")
                 .arg(m_serial.portName(), m_baud->currentText()));
}

void TerminalWindow::receiveData()
{
    const QByteArray data = m_serial.readAll();
    m_rxBytes += static_cast<quint64>(data.size());

    QString text = formatData(data, m_displayMode->currentText() == tr("Hex"));
    if (m_timestamps->isChecked())
        text.prepend(QStringLiteral("[%1] ")
                     .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"))));
    m_output->moveCursor(QTextCursor::End);
    if (!m_output->document()->isEmpty()) {
        if (m_timestamps->isChecked())
            m_output->insertPlainText(QStringLiteral("\n"));
        else if (m_displayMode->currentText() == tr("Hex"))
            m_output->insertPlainText(QStringLiteral(" "));
    }
    m_output->insertPlainText(text);
    m_output->ensureCursorVisible();
    updateStatus(m_connectionState);
}

void TerminalWindow::handleSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError || error == QSerialPort::NotOpenError
            || !m_serial.isOpen()) {
        return;
    }
    const QString message = m_serial.errorString();
    m_serial.close();
    m_connectButton->setEnabled(true);
    m_connectButton->setText(tr("Connect"));
    updateStatus(tr("Connection error: %1").arg(message));
    if (!m_reconnecting && !m_disconnectRequested)
        QMessageBox::critical(this, tr("Serial connection failed"), message);
    scheduleReconnect();
}

void TerminalWindow::sendInput()
{
    QByteArray data;
    QString error;
    if (!encodeInput(m_input->text(), &data, &error)) {
        QMessageBox::warning(this, tr("Invalid input"), error);
        return;
    }
    const QString ending = m_lineEnding->currentText();
    if (ending == tr("CR") || ending == tr("CRLF"))
        data.append('\r');
    if (ending == tr("LF") || ending == tr("CRLF"))
        data.append('\n');
    if (!m_serial.isOpen()) {
        QMessageBox::warning(this, tr("Not connected"),
                             tr("The serial port is not connected."));
        return;
    }
    const qint64 written = m_serial.write(data);
    if (written < 0) {
        QMessageBox::warning(this, tr("Unable to send"), m_serial.errorString());
        return;
    }
    m_txBytes += static_cast<quint64>(written);
    if (m_localEcho->isChecked()) {
        QString echo = formatData(data, m_displayMode->currentText() == tr("Hex"));
        m_output->moveCursor(QTextCursor::End);
        if (m_displayMode->currentText() == tr("Hex") && !m_output->document()->isEmpty())
            m_output->insertPlainText(QStringLiteral(" "));
        m_output->insertPlainText(echo);
        m_output->ensureCursorVisible();
    }
    m_input->clear();
    updateStatus(m_connectionState);
}

void TerminalWindow::sendFile()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Send file"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Unable to send file"), file.errorString());
        return;
    }
    if (!m_serial.isOpen()) {
        QMessageBox::warning(this, tr("Not connected"),
                             tr("The serial port is not connected."));
        return;
    }
    const QByteArray data = file.readAll();
    const qint64 written = m_serial.write(data);
    if (written < 0) {
        QMessageBox::warning(this, tr("Unable to send file"), m_serial.errorString());
        return;
    }
    m_txBytes += static_cast<quint64>(written);
    updateStatus(m_connectionState);
}

void TerminalWindow::saveLog()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save terminal log"), QStringLiteral("terminal.log"),
        tr("Text files (*.txt *.log);;All files (*)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Unable to save log"), file.errorString());
        return;
    }
    file.write(m_output->toPlainText().toUtf8());
}

bool TerminalWindow::encodeInput(const QString &text, QByteArray *data, QString *error) const
{
    data->clear();
    if (m_inputMode->currentText() == tr("Hex")) {
        QByteArray compact;
        compact.reserve(text.size());
        for (const QChar character : text) {
            if (character.isSpace())
                continue;
            const ushort value = character.unicode();
            if (!((value >= '0' && value <= '9')
                  || (value >= 'a' && value <= 'f')
                  || (value >= 'A' && value <= 'F'))) {
                *error = tr("Enter valid hexadecimal bytes separated by spaces.");
                return false;
            }
            compact.append(static_cast<char>(value));
        }
        if (compact.size() % 2 != 0) {
            *error = tr("Hex input must contain pairs of digits.");
            return false;
        }
        *data = QByteArray::fromHex(compact);
        return true;
    }

    data->reserve(text.size());
    for (const QChar character : text) {
        if (character.unicode() > 0x7f) {
            *error = tr("ASCII input cannot contain non-ASCII characters.");
            return false;
        }
        data->append(static_cast<char>(character.unicode()));
    }
    return true;
}

QString TerminalWindow::formatData(const QByteArray &data, bool hex) const
{
    if (hex) {
        QStringList bytes;
        bytes.reserve(data.size());
        for (const unsigned char value : data)
            bytes.append(QStringLiteral("%1").arg(value, 2, 16, QLatin1Char('0')).toUpper());
        return bytes.join(QLatin1Char(' '));
    }

    QString text;
    text.reserve(data.size());
    for (const unsigned char value : data) {
        text.append(value < 0x80 ? QChar(value) : QChar::ReplacementCharacter);
    }
    return text;
}

void TerminalWindow::saveMacro()
{
    const QString name = m_macroName->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Macro name required"),
                             tr("Enter a name for this macro."));
        return;
    }
    QJsonObject macros = QJsonDocument::fromJson(m_settings->value("macros").toByteArray())
                             .object();
    macros.insert(name, m_macroText->text());
    m_settings->setValue("macros", QJsonDocument(macros).toJson(QJsonDocument::Compact));
    loadMacros();
    m_macros->setCurrentText(name);
}

void TerminalWindow::selectMacro(int index)
{
    const QString name = m_macros->itemText(index);
    if (name.isEmpty())
        return;
    const QJsonObject macros = QJsonDocument::fromJson(
        m_settings->value("macros").toByteArray()).object();
    m_macroName->setText(name);
    m_macroText->setText(macros.value(name).toString());
}

void TerminalWindow::loadMacros()
{
    m_macros->blockSignals(true);
    m_macros->clear();
    const QJsonObject macros = QJsonDocument::fromJson(
        m_settings->value("macros").toByteArray()).object();
    for (auto it = macros.begin(); it != macros.end(); ++it)
        m_macros->addItem(it.key());
    m_macros->blockSignals(false);
}

void TerminalWindow::chooseFont()
{
    bool accepted = false;
    const QFont font = QFontDialog::getFont(&accepted, m_output->font(), this);
    if (accepted)
        m_output->setFont(font);
}

void TerminalWindow::chooseColor()
{
    const QColor color = QColorDialog::getColor(m_output->textColor(), this);
    if (color.isValid())
        m_output->setTextColor(color);
}

void TerminalWindow::updateStatus(const QString &state)
{
    m_connectionState = state;
    m_status->setText(QStringLiteral("%1 | TX: %2 bytes | RX: %3 bytes")
                      .arg(state).arg(m_txBytes).arg(m_rxBytes));
}

void TerminalWindow::scheduleReconnect()
{
    if (m_autoReconnect->isChecked() && !m_disconnectRequested) {
        m_reconnecting = true;
        updateStatus(tr("Disconnected — reconnecting…"));
        m_reconnectTimer.start();
    } else {
        m_reconnecting = false;
    }
}

void TerminalWindow::loadSettings()
{
    m_port->setCurrentText(m_settings->value("port").toString());
    m_baud->setCurrentText(m_settings->value("baud", QStringLiteral("9600")).toString());
    m_dataBits->setCurrentText(m_settings->value("dataBits", QStringLiteral("8")).toString());
    m_stopBits->setCurrentText(m_settings->value("stopBits", QStringLiteral("1")).toString());
    m_parity->setCurrentText(m_settings->value("parity", tr("None")).toString());
    m_flowControl->setCurrentText(m_settings->value("flowControl", tr("None")).toString());
    m_displayMode->setCurrentText(m_settings->value("displayMode", tr("ASCII")).toString());
    m_inputMode->setCurrentText(m_settings->value("inputMode", tr("ASCII")).toString());
    m_lineEnding->setCurrentText(m_settings->value("lineEnding", tr("CRLF")).toString());
    m_timestamps->setChecked(m_settings->value("timestamps", false).toBool());
    m_localEcho->setChecked(m_settings->value("localEcho", false).toBool());
    m_autoReconnect->setChecked(m_settings->value("autoReconnect", false).toBool());
    const QFont font = m_settings->value("font").value<QFont>();
    if (font != QFont())
        m_output->setFont(font);
    const QColor color = m_settings->value("textColor").value<QColor>();
    if (color.isValid())
        m_output->setTextColor(color);
    loadMacros();
}

void TerminalWindow::saveSettings()
{
    m_settings->setValue("port", m_port->currentText());
    m_settings->setValue("baud", m_baud->currentText());
    m_settings->setValue("dataBits", m_dataBits->currentText());
    m_settings->setValue("stopBits", m_stopBits->currentText());
    m_settings->setValue("parity", m_parity->currentText());
    m_settings->setValue("flowControl", m_flowControl->currentText());
    m_settings->setValue("displayMode", m_displayMode->currentText());
    m_settings->setValue("inputMode", m_inputMode->currentText());
    m_settings->setValue("lineEnding", m_lineEnding->currentText());
    m_settings->setValue("timestamps", m_timestamps->isChecked());
    m_settings->setValue("localEcho", m_localEcho->isChecked());
    m_settings->setValue("autoReconnect", m_autoReconnect->isChecked());
    m_settings->setValue("font", m_output->font());
    m_settings->setValue("textColor", m_output->textColor());
}

void TerminalWindow::closeEvent(QCloseEvent *event)
{
    m_disconnectRequested = true;
    m_reconnectTimer.stop();
    if (m_serial.isOpen())
        m_serial.close();
    saveSettings();
    event->accept();
}
