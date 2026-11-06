cmake_minimum_required(VERSION 3.20)
project(StereoMoviePro VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(SOURCES
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/stereoprocessor.h
    src/stereoprocessor.cpp
    src/videoworker.h
    src/videoworker.cpp
    src/spherical.h
    src/spherical.cpp
)

add_executable(StereoMoviePro WIN32
    ${SOURCES}
)

target_link_libraries(StereoMoviePro PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

add_executable(TestStereo
    tests/test_stereo.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/stereoprocessor.h
    src/stereoprocessor.cpp
    src/videoworker.h
    src/videoworker.cpp
    src/spherical.h
    src/spherical.cpp
)

target_link_libraries(TestStereo PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

