cmake_minimum_required(VERSION 3.22)

project(opentyrian_main C)

set(OPENTYRIAN_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)
set(OPENTYRIAN_SRC ${OPENTYRIAN_ROOT}/src)

file(GLOB OPENTYRIAN_SOURCES "${OPENTYRIAN_SRC}/*.c")

add_library(main SHARED ${OPENTYRIAN_SOURCES})

target_include_directories(main PRIVATE
    ${OPENTYRIAN_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/../SDL/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../SDL_net
)

target_compile_definitions(main PRIVATE
    TARGET_UNIX
    HAVE_SDL_NET
    NDEBUG
)

target_compile_options(main PRIVATE
    -std=c11
    -Wall
    -Wno-missing-braces
    -Wno-parentheses
    -Wno-unused-parameter
    -Wno-unused-result
    -fvisibility=hidden
)

target_link_libraries(main
    SDL2
    SDL2_net
    log
    android
)
