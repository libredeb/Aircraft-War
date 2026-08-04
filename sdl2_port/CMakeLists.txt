cmake_minimum_required(VERSION 3.16)
project(AircraftWar VERSION 2.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2       REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(SDL2_IMAGE REQUIRED IMPORTED_TARGET SDL2_image)
pkg_check_modules(SDL2_MIXER REQUIRED IMPORTED_TARGET SDL2_mixer)
pkg_check_modules(SDL2_TTF   REQUIRED IMPORTED_TARGET SDL2_ttf)

add_executable(aircraftwar
    src/main.cpp
    src/game.cpp
    src/entity.cpp
    src/resources.cpp
)

target_include_directories(aircraftwar PRIVATE src)

target_compile_options(aircraftwar PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(aircraftwar
    PkgConfig::SDL2
    PkgConfig::SDL2_IMAGE
    PkgConfig::SDL2_MIXER
    PkgConfig::SDL2_TTF
)

set(DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/aircraftwar")
target_compile_definitions(aircraftwar PRIVATE
    DATA_DIR="${DATA_DIR}"
)

install(TARGETS aircraftwar DESTINATION bin)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/../Image/" DESTINATION "${DATA_DIR}/Image")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/../sound/" DESTINATION "${DATA_DIR}/sound")
install(FILES "${CMAKE_SOURCE_DIR}/../fzmw.ttf" DESTINATION "${DATA_DIR}")
install(FILES "${CMAKE_SOURCE_DIR}/data/gamecontrollerdb.txt" DESTINATION "${DATA_DIR}")

# Copy controller DB next to the binary for local runs from the build tree
add_custom_command(TARGET aircraftwar POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:aircraftwar>/data"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/data/gamecontrollerdb.txt"
        "$<TARGET_FILE_DIR:aircraftwar>/data/gamecontrollerdb.txt"
)
