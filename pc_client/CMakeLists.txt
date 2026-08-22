cmake_minimum_required(VERSION 3.16)
project(pc_client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
endif()

# Deliberately independent of the main src/CMakeLists.txt project: this is
# the PC-side client (Linux/Mac/Windows), talking to wm_headless on the Pi
# over TCP. No libusb, no libps2000, no QtSerialPort — those are all on the
# Pi side of the link, not here.
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../bin)

add_executable(pc_client
    main.cpp
    pcclient.cpp
    pcclient.h
    ../src/plotwidget.cpp
    ../src/plotwidget.h
    ../src/stepper.cpp
    ../src/stepper.h
    ../src/virtualnumberpad.cpp
    ../src/virtualnumberpad.h
    ../src/numberinputdialog.cpp
    ../src/numberinputdialog.h
)

target_link_libraries(pc_client PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

target_include_directories(pc_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
