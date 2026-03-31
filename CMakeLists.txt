cmake_minimum_required(VERSION 3.16)
project(QuiverCpp VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Charts Network)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_executable(QuiverCpp
    main.cpp
    src/MainWindow.cpp
    src/AuthManager.cpp
    src/AuthWindow.cpp
    src/DashboardPage.cpp
    src/Components.cpp
    src/Backend.cpp
    src/TablePages.cpp
    src/TopNavBar.cpp
    include/MainWindow.h
    include/DashboardPage.h
    include/AuthWindow.h
    include/AuthManager.h
    include/Components.h
    include/Backend.h
    include/FlowLayout.h
    include/common_header.hpp
    include/TablePages.h
    include/TopNavBar.h
    src/FlowLayout.cpp
    resources.qrc
    assets/style.qss
    assets/light_style.qss
    assets/icons
)
if(WIN32)
    target_sources(QuiverCpp PRIVATE quiver.rc)
endif()
target_link_libraries(QuiverCpp PRIVATE Qt6::Widgets Qt6::Gui Qt6::Core Qt6::Charts Qt6::Network)
target_include_directories(QuiverCpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
