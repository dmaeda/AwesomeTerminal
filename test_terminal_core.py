import unittest

from terminal_core import add_line_ending, encode_input, format_received


class TerminalCoreTests(unittest.TestCase):
    def test_ascii_input_encodes_ascii_and_rejects_non_ascii(self):
        self.assertEqual(encode_input("Hello"), b"Hello")
        with self.assertRaises(ValueError):
            encode_input("café")

    def test_hex_input_accepts_whitespace_and_rejects_invalid_data(self):
        self.assertEqual(encode_input("48 65 6c", hex_mode=True), b"Hel")
        with self.assertRaises(ValueError):
            encode_input("not hex", hex_mode=True)

    def test_line_endings(self):
        self.assertEqual(add_line_ending(b"data", "None"), b"data")
        self.assertEqual(add_line_ending(b"data", "CR"), b"data\r")
        self.assertEqual(add_line_ending(b"data", "LF"), b"data\n")
        self.assertEqual(add_line_ending(b"data", "CRLF"), b"data\r\n")

    def test_received_data_can_be_formatted_as_ascii_or_hex(self):
        self.assertEqual(format_received(b"Hi"), "Hi")
        self.assertEqual(format_received(b"\x00\xff", hex_mode=True), "00 FF")
        self.assertIn("] Hi", format_received(b"Hi", show_timestamp=True))


if __name__ == "__main__":
    unittest.main()
