cmake_minimum_required(VERSION 3.16)
project(WM LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
endif()
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets SerialPort)

# libusb-1.0 — power meter talks USBTMC directly over libusb (not the kernel
# usbtmc driver, which fails on the PM101A).  On the Pi: apt install libusb-1.0-0-dev
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

# libps2000 (PicoScope) — installs to /opt/picoscope on the dev box but to a
# system path (/usr/lib, /usr/lib/<triplet>) from the PicoTech apt repo on the Pi.
find_library(PS2000_LIB NAMES ps2000 libps2000
    PATHS /opt/picoscope/lib /usr/lib /usr/local/lib
    PATH_SUFFIXES aarch64-linux-gnu arm-linux-gnueabihf x86_64-linux-gnu)
find_path(PS2000_INC NAMES ps2000.h libps2000/ps2000.h
    PATHS /opt/picoscope/include /opt/picoscope/include/libps2000
          /usr/include /usr/include/libps2000)
if(NOT PS2000_LIB OR NOT PS2000_INC)
    message(FATAL_ERROR "libps2000 not found. Install the PicoScope driver "
        "(PicoTech apt repo, or /opt/picoscope). Got LIB='${PS2000_LIB}' INC='${PS2000_INC}'")
endif()
message(STATUS "libps2000: ${PS2000_LIB}  (headers: ${PS2000_INC})")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../bin)

add_executable(WM
    main.cpp
    wm.cpp
    wm.h
    plotwidget.cpp
    plotwidget.h
    stepper.cpp
    stepper.h
    oscilloscope.cpp
    oscilloscope.h
    powermeter.cpp
    powermeter.h
    pulser.cpp
    pulser.h
    virtualkeyboard.cpp
    virtualkeyboard.h
    savefiledialog.cpp
    savefiledialog.h
    virtualnumberpad.cpp
    virtualnumberpad.h
    numberinputdialog.cpp
    numberinputdialog.h
)

target_link_libraries(WM PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::SerialPort
    ${PS2000_LIB}
    ${LIBUSB_LIBRARIES}
)

target_include_directories(WM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PS2000_INC}
    ${LIBUSB_INCLUDE_DIRS}
)

install(TARGETS WM DESTINATION bin)
install(FILES
    ${CMAKE_SOURCE_DIR}/../README.md
    ${CMAKE_SOURCE_DIR}/../docs/commands_pulser.md
    ${CMAKE_SOURCE_DIR}/../docs/commands_pm100a.md
    ${CMAKE_SOURCE_DIR}/../docs/commands_ps2000.md
    DESTINATION share/doc/WM
)
