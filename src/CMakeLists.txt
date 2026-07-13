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
)

target_link_libraries(WM PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::SerialPort
    /opt/picoscope/lib/libps2000.so
    ${LIBUSB_LIBRARIES}
)

target_include_directories(WM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    /opt/picoscope/include/libps2000
    ${LIBUSB_INCLUDE_DIRS}
)

install(TARGETS WM DESTINATION bin)
install(FILES
    ${CMAKE_SOURCE_DIR}/../README.md
    ${CMAKE_SOURCE_DIR}/../commands_pulser.md
    ${CMAKE_SOURCE_DIR}/../commands_pm100a.md
    ${CMAKE_SOURCE_DIR}/../commands_ps2000.md
    DESTINATION share/doc/WM
)
