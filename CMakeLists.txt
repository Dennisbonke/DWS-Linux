cmake_minimum_required(VERSION 3.10)
project(DWS_Tray C)

set(CMAKE_C_STANDARD 11)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(AYATANA REQUIRED ayatana-appindicator3-0.1)
find_package(CURL REQUIRED)

# Include and link directories
include_directories(${GTK3_INCLUDE_DIRS} ${AYATANA_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS} ${AYATANA_LIBRARY_DIRS})

# Add the executable
add_executable(DWS_Tray main.c)

# Link all required libraries
target_link_libraries(DWS_Tray
    ${GTK3_LIBRARIES}
    ${AYATANA_LIBRARIES}
    ${CURL_LIBRARIES}
)

# Define compile flags (e.g., for warnings)
add_definitions(${GTK3_CFLAGS_OTHER} ${AYATANA_CFLAGS_OTHER})

# Set install locations
include(GNUInstallDirs)

# Install binary
install(TARGETS DWS_Tray DESTINATION ${CMAKE_INSTALL_BINDIR})

# Install icons
install(FILES ${CMAKE_SOURCE_DIR}/images/defcon1.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/32x32/apps)
install(FILES ${CMAKE_SOURCE_DIR}/images/defcon2.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/32x32/apps)
install(FILES ${CMAKE_SOURCE_DIR}/images/defcon3.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/32x32/apps)
install(FILES ${CMAKE_SOURCE_DIR}/images/defcon4.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/32x32/apps)
install(FILES ${CMAKE_SOURCE_DIR}/images/defcon5.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/32x32/apps)
install(FILES ${CMAKE_SOURCE_DIR}/images/defcon-no-connection.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/32x32/apps)

# Post-install hook to update icon cache
install(CODE "
  execute_process(COMMAND gtk-update-icon-cache -f -t ${CMAKE_INSTALL_FULL_DATAROOTDIR}/icons/hicolor
                  RESULT_VARIABLE result
                  OUTPUT_VARIABLE output
                  ERROR_VARIABLE error
                  OUTPUT_STRIP_TRAILING_WHITESPACE
                  ERROR_STRIP_TRAILING_WHITESPACE)
  if(NOT result EQUAL 0)
    message(WARNING \"Failed to update icon cache: \${error}\")
  endif()
")

# Configure .desktop file
set(EXECUTABLE_PATH "${CMAKE_INSTALL_FULL_BINDIR}/DWS_Tray")
configure_file(${CMAKE_SOURCE_DIR}/dws_tray.desktop.in
               ${CMAKE_BINARY_DIR}/dws_tray.desktop @ONLY)

# Install .desktop file
install(FILES ${CMAKE_BINARY_DIR}/dws_tray.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)

# Configure uninstall script
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake_uninstall.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
    IMMEDIATE @ONLY
)

# Add custom uninstall target
add_custom_target(uninstall
  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
)
