# Linux ARM64 host binaries. Unicorn still executes ARM32 firmware.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

if(DEFINED ENV{M8_ARM64_SYSROOT} AND NOT "$ENV{M8_ARM64_SYSROOT}" STREQUAL "")
    set(CMAKE_SYSROOT "$ENV{M8_ARM64_SYSROOT}" CACHE PATH "ARM64 system root" FORCE)
    list(PREPEND CMAKE_FIND_ROOT_PATH "$ENV{M8_ARM64_SYSROOT}")
endif()

if(DEFINED ENV{M8_ARM64_CC} AND NOT "$ENV{M8_ARM64_CC}" STREQUAL "")
    set(CMAKE_C_COMPILER "$ENV{M8_ARM64_CC}")
else()
    find_program(M8_ARM64_GCC aarch64-linux-gnu-gcc)
    if(M8_ARM64_GCC)
        set(CMAKE_C_COMPILER "${M8_ARM64_GCC}")
    else()
        get_filename_component(M8_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
        set(CMAKE_C_COMPILER "${M8_SOURCE_ROOT}/build/toolchains/bin/aarch64-linux-gnu-cc")
    endif()
endif()
if(CMAKE_C_COMPILER MATCHES "/aarch64-linux-gnu-cc$")
    # Zig's linker does not accept GNU ld's -rpath-link option. Apply the
    # override after CMake loads its Linux compiler defaults.
    set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CMAKE_CURRENT_LIST_DIR}/zig-linux-flags.cmake")
endif()
if(DEFINED ENV{M8_ARM64_AR} AND NOT "$ENV{M8_ARM64_AR}" STREQUAL "")
    set(CMAKE_AR "$ENV{M8_ARM64_AR}")
endif()
if(DEFINED ENV{M8_ARM64_RANLIB} AND NOT "$ENV{M8_ARM64_RANLIB}" STREQUAL "")
    set(CMAKE_RANLIB "$ENV{M8_ARM64_RANLIB}")
endif()

# Do not select host libraries during a cross build. Compiler-provided libc
# remains available without imposing the QEMU runtime prefix as a build sysroot.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

if(DEFINED ENV{M8_QEMU_AARCH64} AND NOT "$ENV{M8_QEMU_AARCH64}" STREQUAL "")
    set(CMAKE_CROSSCOMPILING_EMULATOR "$ENV{M8_QEMU_AARCH64};-L;$ENV{M8_ARM64_RUNTIME}"
        CACHE STRING "ARM64 emulator and libc prefix" FORCE)
endif()
