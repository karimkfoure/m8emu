# A GCC ARM64 build isolated from the installed compiler's newer libc.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
get_filename_component(m8_public_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(M8_ARM64_TOOLCHAIN_CACHE "${m8_public_root}/build/toolchains/handheld" CACHE PATH "Pinned ARM64 sysroot cache")
find_program(m8_host_python NAMES python3 REQUIRED)
if(DEFINED ENV{M8_ARM64_SYSROOT} AND NOT "$ENV{M8_ARM64_SYSROOT}" STREQUAL "")
    set(m8_sysroot "$ENV{M8_ARM64_SYSROOT}")
else()
    execute_process(COMMAND "${m8_host_python}" "${CMAKE_CURRENT_LIST_DIR}/prepare-sysroot.py"
        --output "${M8_ARM64_TOOLCHAIN_CACHE}/sysroot"
        RESULT_VARIABLE m8_setup_status OUTPUT_QUIET ERROR_VARIABLE m8_setup_error
        TIMEOUT 900)
    if(NOT m8_setup_status EQUAL 0)
        message(FATAL_ERROR "Pinned ARM64 sysroot preparation failed: ${m8_setup_error}")
    endif()
    set(m8_sysroot "${M8_ARM64_TOOLCHAIN_CACHE}/sysroot/root")
endif()
if(DEFINED ENV{M8_ARM64_CC} AND NOT "$ENV{M8_ARM64_CC}" STREQUAL "")
    set(m8_cross_gcc "$ENV{M8_ARM64_CC}")
else()
    find_program(m8_cross_gcc NAMES aarch64-linux-gnu-gcc REQUIRED)
endif()
execute_process(COMMAND "${m8_host_python}" "${CMAKE_CURRENT_LIST_DIR}/prepare-compiler.py"
    --compiler "${m8_cross_gcc}" --sysroot "${m8_sysroot}"
    --output "${M8_ARM64_TOOLCHAIN_CACHE}/compiler"
    RESULT_VARIABLE m8_setup_status OUTPUT_QUIET ERROR_VARIABLE m8_setup_error TIMEOUT 120)
if(NOT m8_setup_status EQUAL 0)
    message(FATAL_ERROR "Isolated ARM64 compiler preparation failed: ${m8_setup_error}")
endif()
set(CMAKE_C_COMPILER "${M8_ARM64_TOOLCHAIN_CACHE}/compiler/aarch64-linux-gnu-gcc"
    CACHE FILEPATH "Pinned ARM64 GCC wrapper" FORCE)
set(CMAKE_SYSROOT "${m8_sysroot}" CACHE PATH "Pinned old-glibc ARM64 sysroot" FORCE)
set(CMAKE_FIND_ROOT_PATH "${m8_sysroot}")
# pkg-config is a host tool, but its packages must describe the target ABI.
set(ENV{PKG_CONFIG_LIBDIR} "${m8_sysroot}/usr/lib/aarch64-linux-gnu/pkgconfig:${m8_sysroot}/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${m8_sysroot}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
if(DEFINED ENV{M8_QEMU_AARCH64} AND NOT "$ENV{M8_QEMU_AARCH64}" STREQUAL "")
    set(CMAKE_CROSSCOMPILING_EMULATOR "$ENV{M8_QEMU_AARCH64};-L;${m8_sysroot}"
        CACHE STRING "QEMU with the build's old loader" FORCE)
elseif(NOT DEFINED CMAKE_CROSSCOMPILING_EMULATOR OR CMAKE_CROSSCOMPILING_EMULATOR STREQUAL "")
    find_program(m8_qemu NAMES qemu-aarch64 qemu-aarch64-static REQUIRED)
    set(CMAKE_CROSSCOMPILING_EMULATOR "${m8_qemu};-L;${m8_sysroot}"
        CACHE STRING "QEMU with the build's old loader" FORCE)
endif()
set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES M8_ARM64_TOOLCHAIN_CACHE)
