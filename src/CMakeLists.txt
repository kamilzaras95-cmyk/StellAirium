# Plugin AuroraAircraft — buduje STATIC, wkompilowywane w binarke Stellarium.
# Krok 1 MVP: potwierdzic ze plugin laduje sie i rysuje.
# Krok 2: dodac SHARED build target dla side-loadu (~/.stellarium/modules).

INCLUDE_DIRECTORIES(.)

LINK_DIRECTORIES(${CMAKE_BINARY_DIR}/src)

SET(AuroraAircraft_SRCS
     AuroraAircraft.hpp
     AuroraAircraft.cpp
     AircraftObject.hpp
     AircraftObject.cpp
)

ADD_LIBRARY(AuroraAircraft-static STATIC ${AuroraAircraft_SRCS})

TARGET_LINK_LIBRARIES(AuroraAircraft-static
     Qt${QT_VERSION_MAJOR}::Core
     Qt${QT_VERSION_MAJOR}::Gui
     Qt${QT_VERSION_MAJOR}::Network
)

SET_TARGET_PROPERTIES(AuroraAircraft-static PROPERTIES OUTPUT_NAME "AuroraAircraft")
SET_TARGET_PROPERTIES(AuroraAircraft-static PROPERTIES COMPILE_FLAGS "-DQT_STATICPLUGIN")
SET_TARGET_PROPERTIES(AuroraAircraft-static PROPERTIES FOLDER "plugins/AuroraAircraft")

ADD_DEPENDENCIES(AllStaticPlugins AuroraAircraft-static)
