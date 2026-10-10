cmake_minimum_required(VERSION 3.28)
project(mobs-spike4 LANGUAGES CXX)

# Spike 4: MOBS as a NEW PRODUCT. A Qt program that embeds libobs in its own process (no OBS app,
# no websocket, nothing hidden) and records. This is how OBS Studio itself is built: frontend + libobs.
set(CMAKE_CXX_STANDARD 17)
find_package(libobs REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Gui)
add_executable(libobs-record libobs-record.cpp)
target_link_libraries(libobs-record PRIVATE OBS::libobs Qt6::Gui)
