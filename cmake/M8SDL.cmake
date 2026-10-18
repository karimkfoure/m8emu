include_guard(GLOBAL)

function(m8emu_prepare_sdl)
    if(TARGET SDL3::SDL3)
        return()
    endif()
    option(M8_FETCH_SDL "Build pinned SDL3 when no installed package is available" ON)
    if(NOT M8_FETCH_SDL)
        message(FATAL_ERROR "SDL3 is required: set SDL3_DIR or enable M8_FETCH_SDL")
    endif()
    include(FetchContent)
    set(SDL_SHARED ON CACHE BOOL "Shared SDL3" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "Static SDL3" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "SDL tests" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "SDL test library" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "SDL examples" FORCE)
    if(CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
       CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
        # Handhelds use KMSDRM and ALSA without desktop window system headers.
        set(SDL_UNIX_CONSOLE_BUILD ON CACHE BOOL "SDL handheld console build" FORCE)
    endif()
    FetchContent_Declare(SDL3
        URL https://github.com/libsdl-org/SDL/archive/96292a5b464258a2b926e0a3d72f8b98c2a81aa6.tar.gz
        URL_HASH SHA256=4ae5eba49e7e346f742bea4c9966f4cc871face4b999513fcc1eb1f14d4dec30)
    FetchContent_MakeAvailable(SDL3)
endfunction()
