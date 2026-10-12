cmake_minimum_required(VERSION 3.28)
project(mobs-app VERSION 0.1.0 LANGUAGES CXX)

# MOBS app: its own Omarchy-styled window (ADR 0005). Monologue's theme watcher is reused BY PATH
# (never copied) so subtree pulls keep updating it. GPLv2 like the rest of MOBS (OBS is GPLv2).
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
enable_testing()

find_package(Qt6 REQUIRED COMPONENTS Gui Qml Quick DBus Network Multimedia Test WebEngineQuick)   # WebEngineQuick (qt6-webengine) is kept for a later feature; only the mobs program links it
find_package(libobs REQUIRED)

# QTP0001: embed the QML module under :/qt/qml/<URI>, where Qt's default import path looks. Without it the module's
# qmldir says `prefer :/Mobs/` and an INSTALLED binary cannot find its own QML (it only worked next to the build folder).
qt_policy(SET QTP0001 NEW)
set(MONOLOGUE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../monologue/src)

add_library(mobs-core STATIC src/recorderbackend.h src/mobslog.cpp src/notifier.cpp src/pipcontroller.cpp src/chatmodel.cpp src/chatrelay.cpp src/palette.cpp src/appstate.cpp src/camera.cpp src/audiodevices.cpp src/settings.cpp src/settings_setup.cpp src/appstate_setup.cpp src/previewmodel.cpp src/review.cpp src/engine.cpp src/control.cpp src/obsbackend.cpp ${MONOLOGUE_SRC}/theme.cpp ${MONOLOGUE_SRC}/edit.cpp ${MONOLOGUE_SRC}/mediautils.cpp ${MONOLOGUE_SRC}/thumbnails.cpp)
target_include_directories(mobs-core PUBLIC src ${MONOLOGUE_SRC})
target_link_libraries(mobs-core PUBLIC Qt6::Gui Qt6::Qml Qt6::Quick Qt6::DBus Qt6::Network Qt6::Multimedia OBS::libobs)

qt_add_executable(mobs src/main.cpp)
# Monologue's timeline editor, used UNMODIFIED from its subtree: mapped into this QML module under its own file name.
set_source_files_properties(${MONOLOGUE_SRC}/EditBar.qml PROPERTIES QT_RESOURCE_ALIAS qml/EditBar.qml QT_QML_SOURCE_TYPENAME EditBar)
qt_add_qml_module(mobs
  URI Mobs
  VERSION 1.0
  QML_FILES
    qml/Main.qml qml/Txt.qml qml/Glyph.qml qml/Key.qml qml/Chip.qml qml/Card.qml
    qml/Header.qml qml/Footer.qml qml/ChooseScreen.qml qml/ReadyScreen.qml qml/RecordingScreen.qml qml/CameraPanel.qml qml/PreviewView.qml qml/ReviewScreen.qml qml/Button.qml qml/ProDrawer.qml qml/Toggle.qml qml/AudioPage.qml qml/LevelMeter.qml qml/DevicePicker.qml ${MONOLOGUE_SRC}/EditBar.qml
)
qt_target_qml_sources(mobs QML_FILES qml/SplashScreen.qml qml/PipWindow.qml qml/ChatPanel.qml)   # the opening screen, the webcam window and its chat card
target_link_libraries(mobs PRIVATE mobs-core Qt6::WebEngineQuick)
# The guided setup ("easy mode"): its pages, and the REAL pictures of X's screens it points at (cropped from real screenshots, never drawn).
qt_target_qml_sources(mobs QML_FILES qml/BigButton.qml qml/GuideImage.qml qml/GoLiveGuide.qml qml/XChatView.qml qml/SetupChat.qml qml/SetupGuide.qml qml/SetupWelcome.qml qml/SetupCamera.qml qml/SetupMic.qml qml/SetupScreen.qml qml/SetupWebcam.qml qml/SetupStudio.qml qml/SetupSource.qml qml/SetupKey.qml qml/SetupHow.qml qml/SetupDone.qml
    RESOURCES assets/guide/x-live-studio.png assets/guide/x-manage-sources.png assets/guide/x-source-card.png assets/guide/x-create-livestream.png assets/guide/x-livestream-live.png)

foreach(t palette appstate engine control camera preview review settings audiodevices obsbackend mobslog pipcontroller streamhealth chatmodel chatrelay)
  add_executable(tst_${t} tests/tst_${t}.cpp)
  target_link_libraries(tst_${t} PRIVATE mobs-core Qt6::Test)
  # QTest pulls all of QtCore, which trips GCC 16 over Qt's own QBitArray (same flag Monologue uses).
  target_compile_options(tst_${t} PRIVATE -Wno-sfinae-incomplete)
  add_test(NAME ${t} COMMAND tst_${t})
  if(NOT t STREQUAL obsbackend)   # the libobs test needs a real Wayland session, and skips itself without one
    set_tests_properties(${t} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME=generic;QT_QUICK_BACKEND=software")
  endif()
endforeach()

# ---- install: `cmake --install build --prefix ~/.local` puts MOBS in the launcher, with its icon, and makes the hotkey able
# to start it (D-Bus launch file). Nothing here touches Hyprland: the key bind is a separate, explicit step (README).
install(TARGETS mobs RUNTIME DESTINATION bin)
install(FILES dist/mobs.svg DESTINATION share/icons/hicolor/scalable/apps)
install(CODE "set(MOBS_SRC \"${CMAKE_CURRENT_SOURCE_DIR}\")")
install(SCRIPT dist/install-templates.cmake)

add_test(NAME install_paths COMMAND ${CMAKE_COMMAND} -DBUILD_DIR=${CMAKE_BINARY_DIR} -DPREFIX=${CMAKE_BINARY_DIR}/install-test
         -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_install.cmake)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME ui COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_ui.py $<TARGET_FILE:mobs>)
set_tests_properties(ui PROPERTIES TIMEOUT 900)

# The NVIDIA plugin locates its capability helper beside the host executable, not through PATH.
find_program(MOBS_NVENC_TEST obs-nvenc-test)
if(MOBS_NVENC_TEST)
  configure_file(${MOBS_NVENC_TEST} ${CMAKE_CURRENT_BINARY_DIR}/obs-nvenc-test COPYONLY)
  install(PROGRAMS ${MOBS_NVENC_TEST} DESTINATION bin)
endif()
