cmake_minimum_required(VERSION 3.22)

project(OPENTYRIAN)

# SDL2 (official android build via submodule CMakeLists)
add_subdirectory(SDL)

# SDL2_net built inline — the stock CMakeLists is heavy (install/pc/test
# binaries) and depends on SDL2::SDL2 alias targets that vary between SDL
# versions. Compiling the four C files against the SDL2 target is simpler
# and reliable.
add_library(SDL2_net SHARED
    SDL_net/SDLnet.c
    SDL_net/SDLnetTCP.c
    SDL_net/SDLnetUDP.c
    SDL_net/SDLnetselect.c
)
target_include_directories(SDL2_net PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/SDL_net)
target_include_directories(SDL2_net PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/SDL/include)
target_link_libraries(SDL2_net PRIVATE SDL2)

add_subdirectory(src)
