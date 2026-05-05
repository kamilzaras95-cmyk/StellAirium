INCLUDE_DIRECTORIES(.)
LINK_DIRECTORIES(${CMAKE_BINARY_DIR}/src)

SET(AuroraAircraft_SRCS
     AuroraAircraft.hpp
     AuroraAircraft.cpp
     AircraftObject.hpp
     AircraftObject.cpp
     AuroraAircraftDialog.hpp
     AuroraAircraftDialog.cpp
)

# ── STATIC ── wkompilowany w binarke Stellarium (Aviarium.app) ────────────────
ADD_LIBRARY(AuroraAircraft-static STATIC ${AuroraAircraft_SRCS})

TARGET_LINK_LIBRARIES(AuroraAircraft-static
     Qt${QT_VERSION_MAJOR}::Core
     Qt${QT_VERSION_MAJOR}::Gui
     Qt${QT_VERSION_MAJOR}::Network
     Qt${QT_VERSION_MAJOR}::Widgets
)

SET_TARGET_PROPERTIES(AuroraAircraft-static PROPERTIES
     OUTPUT_NAME  "AuroraAircraft"
     COMPILE_FLAGS "-DQT_STATICPLUGIN"
     FOLDER       "plugins/AuroraAircraft"
)

IF(TARGET AllStaticPlugins)
     ADD_DEPENDENCIES(AllStaticPlugins AuroraAircraft-static)
ENDIF()

# ── MODULE ── side-loadowany plugin (.dylib / .so / .dll) ────────────────────
ADD_LIBRARY(StellAirium MODULE ${AuroraAircraft_SRCS})

TARGET_INCLUDE_DIRECTORIES(StellAirium PRIVATE .)

TARGET_LINK_LIBRARIES(StellAirium
     Qt${QT_VERSION_MAJOR}::Core
     Qt${QT_VERSION_MAJOR}::Gui
     Qt${QT_VERSION_MAJOR}::Network
     Qt${QT_VERSION_MAJOR}::Widgets
)

SET_TARGET_PROPERTIES(StellAirium PROPERTIES
     FOLDER "plugins/StellAirium"
     LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules/StellAirium"
     RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules/StellAirium"
)
# Stellarium oczekuje lib<Name>.dylib / lib<Name>.so / lib<Name>.dll.
# macOS/Linux mają lib prefix domyślnie; Windows wymaga jawnego ustawienia.
IF(WIN32)
     SET_TARGET_PROPERTIES(StellAirium PROPERTIES PREFIX "lib")
ENDIF()

IF(APPLE)
     # Symbole Stellarium (StelApp, StelCore…) rozwiązywane z binarki w runtime.
     SET_TARGET_PROPERTIES(StellAirium PROPERTIES
          SUFFIX     ".dylib"
          LINK_FLAGS "-undefined dynamic_lookup"
     )
ELSEIF(WIN32)
     # Na Windows dynamiczny plugin jest linkowany z targetem stelMain,
     # tak jak przykładowe pluginy Stellarium. Kluczowe jest, żeby hostowy
     # build nie miał zbędnych zależności typu Qt WebEngine.
     IF(TARGET stelMain)
          TARGET_LINK_LIBRARIES(StellAirium stelMain)
     ENDIF()
ENDIF()
