cmake_minimum_required(VERSION 3.22.1)
project(fleetclient_android LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Engine-Kern aus dem Repo-Wurzelverzeichnis (android/app/src/main/cpp -> ../../../../../engine)
set(FLEET_BUILD_GAME ON)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../../../../engine ${CMAKE_CURRENT_BINARY_DIR}/engine)

# android_native_app_glue stammt aus dem NDK (Quelle liegt dort als C-Datei)
add_library(native_app_glue STATIC
  ${CMAKE_ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
target_include_directories(native_app_glue PUBLIC
  ${CMAKE_ANDROID_NDK}/sources/android/native_app_glue)

add_library(fleetclient SHARED android_main.cpp SafSource.cpp)
target_link_libraries(fleetclient PRIVATE fleet_game native_app_glue android EGL log aaudio)
# Einstiegspunkt der NativeActivity darf der Linker nicht wegoptimieren
set_target_properties(fleetclient PROPERTIES LINK_FLAGS "-u ANativeActivity_onCreate")
