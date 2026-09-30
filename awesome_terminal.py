import json
import sys

import serial
from serial.tools import list_ports
from PySide6.QtCore import QSettings, QThread, QTimer, Signal
from PySide6.QtGui import QColor, QFont, QFontDialog
from PySide6.QtWidgets import (
    QApplication,
    QCheckBox,
    QColorDialog,
    QComboBox,
    QFileDialog,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)

from terminal_core import add_line_ending, encode_input, format_received


class SerialReader(QThread):
    opened = Signal()
    received = Signal(bytes)
    failed = Signal(str)
    disconnected = Signal()

    def __init__(self, options, parent=None):
        super().__init__(parent)
        self.options = options
        self._port = None
        self._stop_requested = False

    def run(self):
        try:
            self._port = serial.Serial(timeout=0.1, **self.options)
        except (serial.SerialException, OSError, ValueError) as error:
            self.failed.emit(str(error))
            return

        self.opened.emit()
        while not self._stop_requested:
            try:
                waiting = self._port.in_waiting
                data = self._port.read(waiting or 1)
                if data:
                    self.received.emit(data)
            except (serial.SerialException, OSError) as error:
                self.failed.emit(str(error))
                break

        if self._port and self._port.is_open:
            self._port.close()
        self.disconnected.emit()

    def write(self, data):
        if self._port and self._port.is_open:
            return self._port.write(data)
        raise serial.SerialException("The serial port is not connected.")

    def stop(self):
        self._stop_requested = True


class AwesomeTerminal(QMainWindow):
    def __init__(self):
        super().__init__()
        self.settings = QSettings("AwesomeTerminal", "AwesomeTerminal")
        self.worker = None
        self.tx_bytes = 0
        self.rx_bytes = 0
        self._disconnect_requested = False
        self._reconnecting = False
        self.setWindowTitle("AwesomeTerminal")
        self.resize(1000, 700)
        self._build_ui()
        self._load_settings()
        self.refresh_ports()
        self.reconnect_timer = QTimer(self)
        self.reconnect_timer.setInterval(3000)
        self.reconnect_timer.timeout.connect(self.connect_serial)
        self._update_status("Disconnected")

    def _build_ui(self):
        root = QWidget()
        layout = QVBoxLayout(root)
        self.setCentralWidget(root)

        settings_box = QGroupBox("Connection")
        settings_grid = QGridLayout(settings_box)
        self.port = QComboBox()
        self.port.setEditable(True)
        self.refresh_button = QPushButton("Refresh")
        self.refresh_button.clicked.connect(self.refresh_ports)
        self.baud = QComboBox()
        self.baud.setEditable(True)
        self.baud.addItems(
            ["9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"]
        )
        self.data_bits = QComboBox()
        self.data_bits.addItems(["5", "6", "7", "8"])
        self.stop_bits = QComboBox()
        self.stop_bits.addItems(["1", "1.5", "2"])
        self.parity = QComboBox()
        self.parity.addItems(["None", "Even", "Odd", "Mark", "Space"])
        self.flow = QComboBox()
        self.flow.addItems(["None", "RTS/CTS", "XON/XOFF"])
        self.connect_button = QPushButton("Connect")
        self.connect_button.clicked.connect(self.toggle_connection)
        self.auto_reconnect = QCheckBox("Auto-reconnect")

        fields = [
            ("Port", self.port),
            ("Baud rate", self.baud),
            ("Data bits", self.data_bits),
            ("Stop bits", self.stop_bits),
            ("Parity", self.parity),
            ("Flow control", self.flow),
        ]
        for index, (label, widget) in enumerate(fields):
            row, column = divmod(index, 4)
            settings_grid.addWidget(QLabel(label), row, column * 2)
            settings_grid.addWidget(widget, row, column * 2 + 1)
        settings_grid.addWidget(self.refresh_button, 2, 0)
        settings_grid.addWidget(self.connect_button, 2, 1)
        settings_grid.addWidget(self.auto_reconnect, 2, 2, 1, 2)
        layout.addWidget(settings_box)

        display_tools = QHBoxLayout()
        self.display_mode = QComboBox()
        self.display_mode.addItems(["ASCII", "Hex"])
        self.input_mode = QComboBox()
        self.input_mode.addItems(["ASCII", "Hex"])
        self.timestamps = QCheckBox("Timestamps")
        self.local_echo = QCheckBox("Local echo")
        self.font_button = QPushButton("Font…")
        self.font_button.clicked.connect(self.choose_font)
        self.color_button = QPushButton("Text color…")
        self.color_button.clicked.connect(self.choose_color)
        self.clear_button = QPushButton("Clear")
        self.clear_button.clicked.connect(lambda: self.output.clear())
        self.save_log_button = QPushButton("Save log…")
        self.save_log_button.clicked.connect(self.save_log)
        for label, widget in (
            ("Display", self.display_mode),
            ("Input", self.input_mode),
            ("", self.timestamps),
            ("", self.local_echo),
            ("", self.font_button),
            ("", self.color_button),
            ("", self.save_log_button),
            ("", self.clear_button),
        ):
            if label:
                display_tools.addWidget(QLabel(label))
            display_tools.addWidget(widget)
        display_tools.addStretch()
        layout.addLayout(display_tools)

        self.output = QTextEdit()
        self.output.setReadOnly(True)
        self.output.setLineWrapMode(QTextEdit.LineWrapMode.NoWrap)
        layout.addWidget(self.output, 1)

        send_row = QHBoxLayout()
        self.input = QLineEdit()
        self.input.setPlaceholderText("Enter text or hexadecimal bytes")
        self.input.returnPressed.connect(self.send_input)
        self.line_ending = QComboBox()
        self.line_ending.addItems(["None", "CR", "LF", "CRLF"])
        self.send_button = QPushButton("Send")
        self.send_button.clicked.connect(self.send_input)
        self.send_file_button = QPushButton("Send file…")
        self.send_file_button.clicked.connect(self.send_file)
        send_row.addWidget(self.input, 1)
        send_row.addWidget(QLabel("Line ending"))
        send_row.addWidget(self.line_ending)
        send_row.addWidget(self.send_button)
        send_row.addWidget(self.send_file_button)
        layout.addLayout(send_row)

        macro_box = QGroupBox("Macros")
        macro_layout = QHBoxLayout(macro_box)
        self.macro_name = QLineEdit()
        self.macro_name.setPlaceholderText("Macro name")
        self.macro_text = QLineEdit()
        self.macro_text.setPlaceholderText("Macro text")
        self.save_macro_button = QPushButton("Save macro")
        self.save_macro_button.clicked.connect(self.save_macro)
        self.macros = QComboBox()
        self.macros.currentIndexChanged.connect(self.select_macro)
        self.send_macro_button = QPushButton("Send macro")
        self.send_macro_button.clicked.connect(self.send_macro)
        macro_layout.addWidget(self.macro_name)
        macro_layout.addWidget(self.macro_text, 1)
        macro_layout.addWidget(self.save_macro_button)
        macro_layout.addWidget(self.macros)
        macro_layout.addWidget(self.send_macro_button)
        layout.addWidget(macro_box)

        self.status = QLabel()
        layout.addWidget(self.status)

    def refresh_ports(self):
        current = self.port.currentText()
        ports = [item.device for item in list_ports.comports()]
        self.port.clear()
        self.port.addItems(ports)
        if current:
            self.port.setCurrentText(current)

    def _serial_options(self):
        port_name = self.port.currentText().strip()
        if not port_name:
            raise ValueError("Select or enter a serial port.")
        baud_rate = int(self.baud.currentText())
        if baud_rate <= 0:
            raise ValueError("Baud rate must be greater than zero.")
        bytesizes = {
            "5": serial.FIVEBITS,
            "6": serial.SIXBITS,
            "7": serial.SEVENBITS,
            "8": serial.EIGHTBITS,
        }
        stopbits = {
            "1": serial.STOPBITS_ONE,
            "1.5": serial.STOPBITS_ONE_POINT_FIVE,
            "2": serial.STOPBITS_TWO,
        }
        parities = {
            "None": serial.PARITY_NONE,
            "Even": serial.PARITY_EVEN,
            "Odd": serial.PARITY_ODD,
            "Mark": serial.PARITY_MARK,
            "Space": serial.PARITY_SPACE,
        }
        flow_control = self.flow.currentText()
        return {
            "port": port_name,
            "baudrate": baud_rate,
            "bytesize": bytesizes[self.data_bits.currentText()],
            "stopbits": stopbits[self.stop_bits.currentText()],
            "parity": parities[self.parity.currentText()],
            "rtscts": flow_control == "RTS/CTS",
            "xonxoff": flow_control == "XON/XOFF",
        }

    def toggle_connection(self):
        if self.worker and self.worker.isRunning():
            self._disconnect_requested = True
            self.reconnect_timer.stop()
            self.worker.stop()
            self._update_status("Disconnecting…")
            return
        self.connect_serial()

    def connect_serial(self):
        if self.worker and self.worker.isRunning():
            return
        try:
            options = self._serial_options()
        except (ValueError, KeyError) as error:
            if not self._reconnecting:
                QMessageBox.warning(self, "Invalid settings", str(error))
            self._reconnecting = False
            self.reconnect_timer.stop()
            return
        self._disconnect_requested = False
        self.worker = SerialReader(options, self)
        self.worker.opened.connect(self._connected)
        self.worker.received.connect(self.receive_data)
        self.worker.failed.connect(self._connection_failed)
        self.worker.disconnected.connect(self._disconnected)
        self.connect_button.setEnabled(False)
        self._update_status(f"Connecting to {options['port']}…")
        self.worker.start()

    def _connected(self):
        self._reconnecting = False
        self.reconnect_timer.stop()
        self.connect_button.setEnabled(True)
        self.connect_button.setText("Disconnect")
        self._update_status(f"Connected: {self.port.currentText()} @ {self.baud.currentText()}")

    def _connection_failed(self, message):
        self.connect_button.setEnabled(True)
        self._update_status(f"Connection error: {message}")
        if not self._reconnecting:
            QMessageBox.critical(self, "Serial connection failed", message)
        self._schedule_reconnect()

    def _disconnected(self):
        self.connect_button.setEnabled(True)
        self.connect_button.setText("Connect")
        if self._disconnect_requested:
            self._update_status("Disconnected")
        self._schedule_reconnect()

    def _schedule_reconnect(self):
        if self.auto_reconnect.isChecked() and not self._disconnect_requested:
            self._reconnecting = True
            self._update_status("Disconnected — reconnecting…")
            self.reconnect_timer.start()

    def receive_data(self, data):
        self.rx_bytes += len(data)
        text = format_received(
            data,
            hex_mode=self.display_mode.currentText() == "Hex",
            show_timestamp=self.timestamps.isChecked(),
        )
        self.output.moveCursor(self.output.textCursor().MoveOperation.End)
        if self.output.toPlainText() and self.timestamps.isChecked():
            self.output.insertPlainText("\n")
        elif self.output.toPlainText() and self.display_mode.currentText() == "Hex":
            self.output.insertPlainText(" ")
        self.output.insertPlainText(text)
        self.output.ensureCursorVisible()
        self._update_counters()

    def send_input(self):
        try:
            data = encode_input(self.input.text(), self.input_mode.currentText() == "Hex")
            data = add_line_ending(data, self.line_ending.currentText())
            self._send(data)
            if self.local_echo.isChecked():
                if self.output.toPlainText() and self.display_mode.currentText() == "Hex":
                    self.output.insertPlainText(" ")
                self.output.insertPlainText(
                    format_received(data, self.display_mode.currentText() == "Hex")
                )
                self.output.ensureCursorVisible()
            self.input.clear()
        except ValueError as error:
            QMessageBox.warning(self, "Invalid input", str(error))
        except (serial.SerialException, OSError) as error:
            QMessageBox.warning(self, "Not connected", str(error))

    def _send(self, data):
        if not self.worker or not self.worker.isRunning():
            raise serial.SerialException("The serial port is not connected.")
        written = self.worker.write(data)
        self.tx_bytes += written
        self._update_counters()

    def send_file(self):
        path, _ = QFileDialog.getOpenFileName(self, "Send file")
        if not path:
            return
        try:
            with open(path, "rb") as source:
                self._send(source.read())
        except (OSError, serial.SerialException) as error:
            QMessageBox.warning(self, "Unable to send file", str(error))

    def save_log(self):
        path, _ = QFileDialog.getSaveFileName(
            self, "Save terminal log", "terminal.log", "Text files (*.txt *.log);;All files (*)"
        )
        if not path:
            return
        try:
            with open(path, "w", encoding="utf-8") as log_file:
                log_file.write(self.output.toPlainText())
        except OSError as error:
            QMessageBox.warning(self, "Unable to save log", str(error))

    def save_macro(self):
        name = self.macro_name.text().strip()
        if not name:
            QMessageBox.warning(self, "Macro name required", "Enter a name for this macro.")
            return
        macros = self._macros()
        macros[name] = self.macro_text.text()
        self.settings.setValue("macros", json.dumps(macros))
        self.macros.blockSignals(True)
        self.macros.clear()
        self.macros.addItems(macros)
        self.macros.setCurrentText(name)
        self.macros.blockSignals(False)

    def _macros(self):
        try:
            value = json.loads(self.settings.value("macros", "{}"))
            return value if isinstance(value, dict) else {}
        except (TypeError, json.JSONDecodeError):
            return {}

    def select_macro(self, index):
        name = self.macros.itemText(index)
        if name:
            self.macro_name.setText(name)
            self.macro_text.setText(self._macros().get(name, ""))

    def send_macro(self):
        if not self.macro_text.text():
            QMessageBox.warning(self, "Empty macro", "Select or enter a macro first.")
            return
        self.input.setText(self.macro_text.text())
        self.send_input()

    def choose_font(self):
        font, accepted = QFontDialog.getFont(self.output.font(), self)
        if accepted:
            self.output.setFont(font)

    def choose_color(self):
        color = QColorDialog.getColor(self.output.textColor(), self)
        if color.isValid():
            self.output.setTextColor(color)

    def _update_counters(self):
        state = self.status.text().split(" | TX:", 1)[0]
        self.status.setText(f"{state} | TX: {self.tx_bytes} bytes | RX: {self.rx_bytes} bytes")

    def _update_status(self, state):
        self.status.setText(f"{state} | TX: {self.tx_bytes} bytes | RX: {self.rx_bytes} bytes")

    def _load_settings(self):
        defaults = {
            "baud": "9600",
            "data_bits": "8",
            "stop_bits": "1",
            "parity": "None",
            "flow": "None",
            "line_ending": "CRLF",
            "display_mode": "ASCII",
            "input_mode": "ASCII",
        }
        for key, default in defaults.items():
            widget = {
                "baud": self.baud,
                "data_bits": self.data_bits,
                "stop_bits": self.stop_bits,
                "parity": self.parity,
                "flow": self.flow,
                "line_ending": self.line_ending,
                "display_mode": self.display_mode,
                "input_mode": self.input_mode,
            }[key]
            widget.setCurrentText(self.settings.value(key, default))
        self.port.setCurrentText(self.settings.value("port", ""))
        self.timestamps.setChecked(self.settings.value("timestamps", False, type=bool))
        self.local_echo.setChecked(self.settings.value("local_echo", False, type=bool))
        self.auto_reconnect.setChecked(
            self.settings.value("auto_reconnect", False, type=bool)
        )
        font = self.settings.value("font")
        if isinstance(font, QFont):
            self.output.setFont(font)
        color = self.settings.value("text_color")
        if color:
            self.output.setTextColor(QColor(color))
        for name in self._macros():
            self.macros.addItem(name)

    def closeEvent(self, event):
        for key, widget in (
            ("baud", self.baud),
            ("data_bits", self.data_bits),
            ("stop_bits", self.stop_bits),
            ("parity", self.parity),
            ("flow", self.flow),
            ("line_ending", self.line_ending),
            ("display_mode", self.display_mode),
            ("input_mode", self.input_mode),
        ):
            self.settings.setValue(key, widget.currentText())
        self.settings.setValue("port", self.port.currentText())
        self.settings.setValue("timestamps", self.timestamps.isChecked())
        self.settings.setValue("local_echo", self.local_echo.isChecked())
        self.settings.setValue("auto_reconnect", self.auto_reconnect.isChecked())
        self.settings.setValue("font", self.output.font())
        self.settings.setValue("text_color", self.output.textColor().name())
        self._disconnect_requested = True
        self.reconnect_timer.stop()
        if self.worker and self.worker.isRunning():
            self.worker.stop()
            self.worker.wait(1000)
        event.accept()


if __name__ == "__main__":
    app = QApplication(sys.argv)
    app.setApplicationName("AwesomeTerminal")
    app.setOrganizationName("AwesomeTerminal")
    window = AwesomeTerminal()
    window.show()
    sys.exit(app.exec())
