# AwesomeTerminal

AwesomeTerminal is a C++ serial terminal application for Windows and Linux,
with a graphical interface built using Qt.

## Project goals

- Provide a serial terminal with a graphical user interface.
- Support Windows and Linux.
- Use C++ and Qt for the GUI.

## Build and run

Install Qt 6 with the Widgets and SerialPort modules, plus CMake and a C++17
compiler. Build from the repository root with:

```sh
cmake -S . -B build
cmake --build build
```

Run `build/awesome_terminal` on Linux or `build/awesome_terminal.exe` on Windows.

The GUI supports serial port configuration, connecting and disconnecting,
ASCII/hex send and receive, line endings, local echo, timestamps, log saving,
file transfer, saved macros, automatic reconnection, and persistent settings.
Use the controls above the terminal to configure the connection and display.
Ports can be selected from the list or entered manually.
