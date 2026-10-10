cmake_minimum_required(VERSION 3.28)
project(mobs-app VERSION 0.1.0 LANGUAGES CXX)

# MOBS app: its own Omarchy-styled window (ADR 0005). Monologue's theme watcher is reused BY PATH
# (never copied) so subtree pulls keep updating it. GPLv2 like the rest of MOBS (OBS is GPLv2).
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
enable_testing()

find_package(Qt6 REQUIRED COMPONENTS Gui Qml Quick Test)
find_package(libobs REQUIRED)
set(MONOLOGUE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../monologue/src)

add_library(mobs-core STATIC src/recorderbackend.h src/palette.cpp src/appstate.cpp src/engine.cpp src/obsbackend.cpp ${MONOLOGUE_SRC}/theme.cpp)
target_include_directories(mobs-core PUBLIC src ${MONOLOGUE_SRC})
target_link_libraries(mobs-core PUBLIC Qt6::Gui Qt6::Qml Qt6::Quick OBS::libobs)

qt_add_executable(mobs src/main.cpp)
qt_add_qml_module(mobs
  URI Mobs
  VERSION 1.0
  QML_FILES
    qml/Main.qml qml/Txt.qml qml/Glyph.qml qml/Key.qml qml/Chip.qml qml/Card.qml
    qml/Header.qml qml/Footer.qml qml/ChooseScreen.qml qml/ReadyScreen.qml qml/RecordingScreen.qml
)
target_link_libraries(mobs PRIVATE mobs-core)

foreach(t palette appstate engine obsbackend)
  add_executable(tst_${t} tests/tst_${t}.cpp)
  target_link_libraries(tst_${t} PRIVATE mobs-core Qt6::Test)
  # QTest pulls all of QtCore, which trips GCC 16 over Qt's own QBitArray (same flag Monologue uses).
  target_compile_options(tst_${t} PRIVATE -Wno-sfinae-incomplete)
  add_test(NAME ${t} COMMAND tst_${t})
  if(NOT t STREQUAL obsbackend)   # the libobs test needs a real Wayland session, and skips itself without one
    set_tests_properties(${t} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  endif()
endforeach()
