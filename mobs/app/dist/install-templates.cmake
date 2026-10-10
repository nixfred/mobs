# Runs at INSTALL time (not configure time) so the paths in the launcher entry and the D-Bus launch file are the
# prefix actually installed to, e.g. ~/.local, and DESTDIR is honoured.
set(MOBS_BIN "${CMAKE_INSTALL_PREFIX}/bin/mobs")
set(dest "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}")
foreach(pair "mobs.desktop.in|share/applications/mobs.desktop"
             "org.mobs.Control.service.in|share/dbus-1/services/org.mobs.Control.service")
  string(REPLACE "|" ";" parts "${pair}")
  list(GET parts 0 in)
  list(GET parts 1 out)
  file(READ "${MOBS_SRC}/dist/${in}" content)
  string(REPLACE "@MOBS_BIN@" "${MOBS_BIN}" content "${content}")
  get_filename_component(dir "${dest}/${out}" DIRECTORY)
  file(MAKE_DIRECTORY "${dir}")
  file(WRITE "${dest}/${out}" "${content}")
  message(STATUS "Installing: ${dest}/${out}")
endforeach()
