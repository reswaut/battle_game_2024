cmake_minimum_required(VERSION 3.22)

project(battle_game)
set(CMAKE_CXX_STANDARD 17)

if (MSVC)
    add_compile_options(/utf-8)
endif()

set(BATTLE_GAME_ASSETS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets)

add_subdirectory(external/grassland)
find_package(absl CONFIG REQUIRED)
find_package(jsoncpp REQUIRED)

list(APPEND BATTLE_GAME_EXTERNAL_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/external/abseil-cpp)
list(APPEND BATTLE_GAME_EXTERNAL_INCLUDE_DIRS ${GRASSLAND_INCLUDE_DIRS})
list(APPEND BATTLE_GAME_EXTERNAL_INCLUDE_DIRS ${JSONCPP_INCLUDE_DIR})

add_subdirectory(src)

target_compile_definitions(battle_game PRIVATE BATTLE_GAME_ASSETS_DIR="${BATTLE_GAME_ASSETS_DIR}/")
