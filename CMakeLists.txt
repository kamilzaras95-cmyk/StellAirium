# Plugin AuroraAircraft — root CMakeLists, ładowany przez plugins/CMakeLists.txt
# Stellarium gdy ma USE_PLUGIN_AURORAAIRCRAFT=1 i AuroraAircraft jest na liście
# STELLARIUM_PLUGINS w głównym CMakeLists.txt drzewa źródeł.

SET(AURORAAIRCRAFT_VERSION "0.0.11")

ADD_DEFINITIONS(-DAURORAAIRCRAFT_PLUGIN_VERSION="${AURORAAIRCRAFT_VERSION}")
ADD_DEFINITIONS(-DAURORAAIRCRAFT_PLUGIN_LICENSE="GNU GPLv2 or later")

ADD_SUBDIRECTORY(src)

# Na macOS i Linuxie instalujemy do user data dir (side-load), nie do systemowego.
IF(APPLE)
     SET(CMAKE_INSTALL_PREFIX $ENV{HOME}/Library/Application\ Support/Stellarium)
ELSE(APPLE)
     SET(CMAKE_INSTALL_PREFIX $ENV{HOME}/.stellarium)
ENDIF(APPLE)
