cmake_minimum_required(VERSION 3.16)
project(AwesomeTerminal VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets SerialPort)

qt_add_executable(awesome_terminal
    src/main.cpp
    src/terminalwindow.cpp
    src/terminalwindow.h
    src/terminalsession.cpp
    src/terminalsession.h
)

target_link_libraries(awesome_terminal PRIVATE Qt6::Widgets Qt6::SerialPort)
