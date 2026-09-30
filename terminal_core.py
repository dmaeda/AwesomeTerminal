"""Byte conversion helpers shared by the terminal UI and its tests."""

from datetime import datetime


def encode_input(text, hex_mode=False):
    if hex_mode:
        try:
            return bytes.fromhex(text)
        except ValueError as error:
            raise ValueError("Enter valid hexadecimal bytes separated by spaces.") from error
    try:
        return text.encode("ascii")
    except UnicodeEncodeError as error:
        raise ValueError("ASCII input cannot contain non-ASCII characters.") from error


def add_line_ending(data, line_ending):
    endings = {"None": b"", "CR": b"\r", "LF": b"\n", "CRLF": b"\r\n"}
    return data + endings[line_ending]


def format_received(data, hex_mode=False, show_timestamp=False):
    if hex_mode:
        text = data.hex(" ").upper()
    else:
        text = data.decode("ascii", errors="replace")
    if show_timestamp:
        return f"[{datetime.now().strftime('%H:%M:%S.%f')[:-3]}] {text}"
    return text
