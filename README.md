# AwesomeTerminal

AwesomeTerminal is a serial terminal application intended to run on Windows
and Linux. It provides a graphical interface built with Qt for working with
serial connections.

## Project goals

- Provide a serial terminal with a graphical user interface.
- Support Windows and Linux.
- Use Qt for the GUI.

## Run the application

Install Python 3.9 or later, then install the application dependencies and run:

```sh
python -m pip install -r requirements.txt
python awesome_terminal.py
```

On Linux, install the Qt platform libraries provided by your distribution,
including `libEGL` (for example, `sudo apt install libegl1` on Debian/Ubuntu).

The GUI supports serial port configuration, connecting and disconnecting,
ASCII/hex send and receive, line endings, local echo, timestamps, log saving,
file transfer, saved macros, automatic reconnection, and persistent settings.
Use the controls above the terminal to configure the connection and display.
Ports can be selected from the list or entered manually.

Run the byte-conversion tests with:

```sh
python -m unittest
```
