cmake_minimum_required(VERSION 3.28)
project(mobs-spike3 LANGUAGES CXX)

# Spike 3: can a plain Qt client drive a hidden OBS over obs-websocket v5?
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Network WebSockets)
add_executable(obsctl obsctl.cpp)
target_link_libraries(obsctl PRIVATE Qt6::Core Qt6::Network Qt6::WebSockets)
