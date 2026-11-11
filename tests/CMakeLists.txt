add_executable(fleet_tests test_main.cpp)
target_link_libraries(fleet_tests PRIVATE fleet_core)
add_test(NAME fleet_tests COMMAND fleet_tests)

# Vergleichswerkzeug gegen die Python-Referenz (braucht die echten Spieldaten, laeuft nicht automatisch).
add_executable(fleet_data_dump data_dump.cpp)
target_link_libraries(fleet_data_dump PRIVATE fleet_core)

# Schichtentrennung: kein Plattformcode im Kern (engine/src).
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
  add_test(NAME layering COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/check_layering.py ${CMAKE_SOURCE_DIR})
endif()

# Layout-Ausgabe des Original-HUDs fuer die Sichtpruefung (braucht die echten Spieldaten).
add_executable(fleet_hud_dump hud_dump.cpp)
target_link_libraries(fleet_hud_dump PRIVATE fleet_core)
