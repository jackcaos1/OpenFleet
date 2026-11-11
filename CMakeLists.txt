cmake_minimum_required(VERSION 3.22)
project(fleetclient VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Arbeitsname "fleetclient" ist ein Platzhalter (kein Markenname), endgueltiger Name steht noch aus.
option(FLEET_BUILD_GAME    "Render-/Spielkern bauen (braucht OpenGL ES 3)" ON)
option(FLEET_BUILD_DESKTOP "Desktop-Launcher bauen (braucht SDL2)"         ON)
option(FLEET_BUILD_TESTS   "Unit-Tests bauen"                              ON)

add_subdirectory(engine)

if(FLEET_BUILD_DESKTOP AND FLEET_BUILD_GAME)
  add_subdirectory(desktop)
endif()

if(FLEET_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
