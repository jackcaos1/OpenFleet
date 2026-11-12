# Plattformunabhaengiger Kern (C++17). Keine Plattform-#ifdefs im Code, nur hier in CMake.

add_library(fleet_core STATIC
  src/core/Log.cpp
  src/core/Math.cpp
  src/core/TextLayout.cpp
  src/io/DirSource.cpp
  src/io/Inflate.cpp
  src/io/ZipSource.cpp
  src/formats/Tga.cpp
  src/formats/Bmp.cpp
  src/formats/LabelMap.cpp
  src/formats/Zfs.cpp
  src/formats/Odf.cpp
  src/formats/Spr.cpp
  src/formats/Tt.cpp
  src/formats/Bzn.cpp
  src/formats/SaveFile.cpp
  src/formats/SaveSchema.cpp
  src/formats/SaveGame.cpp
  src/formats/Wav.cpp
  src/audio/Mixer.cpp
  src/game/MapScene.cpp
  src/game/Ui.cpp
  src/game/Hud.cpp
  src/game/ShipSim.cpp
  src/game/HoverSim.cpp
    src/game/HoverShip.cpp
    src/game/PathGrid.cpp
  src/game/Combat.cpp
  src/game/Shockwave.cpp
  src/game/Boarding.cpp
  src/game/Skirmish.cpp
  src/game/Shell.cpp
  src/game/MenuLayout.cpp
  src/formats/GuiCfg.cpp
  src/data/UnitDef.cpp
  src/game/FireDemo.cpp
  src/formats/Sod.cpp
  src/formats/TextureLookup.cpp
  src/model/ModelBuilder.cpp)
target_include_directories(fleet_core PUBLIC src)
target_compile_features(fleet_core PUBLIC cxx_std_17)
if(MSVC)
  target_compile_options(fleet_core PRIVATE /W4)
else()
  target_compile_options(fleet_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(FLEET_BUILD_GAME)
  add_library(fleet_game STATIC
    src/game/Game.cpp
    src/render/TriangleRenderer.cpp
    src/render/ModelRenderer.cpp
    src/render/SpriteRenderer.cpp
    src/render/TextRenderer.cpp
    src/render/HudRenderer.cpp
    src/render/MarkerRenderer.cpp
    src/render/EffectRenderer.cpp)
  target_link_libraries(fleet_game PUBLIC fleet_core)
  if(MSVC)
    target_compile_options(fleet_game PRIVATE /W4)
  else()
    target_compile_options(fleet_game PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  if(ANDROID)
    target_link_libraries(fleet_game PUBLIC GLESv3 log)
  else()
    find_path(GLES3_INCLUDE_DIR GLES3/gl3.h)
    find_library(GLES_LIBRARY NAMES GLESv2)
    if(NOT GLES3_INCLUDE_DIR OR NOT GLES_LIBRARY)
      message(FATAL_ERROR
        "OpenGL ES 3 nicht gefunden. Unter Debian/Ubuntu: sudo apt install libgles-dev (oder libgles2-mesa-dev). "
        "Zum Bauen ohne Rendering: -DFLEET_BUILD_GAME=OFF -DFLEET_BUILD_DESKTOP=OFF")
    endif()
    target_include_directories(fleet_game PUBLIC ${GLES3_INCLUDE_DIR})
    target_link_libraries(fleet_game PUBLIC ${GLES_LIBRARY})
  endif()
endif()
