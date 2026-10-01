# AwesomeTerminal

AwesomeTerminal is a C++ serial terminal application for Windows and Linux,
with a graphical interface built using Qt.

## Project goals

- Provide a serial terminal with a graphical user interface.
- Support Windows and Linux.
- Use C++ and Qt for the GUI.

## Build and run

### Requirements

- Qt 6.2 or later, including the **Widgets** and **SerialPort** modules.
- CMake 3.16 or later.
- A C++17 compiler (for example, GCC on Linux or MSVC on Windows).

On Debian or Ubuntu, install the build tools and Qt development modules with:

```sh
sudo apt install build-essential cmake qt6-base-dev qt6-serialport-dev
```

On Fedora, install the equivalent packages with:

```sh
sudo dnf install gcc-c++ cmake qt6-qtbase-devel qt6-qtserialport-devel
```

On Windows, install Qt 6.2 or later with the Widgets and SerialPort modules,
along with CMake and an MSVC C++17 compiler (available through Visual Studio
or the Visual Studio Build Tools). Make sure the Qt version and compiler kit
match.

### Build

Run these commands from the repository root:

```sh
cmake -S . -B build
cmake --build build
```

Run `build/awesome_terminal` on Linux. On Windows, run
`build\awesome_terminal.exe` with a single-configuration generator, or
`build\Debug\awesome_terminal.exe` when using a multi-configuration generator
such as Visual Studio.

The GUI supports serial port configuration, connecting and disconnecting,
ASCII/hex send and receive, line endings, local echo, timestamps, log saving,
file transfer, saved macros, automatic reconnection, and persistent settings.
Use the controls above the terminal to configure the connection and display.
Ports can be selected from the list or entered manually.
