cmake_minimum_required(VERSION 3.28)
project(mobs-design LANGUAGES CXX)

# Prototype runner for the MOBS screen design: loads proto.qml with SAMPLE data only.
# Talks to nothing: no OBS, no camera, no network.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Gui Qml Quick)
add_executable(mobs-design runner.cpp)
target_link_libraries(mobs-design PRIVATE Qt6::Gui Qt6::Qml Qt6::Quick)
