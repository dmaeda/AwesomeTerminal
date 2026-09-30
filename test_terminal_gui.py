import os
import tempfile
import unittest
from unittest.mock import Mock

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

try:
    from PySide6.QtCore import QSettings
    from PySide6.QtWidgets import QApplication
    from awesome_terminal import AwesomeTerminal
except (ImportError, OSError) as error:
    QApplication = None
    QT_IMPORT_ERROR = str(error)
else:
    QT_IMPORT_ERROR = None


@unittest.skipIf(QApplication is None, f"Qt runtime unavailable: {QT_IMPORT_ERROR}")
class TerminalGuiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])
        QSettings.setDefaultFormat(QSettings.Format.IniFormat)
        QSettings.setPath(
            QSettings.Format.IniFormat,
            QSettings.Scope.UserScope,
            tempfile.mkdtemp(),
        )

    def setUp(self):
        QSettings("AwesomeTerminal", "AwesomeTerminal").clear()
        self.window = AwesomeTerminal()

    def tearDown(self):
        self.window.close()

    def test_window_initializes_with_serial_defaults(self):
        self.assertEqual(self.window.baud.currentText(), "9600")
        self.assertEqual(self.window.data_bits.currentText(), "8")
        self.assertEqual(self.window.line_ending.currentText(), "CRLF")

    def test_invalid_port_settings_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "serial port"):
            self.window._serial_options()
        self.window.port.setCurrentText("/dev/ttyUSB0")
        self.window.baud.setCurrentText("invalid")
        with self.assertRaises(ValueError):
            self.window._serial_options()

    def test_connection_options_and_received_data(self):
        self.window.port.setCurrentText("/dev/ttyUSB0")
        self.window.baud.setCurrentText("115200")
        self.window.flow.setCurrentText("RTS/CTS")
        options = self.window._serial_options()
        self.assertEqual(options["baudrate"], 115200)
        self.assertTrue(options["rtscts"])

        self.window.receive_data(b"OK")
        self.assertEqual(self.window.output.toPlainText(), "OK")
        self.assertIn("RX: 2 bytes", self.window.status.text())

    def test_send_encodes_hex_with_selected_line_ending(self):
        self.window.worker = Mock()
        self.window.worker.isRunning.return_value = True
        self.window.worker.write.side_effect = lambda data: len(data)
        self.window.input_mode.setCurrentText("Hex")
        self.window.input.setText("41 42")
        self.window.line_ending.setCurrentText("CR")
        self.window.send_input()
        self.window.worker.write.assert_called_once_with(b"AB\r")
        self.assertEqual(self.window.tx_bytes, 3)
