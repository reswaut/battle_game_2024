set(lib_name battle_game_${COMPONENT_NAME}_lib)
list(APPEND LIBRARY_LIST ${lib_name})

add_library(${lib_name})
file(GLOB source_files *.cpp *.h)
target_sources(${lib_name} PUBLIC ${source_files})
get_target_property(JSON_INC_PATH jsoncpp_lib INTERFACE_INCLUDE_DIRECTORIES)
include_directories(${JSON_INC_PATH})
target_link_libraries(${lib_name} PUBLIC jsoncpp_lib)
target_include_directories(${lib_name} PUBLIC ${BATTLE_GAME_EXTERNAL_INCLUDE_DIRS} ${BATTLE_GAME_INCLUDE_DIR})

set(LIBRARY_LIST ${LIBRARY_LIST} PARENT_SCOPE)

target_compile_definitions(${lib_name} PUBLIC BATTLE_GAME_ASSETS_DIR="${BATTLE_GAME_ASSETS_DIR}/")

set(BATTLE_GAME_LOCALIZATION_DIR ${CMAKE_SOURCE_DIR}/localization)
add_custom_command(TARGET ${lib_name} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                   ${BATTLE_GAME_LOCALIZATION_DIR} $<TARGET_FILE_DIR:${lib_name}>)
