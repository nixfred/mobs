cmake_minimum_required(VERSION 3.28)
project(mobs-spike LANGUAGES CXX)

# Throwaway spike: can a QML editor (Monologue's real EditBar.qml) live inside an OBS dock?
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets Qml Quick QuickWidgets)

add_library(mobs-spike MODULE spike.cpp)
set_target_properties(mobs-spike PROPERTIES PREFIX "")
target_link_libraries(mobs-spike PRIVATE OBS::libobs OBS::obs-frontend-api
  Qt6::Widgets Qt6::Qml Qt6::Quick Qt6::QuickWidgets)
