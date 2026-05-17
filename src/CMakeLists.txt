cmake_minimum_required(VERSION 3.16)
project(WM LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets SerialPort PrintSupport)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../bin)

add_executable(WM
    main.cpp
    wm.cpp
    wm.h
    wm.ui
    oscilloscope.cpp
    oscilloscope.h
    powermeter.cpp
    powermeter.h
    pulser.cpp
    pulser.h
    qcustomplot.cpp
    qcustomplot.h
)

target_link_libraries(WM PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::SerialPort
    Qt6::PrintSupport
    /opt/picoscope/lib/libps2000.so
)

target_include_directories(WM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    /opt/picoscope/include/libps2000
)
