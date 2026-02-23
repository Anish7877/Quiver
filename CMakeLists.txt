cmake_minimum_required(VERSION 3.16)
project(QuiverCpp VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6 components
find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Core)

# Auto-generate meta-object code (MOC) for signals/slots
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Define executable and source files
add_executable(QuiverCpp
    main.cpp
    src/MainWindow.cpp
    src/Components.cpp
    src/Backend.cpp
    include/MainWindow.h
    include/Components.h
    include/Backend.h
    resources.qrc
    assets/style.qss
    include/FlowLayout.h
    include/common_header.hpp
    src/FlowLayout.cpp
    assets/light_style.qss
    assets/icons/
)

# Link Qt libraries
target_link_libraries(QuiverCpp PRIVATE Qt6::Widgets Qt6::Gui Qt6::Core)
target_include_directories(QuiverCpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
