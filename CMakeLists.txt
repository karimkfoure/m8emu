cmake_minimum_required(VERSION 3.20)
project(m8emu VERSION 0.1.0 LANGUAGES C)
include(cmake/M8QualifiedProfile.cmake)
include(cmake/M8RetiredOptions.cmake)
include(cmake/M8Firmware.cmake)
m8emu_prepare_firmware()
include(cmake/M8Python.cmake)
if(M8_BUILD_AOT OR M8_NATIVE_ENGINE OR M8_PGO STREQUAL "AUTO")
    m8emu_prepare_python()
endif()
include(cmake/M8BuildPGO.cmake)
m8_pgo_orchestrate(m8_outer_pgo)
if(m8_outer_pgo)
    return()
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# Mach-O resolves @loader_path where ELF resolves $ORIGIN.
if(APPLE)
    set(M8_RPATH_ORIGIN "@loader_path")
else()
    set(M8_RPATH_ORIGIN "$ORIGIN")
endif()
option(BUILD_SHARED_LIBS "Build shared runtime libraries" ON)
option(M8_BUILD_AOT "Build the original-firmware DSP AOT library" OFF)
option(M8_AOT_AUDIO_POOL "Use default-off exact integer audio-buffer pool kernels" OFF)
option(M8_NATIVE_PERFORMANCE_PROFILE "Use the default-off SAMPLER, HYPERSYNTH and audio-pool profile" OFF)
option(M8_NATIVE_PERFORMANCE_SOURCE_COPY "Build a checked combined-profile source copy for controls" OFF)
option(M8_AOT_CODEGEN_V2 "Use the default-off structural AOT source copy" OFF)
option(M8_BUILD_AOT_COUNTERS "Build a separate AOT instruction-counting library" ON)
option(M8_LOW_MEMORY_BUILD "Collect GCC compiler garbage more often for AOT" OFF)
option(M8_BUILD_BENCH "Build the headless benchmark when available" ON)
option(M8_GUEST_PC_PROFILE "Build playback guest-PC instruction attribution" OFF)
option(M8_UNICORN_TB_EXIT_FAST "Inline the empty ARM translation-block stop poll" ON)
option(M8RT_DIRECT_MEMORY "Use guarded owned-buffer memory copies by default" ON)
option(M8_RT_CALLBACK_TIMING "Collect host callback duration statistics" ON)
option(M8_ACCESS_PROFILE "Build playback slow-access attribution" OFF)
option(M8_NATIVE_ENGINE "Build the default-off native playback prototype" OFF)
set(M8_NATIVE_TLS_INITIAL_EXEC_DEFAULT OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SIZEOF_VOID_P EQUAL 8 AND
        CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64|aarch64|arm64|ARM64)$" AND
        CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(M8_NATIVE_TLS_INITIAL_EXEC_DEFAULT ON)
endif()
option(M8_NATIVE_TLS_INITIAL_EXEC "Use static TLS access in native playback libraries" ${M8_NATIVE_TLS_INITIAL_EXEC_DEFAULT})
set(M8_NATIVE_DEVICE_FAST_DEFAULT OFF)
if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin)$" AND
        CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$" AND
        M8_NATIVE_ENGINE AND M8_AOT_CODEGEN_V2)
    set(M8_NATIVE_DEVICE_FAST_DEFAULT ON)
endif()
option(M8_NATIVE_DEVICE_FAST "Use qualified native device callbacks" ${M8_NATIVE_DEVICE_FAST_DEFAULT})
set(M8_AOT_ARM64_FP_HARDWARE_DEFAULT OFF)
if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin)$" AND
        CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$" AND
        M8_NATIVE_ENGINE AND M8_AOT_CODEGEN_V2)
    set(M8_AOT_ARM64_FP_HARDWARE_DEFAULT ON)
endif()
option(M8_NATIVE_DEVICE_SITE_PROFILE "Count native device callback sites (needs M8_ACCESS_PROFILE)" OFF)
option(M8_NATIVE_BOOT "Build the default-off checked native cold-boot library" OFF)
option(M8_NATIVE_REGIONS "Compile the measured playback tail into native regions" OFF)
option(M8_NATIVE_PGO_SCOPED "Use the retained bounded native instrumentation scope" OFF)
option(M8_PORTABLE_BUNDLE "Use portable relative runtime paths" OFF)
option(M8_NATIVE_CORPUS_CATALOG "Use the default-off real-project native coverage catalog" OFF)
option(M8_NATIVE_AOT_CODE_GUARD "Guard code writes in a candidate AOT copy" OFF)
option(M8_NATIVE_REGION_INTEGER_FP_FASTPATH "Skip FP environment setup in integer-only native pages" OFF)
option(M8_NATIVE_REGION_SERVICE_CONTINUE "Continue native regions after qualified device callbacks" OFF)
option(M8_NATIVE_REGION_INLINE_MEMORY "Inline fixed-width native region memory accesses" OFF)
option(M8_NATIVE_REGION_CHAIN "Chain qualified native region exits directly" OFF)
option(M8_NATIVE_REGION_COMPUTED_REDIRECTS "Continue measured computed transfers inside native groups" OFF)
option(M8_NATIVE_REGION_INLINE_SERVICES "Compile exact barriers and RAM exclusives in native regions" OFF)
option(M8_NATIVE_REGION_RAM_EXCLUSIVES "Compile RAM word exclusives without inline barriers or prefetch" OFF)
option(M8_NATIVE_REGION_EVENT_LOOP "Run exact scheduling and IRQ boundaries in native region chains" OFF)
set(M8_NATIVE_REGION_SHARED_STATE_DEFAULT OFF)
if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin)$" AND
        CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$" AND
        M8_NATIVE_ENGINE AND M8_AOT_CODEGEN_V2)
    set(M8_NATIVE_REGION_SHARED_STATE_DEFAULT ON)
endif()
option(M8_NATIVE_REGION_SHARED_STATE "Keep region registers in the caller CPU" ${M8_NATIVE_REGION_SHARED_STATE_DEFAULT})
option(M8_RT_LOCAL_CALLS "Bind private ELF runtime helper calls locally" ON)
option(M8_COMBINED_RUNTIME "Link Unicorn and AOT into the shared runtime" OFF)
option(M8_COMBINED_LOCAL_BINDING "Bind calls within the combined runtime locally" ON)
set(M8_COMBINED_TEXT_ORDER_FILE "" CACHE FILEPATH "GNU ld script ordering combined runtime function sections")
include(cmake/M8Tuning.cmake)
include(cmake/M8UnicornCold.cmake)
include(CTest)
if((M8_NATIVE_BOOT OR M8_NATIVE_REGIONS OR M8_NATIVE_AOT_CODE_GUARD OR M8_NATIVE_CORPUS_CATALOG) AND NOT M8_NATIVE_ENGINE)
    message(FATAL_ERROR "Native boot, region and code-guard candidates require M8_NATIVE_ENGINE=ON")
endif()
if(M8_AOT_AUDIO_POOL AND NOT M8_BUILD_AOT)
    message(FATAL_ERROR "M8_AOT_AUDIO_POOL requires M8_BUILD_AOT=ON")
endif()
if(M8_NATIVE_PERFORMANCE_PROFILE OR (M8_AOT_AUDIO_POOL AND M8_AOT_CODEGEN_V2))
    # V2 admits the checked combined source format, not standalone pool input.
    # Compile flags still select only the requested component.
    set(M8_NATIVE_PERFORMANCE_SOURCE_COPY ON)
endif()
if(M8_NATIVE_PERFORMANCE_SOURCE_COPY AND
        (NOT M8_BUILD_AOT OR NOT M8_NATIVE_ENGINE OR NOT M8_NATIVE_AOT_CODE_GUARD))
    message(FATAL_ERROR "Native performance source copies require AOT, native engine and code guards")
endif()
if(M8_AOT_CODEGEN_V2 AND
        (NOT M8_BUILD_AOT OR NOT M8_NATIVE_ENGINE OR NOT M8_NATIVE_AOT_CODE_GUARD))
    message(FATAL_ERROR "M8_AOT_CODEGEN_V2 requires AOT, native engine and code guards")
endif()
if(M8_NATIVE_REGION_EVENT_LOOP AND NOT M8_NATIVE_REGION_CHAIN)
    message(FATAL_ERROR "M8_NATIVE_REGION_EVENT_LOOP requires M8_NATIVE_REGION_CHAIN=ON")
endif()

if(M8_COMBINED_RUNTIME)
    if(NOT BUILD_SHARED_LIBS OR NOT M8_BUILD_AOT)
        message(FATAL_ERROR "M8_COMBINED_RUNTIME requires BUILD_SHARED_LIBS=ON and M8_BUILD_AOT=ON")
    endif()
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "M8_COMBINED_RUNTIME requires an ELF Linux build with GCC or Clang")
    endif()
endif()
if(M8_COMBINED_TEXT_ORDER_FILE AND NOT M8_COMBINED_RUNTIME)
    message(FATAL_ERROR "M8_COMBINED_TEXT_ORDER_FILE requires M8_COMBINED_RUNTIME=ON")
endif()
if(M8_COMBINED_RUNTIME)
    set(m8_order_choices "")
    foreach(order_option M8_COMBINED_TEXT_ORDER_FILE M8_AOT_ORDER_FILE M8_RT_ORDER_FILE M8_UNICORN_ORDER_FILE)
        if(${order_option})
            list(APPEND m8_order_choices ${order_option})
        endif()
    endforeach()
    list(LENGTH m8_order_choices m8_order_count)
    if(m8_order_count GREATER 1)
        message(FATAL_ERROR "Combined runtime requires one order script: selected=${m8_order_choices}")
    endif()
endif()

set(M8_REFERENCE_ROOT "$ENV{M8_REFERENCE_ROOT}" CACHE PATH "Firmware and private SDL3 inputs")
if(NOT M8_REFERENCE_ROOT)
    set(M8_REFERENCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "Firmware and private SDL3 inputs" FORCE)
endif()
set(M8_SDL_PREFIX "$ENV{M8_LIVE_SDL_PREFIX}" CACHE PATH "Private SDL3 prefix")
if(NOT M8_SDL_PREFIX)
    foreach(sdl_candidate IN ITEMS
            "${CMAKE_CURRENT_SOURCE_DIR}/build/m8-live/sdl-prefix-requested"
            "${M8_REFERENCE_ROOT}/build/m8-live/sdl-prefix-requested"
            "${CMAKE_CURRENT_SOURCE_DIR}/build/m8-live/sdl-prefix"
            "${M8_REFERENCE_ROOT}/build/m8-live/sdl-prefix")
        if(EXISTS "${sdl_candidate}/lib/cmake/SDL3/SDL3Config.cmake")
            set(M8_SDL_PREFIX "${sdl_candidate}" CACHE PATH "Private SDL3 prefix" FORCE)
            break()
        endif()
    endforeach()
endif()
find_package(SDL3 QUIET CONFIG HINTS "${M8_SDL_PREFIX}/lib/cmake/SDL3")
option(M8_BUILD_DESKTOP "Build the SDL3 native desktop application" ${SDL3_FOUND})
if(M8_BUILD_DESKTOP AND NOT SDL3_FOUND)
    include(cmake/M8SDL.cmake)
    m8emu_prepare_sdl()
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/unicorn/CMakeLists.txt")
    message(FATAL_ERROR "Initialize the engine: git submodule update --init third_party/unicorn")
endif()
# Unicorn builds a universal macOS library unless ARCHFLAGS names one slice.
if(APPLE AND NOT DEFINED ENV{ARCHFLAGS})
    if(CMAKE_OSX_ARCHITECTURES MATCHES "^(arm64|x86_64)$")
        set(ENV{ARCHFLAGS} "-arch ${CMAKE_OSX_ARCHITECTURES}")
    elseif(NOT CMAKE_OSX_ARCHITECTURES AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(arm64|x86_64)$")
        set(ENV{ARCHFLAGS} "-arch ${CMAKE_HOST_SYSTEM_PROCESSOR}")
    endif()
endif()
# Match the corrected engine: ARM guest only, without tests or tracing.
set(UNICORN_ARCH arm CACHE STRING "Unicorn guest architectures" FORCE)
set(UNICORN_BUILD_TESTS OFF CACHE BOOL "Build Unicorn tests" FORCE)
set(UNICORN_INSTALL OFF CACHE BOOL "Install Unicorn" FORCE)
set(UNICORN_LEGACY_STATIC_ARCHIVE OFF CACHE BOOL "Bundle legacy archive" FORCE)
set(UNICORN_LOGGING OFF CACHE BOOL "Log Unicorn execution" FORCE)
set(UNICORN_TRACER OFF CACHE BOOL "Trace Unicorn execution" FORCE)
add_subdirectory(third_party/unicorn unicorn)
foreach(engine_target unicorn unicorn_static arm-softmmu)
    if(TARGET ${engine_target})
        target_compile_definitions(${engine_target} PRIVATE M8_UNICORN_DIRTY_RAM_STORE_ABI1=1)
        if(M8_UNICORN_TB_EXIT_FAST)
            target_compile_definitions(${engine_target} PRIVATE M8_UNICORN_TB_EXIT_FAST_ABI1=1)
        endif()
    endif()
endforeach()
foreach(engine_target unicorn unicorn_static unicorn-common arm-softmmu)
    if(TARGET ${engine_target})
        if((engine_target STREQUAL "unicorn_static" AND NOT M8_COMBINED_RUNTIME) OR
                (engine_target STREQUAL "unicorn" AND M8_COMBINED_RUNTIME))
            # Profile the API object used by the selected runtime. The other
            # API object has different profile names and is not executed.
            m8_tune_target(${engine_target} UNICORN NO_PGO)
        else()
            m8_tune_target(${engine_target} UNICORN)
        endif()
    endif()
endforeach()
m8_unicorn_cold_sources()
if(APPLE)
    # _POSIX_C_SOURCE hides flock, O_NOFOLLOW and BSD stat fields on Darwin.
    add_compile_definitions(_DARWIN_C_SOURCE)
endif()
set(m8_runtime_engine unicorn)
set(m8_test_engine unicorn)
if(M8_COMBINED_RUNTIME)
    set(m8_runtime_engine unicorn_static)
    set(m8_test_engine m8rt)
endif()
if(M8_UNICORN_PGO STREQUAL "generate" AND BUILD_SHARED_LIBS AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    # Ordinary archive linkage omits unused object files. Their profiles would
    # be missing when the use build compiles every archive source. Include all
    # profile constructors during training; use builds keep normal linkage.
    # LLVM use builds accept functions without profile records.
    get_target_property(unicorn_links unicorn LINK_LIBRARIES)
    set(unicorn_profile_links "")
    foreach(library IN LISTS unicorn_links)
        if(library MATCHES "^(unicorn-common|arm-softmmu)$")
            list(APPEND unicorn_profile_links
                "-Wl,--whole-archive" ${library} "-Wl,--no-whole-archive")
        else()
            list(APPEND unicorn_profile_links ${library})
        endif()
    endforeach()
    set_property(TARGET unicorn PROPERTY LINK_LIBRARIES "${unicorn_profile_links}")
endif()

# The public core uses one runtime ABI with separate implementation areas.
set(M8RT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/core/machine")
set(M8RT_ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/core/engine")
set(M8RT_STORAGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/core/storage")
set(M8RT_PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/core/platform")
set(M8RT_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/core/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/include/m8emu"
    "${M8RT_SOURCE_DIR}" "${M8RT_ENGINE_DIR}" "${M8RT_STORAGE_DIR}"
    "${M8RT_PLATFORM_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/aot")
add_library(m8rt
    ${M8RT_SOURCE_DIR}/core.c
    ${M8RT_SOURCE_DIR}/mmio.c
    ${M8RT_SOURCE_DIR}/usb.c
    ${M8RT_STORAGE_DIR}/storage.c
    ${M8RT_SOURCE_DIR}/audio.c
    ${M8RT_SOURCE_DIR}/audit.c
    ${M8RT_SOURCE_DIR}/watchdog.c
    ${M8RT_STORAGE_DIR}/nor.c
    ${M8RT_ENGINE_DIR}/native-live.c
    ${M8RT_ENGINE_DIR}/native-boot-app.c
    ${M8RT_PLATFORM_DIR}/stall.c)
add_library(m8::runtime ALIAS m8rt)
target_compile_definitions(m8rt PRIVATE M8RT_DIRECT_MEMORY_DEFAULT=$<BOOL:${M8RT_DIRECT_MEMORY}>)
target_compile_definitions(m8rt PRIVATE M8RT_CALLBACK_TIMING_DEFAULT=$<BOOL:${M8_RT_CALLBACK_TIMING}>)
if(M8_NATIVE_DEVICE_FAST)
    target_compile_definitions(m8rt PRIVATE M8RT_NATIVE_DEVICE_FAST=1)
endif()
if(M8_GUEST_PC_PROFILE)
    target_compile_definitions(m8rt PUBLIC M8RT_GUEST_PC_PROFILE)
endif()
if(M8_ACCESS_PROFILE)
    target_compile_definitions(m8rt PUBLIC M8RT_ACCESS_PROFILE)
    target_sources(m8rt PRIVATE ${M8RT_SOURCE_DIR}/access-profile.c)
    target_link_libraries(m8rt PRIVATE ${CMAKE_DL_LIBS})
endif()
if(M8_NATIVE_DEVICE_SITE_PROFILE)
    if(NOT M8_ACCESS_PROFILE)
        message(FATAL_ERROR "M8_NATIVE_DEVICE_SITE_PROFILE needs M8_ACCESS_PROFILE")
    endif()
    target_compile_definitions(m8rt PRIVATE M8RT_DEVICE_SITE_PROFILE=1)
endif()
target_include_directories(m8rt PUBLIC ${M8RT_INCLUDE_DIRS})
target_include_directories(m8rt BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/unicorn/include")
if(M8_NATIVE_ENGINE)
    target_compile_definitions(m8rt PUBLIC M8RT_NATIVE_ENGINE)
    target_sources(m8rt PRIVATE ${M8RT_ENGINE_DIR}/native-engine.c)
    set(M8_NATIVE_STEP_SOURCE "" CACHE FILEPATH "Generated single-step playback fallback C")
    set(M8_NATIVE_REGION_SOURCE "" CACHE FILEPATH "Generated native playback regions")
    if(M8_NATIVE_CORPUS_CATALOG)
        include(cmake/M8NativeCorpus.cmake)
    endif()
    if(NOT EXISTS "${M8_NATIVE_STEP_SOURCE}" AND NOT M8_NATIVE_BOOT AND NOT M8_NATIVE_CORPUS_CATALOG)
        message(FATAL_ERROR "M8_NATIVE_ENGINE needs M8_NATIVE_STEP_SOURCE; generate it with aot/native_step.py")
    endif()
    if(EXISTS "${M8_NATIVE_STEP_SOURCE}" OR M8_NATIVE_CORPUS_CATALOG)
        add_library(m8native-step SHARED "${M8_NATIVE_STEP_SOURCE}")
        target_include_directories(m8native-step PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/aot")
        if(M8_NATIVE_CORPUS_CATALOG)
            target_include_directories(m8native-step BEFORE PRIVATE "${m8_native_corpus_dir}/headers")
        endif()
        target_compile_definitions(m8native-step PRIVATE M8_AOT_NATIVE_FP)
        target_compile_options(m8native-step PRIVATE -O2 -frounding-math -ffp-contract=off -fno-math-errno
            -Wall -Wextra -Wno-unused-label -Wno-unused-variable -Wno-unused-but-set-variable)
        target_link_libraries(m8native-step PRIVATE m)
        set_target_properties(m8native-step PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/aot")
    endif()
    if(M8_NATIVE_BOOT)
        include(cmake/M8NativeBoot.cmake)
    endif()
    set(M8_NATIVE_REGION_SOURCE "" CACHE FILEPATH "Generated native playback regions")
    set(M8_NATIVE_REGION_PC_FRAGMENTS "" CACHE STRING "Additional native playback PC fragments")
    set(M8_NATIVE_REGION_LAYOUT "" CACHE FILEPATH "Measured native region grouping")
    if(M8_NATIVE_REGION_COMPUTED_REDIRECTS)
        if(NOT M8_NATIVE_REGION_CHAIN)
            message(FATAL_ERROR "Computed native continuations: M8_NATIVE_REGION_CHAIN=${M8_NATIVE_REGION_CHAIN}; required=ON")
        endif()
        if(NOT M8_NATIVE_REGIONS)
            message(FATAL_ERROR "Computed native continuations: M8_NATIVE_REGIONS=${M8_NATIVE_REGIONS}; required=ON")
        endif()
        if(M8_NATIVE_REGION_SOURCE AND NOT M8_NATIVE_CORPUS_CATALOG)
            message(FATAL_ERROR "Computed native continuations: M8_NATIVE_REGION_SOURCE=${M8_NATIVE_REGION_SOURCE}; required=empty for automatic generation")
        endif()
    endif()
    if(M8_NATIVE_REGIONS AND NOT M8_NATIVE_REGION_SOURCE)
        set(M8_NATIVE_PYTHON "${M8_AOT_PYTHON}"
            CACHE FILEPATH "Host Python for native playback generation and checks")
        set(M8_NATIVE_REGION_FIRMWARE "${M8_FIRMWARE_BIN}"
            CACHE FILEPATH "Original firmware for measured playback regions")
        set(M8_NATIVE_REGION_SOURCE "${CMAKE_BINARY_DIR}/native-regions/generated.c")
        set(m8_native_region_dir "${CMAKE_CURRENT_SOURCE_DIR}/aot")
        set(m8_native_region_generator_options "")
        set(m8_native_region_chain_command "")
        set(m8_native_region_redirect_command "")
        set(m8_native_region_extra_inputs "")
        set(m8_native_region_chain_options "")
        if(M8_NATIVE_REGION_EVENT_LOOP)
            list(APPEND m8_native_region_chain_options --event-loop)
        endif()
        if(M8_NATIVE_REGION_INLINE_SERVICES)
            list(APPEND m8_native_region_generator_options --inline-services)
        endif()
        if(M8_NATIVE_REGION_RAM_EXCLUSIVES)
            list(APPEND m8_native_region_generator_options --ram-exclusives)
        endif()
        if(M8_NATIVE_REGION_SHARED_STATE)
            list(APPEND m8_native_region_generator_options --shared-state)
        endif()
        foreach(m8_fragment IN LISTS M8_NATIVE_REGION_PC_FRAGMENTS)
            if(NOT EXISTS "${m8_fragment}")
                message(FATAL_ERROR "Native playback PC fragment does not exist: ${m8_fragment}")
            endif()
            list(APPEND m8_native_region_generator_options --pc-file "${m8_fragment}")
            list(APPEND m8_native_region_extra_inputs "${m8_fragment}")
        endforeach()
        if(M8_NATIVE_REGION_LAYOUT)
            if(NOT EXISTS "${M8_NATIVE_REGION_LAYOUT}")
                message(FATAL_ERROR "Native playback layout does not exist: ${M8_NATIVE_REGION_LAYOUT}")
            endif()
            list(APPEND m8_native_region_generator_options --layout "${M8_NATIVE_REGION_LAYOUT}")
            list(APPEND m8_native_region_extra_inputs "${M8_NATIVE_REGION_LAYOUT}")
        endif()
        file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/native-regions/generator-options.txt"
            CONTENT "chain=${M8_NATIVE_REGION_CHAIN}\nfragments=${M8_NATIVE_REGION_PC_FRAGMENTS}\nlayout=${M8_NATIVE_REGION_LAYOUT}\ncomputed=${M8_NATIVE_REGION_COMPUTED_REDIRECTS}\ninline_services=${M8_NATIVE_REGION_INLINE_SERVICES}\nram_exclusives=${M8_NATIVE_REGION_RAM_EXCLUSIVES}\nevent_loop=${M8_NATIVE_REGION_EVENT_LOOP}\nshared_state=${M8_NATIVE_REGION_SHARED_STATE}\n")
        if(M8_NATIVE_REGION_CHAIN)
            list(APPEND m8_native_region_generator_options --chain-ready)
            set(m8_native_region_chain_command COMMAND "${M8_NATIVE_PYTHON}" -B
                "${m8_native_region_dir}/native_region_chain.py"
                --input "${M8_NATIVE_REGION_SOURCE}"
                --manifest "${CMAKE_BINARY_DIR}/native-regions/manifest.json"
                --output "${M8_NATIVE_REGION_SOURCE}" ${m8_native_region_chain_options})
        endif()
        if(M8_NATIVE_REGION_COMPUTED_REDIRECTS)
            set(m8_native_region_redirect_targets "${CMAKE_CURRENT_SOURCE_DIR}/aot/catalogs/redirect-targets.txt")
            if(M8_NATIVE_REGION_INLINE_SERVICES)
                set(m8_native_region_redirect_targets "${CMAKE_CURRENT_SOURCE_DIR}/aot/catalogs/redirect-targets-services.txt")
            endif()
            set(m8_native_region_redirect_command COMMAND "${M8_NATIVE_PYTHON}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/aot/native-engine-redirects.py"
                --source "${M8_NATIVE_REGION_SOURCE}"
                --region-manifest "${CMAKE_BINARY_DIR}/native-regions/manifest.json"
                --targets "${m8_native_region_redirect_targets}"
                --fuse-computed --output "${M8_NATIVE_REGION_SOURCE}"
                --manifest "${CMAKE_BINARY_DIR}/native-regions/redirects-manifest.json")
            list(APPEND m8_native_region_extra_inputs
                "${CMAKE_CURRENT_SOURCE_DIR}/aot/native-engine-redirects.py"
                "${m8_native_region_redirect_targets}")
        endif()
        add_custom_command(OUTPUT "${M8_NATIVE_REGION_SOURCE}"
            BYPRODUCTS "${CMAKE_BINARY_DIR}/native-regions/manifest.json"
            COMMAND "${M8_NATIVE_PYTHON}" -B "${m8_native_region_dir}/native_regions.py"
                --firmware "${M8_NATIVE_REGION_FIRMWARE}"
                --pc-file "${m8_native_region_dir}/functions-playback-native-pcs.txt"
                --output "${M8_NATIVE_REGION_SOURCE}"
                --manifest "${CMAKE_BINARY_DIR}/native-regions/manifest.json"
                ${m8_native_region_generator_options}
            ${m8_native_region_chain_command}
            ${m8_native_region_redirect_command}
            DEPENDS "${M8_NATIVE_REGION_FIRMWARE}"
                "${CMAKE_BINARY_DIR}/native-regions/generator-options.txt"
                ${m8_native_region_extra_inputs}
                "${m8_native_region_dir}/functions-playback-native-pcs.txt"
                "${m8_native_region_dir}/native_regions.py"
                "${m8_native_region_dir}/native_region_services.py"
                "${m8_native_region_dir}/native_region_services.h"
                "${m8_native_region_dir}/native_region_layout.py"
                "${m8_native_region_dir}/native_region_chain.py"
                "${m8_native_region_dir}/native_region_chain.h"
                "${m8_native_region_dir}/native_region_event_chain.h"
                "${m8_native_region_dir}/native_region_state.py"
                "${m8_native_region_dir}/codegen_v2.py"
                "${m8_native_region_dir}/native_events.h"
                "${m8_native_region_dir}/native_regions.h"
                "${m8_native_region_dir}/translate.py"
                "${m8_native_region_dir}/emit_int.py" "${m8_native_region_dir}/emit_fp.py"
            COMMENT "Compile measured playback tail into native regions" VERBATIM)
    endif()
    if(M8_NATIVE_REGION_SOURCE)
        if(NOT M8_NATIVE_REGIONS AND NOT EXISTS "${M8_NATIVE_REGION_SOURCE}")
            message(FATAL_ERROR "M8_NATIVE_REGION_SOURCE does not exist")
        endif()
        add_library(m8native-regions SHARED "${M8_NATIVE_REGION_SOURCE}")
        if(M8_NATIVE_CORPUS_CATALOG)
            target_sources(m8native-regions PRIVATE "${m8_native_corpus_dir}/hot.c" "${m8_native_corpus_dir}/cold.c")
            target_include_directories(m8native-regions BEFORE PRIVATE "${m8_native_corpus_dir}/headers")
        endif()
        target_include_directories(m8native-regions PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/aot")
        target_compile_definitions(m8native-regions PRIVATE M8_AOT_NATIVE_FP)
        if(M8_NATIVE_REGION_SHARED_STATE)
            if(EXISTS "${M8_NATIVE_REGION_SOURCE}" AND NOT m8_native_region_dir)
                file(STRINGS "${M8_NATIVE_REGION_SOURCE}" m8_region_state_marker
                    LIMIT_COUNT 1 REGEX "m8-native-region-shared-state/1")
                if(NOT m8_region_state_marker)
                    message(FATAL_ERROR "Shared region state: source=${M8_NATIVE_REGION_SOURCE}; marker=absent; required=m8-native-region-shared-state/1")
                endif()
            endif()
            target_compile_definitions(m8native-regions PRIVATE M8_NATIVE_REGION_SHARED_STATE)
        endif()
        target_compile_definitions(m8native-regions PRIVATE
            M8_NATIVE_REGION_INTEGER_FP_FASTPATH=$<BOOL:${M8_NATIVE_REGION_INTEGER_FP_FASTPATH}>
            M8_NATIVE_REGION_SERVICE_CONTINUE=$<BOOL:${M8_NATIVE_REGION_SERVICE_CONTINUE}>
            M8_NATIVE_REGION_INLINE_MEMORY=$<BOOL:${M8_NATIVE_REGION_INLINE_MEMORY}>)
        target_compile_options(m8native-regions PRIVATE -O2 -frounding-math -ffp-contract=off -fno-math-errno
            -Wall -Wextra -Wno-unused-label -Wno-unused-variable -Wno-unused-but-set-variable)
        target_link_libraries(m8native-regions PRIVATE m)
        set_target_properties(m8native-regions PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/aot")
    endif()
    if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmark" AND NOT CMAKE_CROSSCOMPILING)
        set(M8_NATIVE_PYTHON "${M8_AOT_PYTHON}"
            CACHE FILEPATH "Host Python with capstone and Unicorn for native-step checks")
        add_test(NAME native_step_contract COMMAND "${M8_NATIVE_PYTHON}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_native_step.py")
        set_tests_properties(native_step_contract PROPERTIES TIMEOUT 60
            ENVIRONMENT "LIBUNICORN_PATH=$<TARGET_FILE_DIR:unicorn>")
        add_test(NAME native_service_boundaries COMMAND "${M8_NATIVE_PYTHON}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/machine/test_native_services.py"
            --runtime "$<TARGET_FILE:m8rt>" --engine "$<TARGET_FILE:unicorn>")
        set_tests_properties(native_service_boundaries PROPERTIES TIMEOUT 120
            ENVIRONMENT "LIBUNICORN_PATH=$<TARGET_FILE_DIR:unicorn>")
        add_test(NAME native_event_boundaries COMMAND "${M8_NATIVE_PYTHON}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/machine/test_native_events.py"
            --engine "$<TARGET_FILE:unicorn>"
            --output "${CMAKE_BINARY_DIR}/native-event-controls")
        set_tests_properties(native_event_boundaries PROPERTIES TIMEOUT 170)
        add_test(NAME native_device_boundaries COMMAND "${M8_NATIVE_PYTHON}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/machine/test_native_devices.py"
            --engine "$<TARGET_FILE:unicorn>"
            --output "${CMAKE_BINARY_DIR}/native-device-controls")
        set_tests_properties(native_device_boundaries PROPERTIES TIMEOUT 170)
        add_test(NAME native_device_fast_boundaries COMMAND "${M8_NATIVE_PYTHON}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/machine/test_native_device_fast.py"
            --engine "$<TARGET_FILE:unicorn>"
            --output "${CMAKE_BINARY_DIR}/native-device-fast-controls")
        set_tests_properties(native_device_fast_boundaries PROPERTIES TIMEOUT 300
            ENVIRONMENT "CC=${CMAKE_C_COMPILER}")
        add_test(NAME native_exclusive_boundaries COMMAND "${M8_NATIVE_PYTHON}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/machine/test_native_exclusives.py"
            --engine "$<TARGET_FILE:unicorn>"
            --output "${CMAKE_BINARY_DIR}/native-exclusive-controls")
        set_tests_properties(native_exclusive_boundaries PROPERTIES TIMEOUT 170)
        add_test(NAME native_boot_irq_boundaries COMMAND "${M8_NATIVE_PYTHON}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/machine/native_boot_irq_control.py"
            --working-tree --native --engine "$<TARGET_FILE:unicorn>"
            --output "${CMAKE_BINARY_DIR}/native-boot-irq-controls")
        set_tests_properties(native_boot_irq_boundaries PROPERTIES TIMEOUT 170)
        add_test(NAME native_boot_app_irq_boundaries COMMAND "${M8_NATIVE_PYTHON}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/machine/native_boot_irq_control.py"
            --working-tree --native --app-guard --engine "$<TARGET_FILE:unicorn>"
            --output "${CMAKE_BINARY_DIR}/native-boot-app-irq-controls")
        set_tests_properties(native_boot_app_irq_boundaries PROPERTIES TIMEOUT 170)
        add_test(NAME native_boot_session_contract COMMAND "${M8_NATIVE_PYTHON}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/test_native_boot_session.py")
        set_tests_properties(native_boot_session_contract PROPERTIES TIMEOUT 30)
        if(M8_NATIVE_REGIONS)
            if(M8_NATIVE_REGION_CHAIN)
                add_test(NAME native_region_chain_contract COMMAND "${M8_NATIVE_PYTHON}" -B
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_native_region_chain.py")
                set_tests_properties(native_region_chain_contract PROPERTIES TIMEOUT 60)
                add_test(NAME native_region_redirect_contract COMMAND "${M8_NATIVE_PYTHON}" -B
                    "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/test_native_engine_redirects.py")
                set_tests_properties(native_region_redirect_contract PROPERTIES TIMEOUT 120
                    ENVIRONMENT "LIBUNICORN_PATH=$<TARGET_FILE_DIR:unicorn>")
            endif()
            add_test(NAME native_region_handler_contract COMMAND "${M8_NATIVE_PYTHON}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_native_region_handlers.py")
            set_tests_properties(native_region_handler_contract PROPERTIES TIMEOUT 120
                ENVIRONMENT "LIBUNICORN_PATH=$<TARGET_FILE_DIR:unicorn>")
            add_test(NAME native_region_memory_contract COMMAND "${M8_NATIVE_PYTHON}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_native_region_memory.py")
            set_tests_properties(native_region_memory_contract PROPERTIES TIMEOUT 30)
            add_test(NAME native_region_services_contract COMMAND "${M8_NATIVE_PYTHON}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_native_region_services.py")
            set_tests_properties(native_region_services_contract PROPERTIES TIMEOUT 120
                ENVIRONMENT "LIBUNICORN_PATH=$<TARGET_FILE_DIR:unicorn>")
            add_test(NAME native_region_ram_exclusive_contract COMMAND "${M8_NATIVE_PYTHON}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_native_region_ram_exclusives.py")
            set_tests_properties(native_region_ram_exclusive_contract PROPERTIES TIMEOUT 120
                ENVIRONMENT "LIBUNICORN_PATH=$<TARGET_FILE_DIR:unicorn>")
            add_test(NAME native_region_layout_contract COMMAND "${M8_NATIVE_PYTHON}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_native_region_layout.py")
            set_tests_properties(native_region_layout_contract PROPERTIES TIMEOUT 30)
            add_test(NAME native_region_contract COMMAND "${M8_NATIVE_PYTHON}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_native_regions.py")
            set_tests_properties(native_region_contract PROPERTIES TIMEOUT 120
                ENVIRONMENT "LIBUNICORN_PATH=$<TARGET_FILE_DIR:unicorn>")
        endif()
        if(M8_NATIVE_AOT_CODE_GUARD)
            add_test(NAME native_aot_guard_contract COMMAND "${M8_NATIVE_PYTHON}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_native_aot_guard.py")
            set_tests_properties(native_aot_guard_contract PROPERTIES TIMEOUT 120
                ENVIRONMENT "LIBUNICORN_PATH=$<TARGET_FILE_DIR:unicorn>")
        endif()
    endif()
endif()
if(M8_RT_LOCAL_CALLS)
    target_compile_definitions(m8rt PRIVATE M8RT_LOCAL_CALLS)
endif()
find_package(Threads REQUIRED)
target_link_libraries(m8rt PRIVATE ${m8_runtime_engine} m Threads::Threads)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(m8rt PRIVATE -Wall -Wextra)
    # Match the Makefile's rounding contract for PCM conversion.
    set_source_files_properties(${M8RT_SOURCE_DIR}/audio.c PROPERTIES
        COMPILE_OPTIONS "-fno-fast-math;-frounding-math")
endif()
m8_tune_target(m8rt RT)
set_target_properties(m8rt PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/m8rt"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/m8rt"
    BUILD_RPATH_USE_ORIGIN ON
    INSTALL_RPATH "${M8_RPATH_ORIGIN}/../unicorn")
add_executable(m8rt-cli ${M8RT_SOURCE_DIR}/cli.c)
target_link_libraries(m8rt-cli PRIVATE m8rt)
set_target_properties(m8rt-cli PROPERTIES
    OUTPUT_NAME m8rt RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/m8rt"
    BUILD_RPATH_USE_ORIGIN ON INSTALL_RPATH "${M8_RPATH_ORIGIN}")

if(M8_BUILD_AOT)
    set(M8_AOT_SOURCE ""
        CACHE FILEPATH "Generated AOT C source")
    set(M8_AOT_PROFILE fast CACHE STRING "AOT floating-point profile: fast or exact")
    set_property(CACHE M8_AOT_PROFILE PROPERTY STRINGS fast exact)
    set(M8_AOT_DEFINITIONS "" CACHE STRING "Additional compile definitions for the selected AOT source")
    option(M8_AOT_MEMORY_SPANS "Qualify invariant function memory spans" ON)
    option(M8_AOT_LOCAL_NZCV "Keep condition flags in local values" ON)
    option(M8_AOT_HYPER_VOICE "Batch experimental unfiltered HYPERSYNTH voice blocks" OFF)
    option(M8_AOT_SAMPLER_VOICE "Batch experimental mono16 unfiltered SAMPLER voice blocks" OFF)
    option(M8_AOT_SAMPLER_LOOPS "Permit bounded forward loops in SAMPLER voice blocks" OFF)
    if(M8_NATIVE_PERFORMANCE_PROFILE)
        set(M8_AOT_SAMPLER_VOICE ON)
        set(M8_AOT_SAMPLER_LOOPS ON)
        set(M8_AOT_HYPER_VOICE ON)
        set(M8_AOT_AUDIO_POOL ON)
    endif()
    option(M8_AOT_IT_PREFLIGHT "Qualify memory before IT block effects" ON)
    option(M8_AOT_FIXED_MEMORY "Require fixed-offset guest memory in the fast library" ON)
    option(M8_AOT_VALIDITY_FIRST "Check fixed scalar spans before forming host pointers" ${M8_AOT_FIXED_MEMORY})
    option(M8_AOT_CALL_DISPATCH "Use tagged computed-call lookup" ON)
    option(M8_AOT_EXACT_ENTRY_LEAVES "Use exact-entry register leaves" ON)
    option(M8_AOT_REGION_SPANS "Use bounded internal memory spans" ON)
    option(M8_AOT_POINTEE_SPANS "Cache checked pointers in bounded sections" ON)
    option(M8_AOT_BRAIDS_MODE0 "Use the bounded mode-zero filter quantum" ON)
    option(M8_AOT_ARM64_FP_HARDWARE "Use qualified ARM64 FP operations without NaN guards" ${M8_AOT_ARM64_FP_HARDWARE_DEFAULT})
    option(M8_AOT_INT_FLOAT_SCALE "Use direct F32 fixed-point conversion scaling" ON)
    if(M8_NATIVE_PERFORMANCE_SOURCE_COPY AND
            (NOT M8_AOT_PROFILE STREQUAL "fast" OR NOT M8_AOT_FIXED_MEMORY))
        message(FATAL_ERROR "Native performance source copies require the fast fixed-memory AOT profile")
    endif()
    set(M8_AOT_FMA auto CACHE STRING "x86 FMA: auto, on or off")
    set_property(CACHE M8_AOT_FMA PROPERTY STRINGS auto on off)
    if(M8_AOT_PROFILE STREQUAL "fast" AND M8_AOT_VALIDITY_FIRST AND NOT M8_AOT_FIXED_MEMORY)
        message(FATAL_ERROR "M8_AOT_VALIDITY_FIRST requires M8_AOT_FIXED_MEMORY")
    endif()
    set(aot_generator_flags)
    foreach(option_suffix IN ITEMS MEMORY_SPANS LOCAL_NZCV IT_PREFLIGHT CALL_DISPATCH EXACT_ENTRY_LEAVES )
        if(M8_AOT_${option_suffix})
            string(TOLOWER "${option_suffix}" generator_flag)
            string(REPLACE "_" "-" generator_flag "${generator_flag}")
            list(APPEND aot_generator_flags "--${generator_flag}")
        endif()
    endforeach()
    set(aot_fma OFF)
    string(REGEX MATCH "(^|;)(M8_AOT_DIAGNOSTIC|M8_AOT_GUEST_COUNT)(=[^;]*)?(;|$)"
        aot_instrumented "${M8_AOT_DEFINITIONS}")
    if(M8_AOT_SAMPLER_LOOPS AND NOT M8_AOT_SAMPLER_VOICE)
        message(FATAL_ERROR "M8_AOT_SAMPLER_LOOPS requires M8_AOT_SAMPLER_VOICE")
    endif()
    if(M8_AOT_PROFILE STREQUAL "fast" AND NOT aot_instrumented)
        foreach(voice_option IN ITEMS HYPER_VOICE SAMPLER_VOICE)
            if(M8_AOT_${voice_option} AND NOT M8_AOT_FIXED_MEMORY)
                message(FATAL_ERROR "M8_AOT_${voice_option} requires M8_AOT_FIXED_MEMORY")
            endif()
        endforeach()
    endif()
    if(NOT M8_AOT_FMA MATCHES "^(auto|on|off)$")
        message(FATAL_ERROR "M8_AOT_FMA must be auto, on or off")
    endif()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$" AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        if(M8_AOT_FMA STREQUAL "on")
            set(aot_fma ON)
        elseif(M8_AOT_FMA STREQUAL "auto" AND NOT CMAKE_CROSSCOMPILING)
            include(CheckCSourceRuns)
            check_c_source_runs("int main(void) { __builtin_cpu_init(); return __builtin_cpu_supports(\"fma\") ? 0 : 1; }" M8_HOST_HAS_FMA)
            set(aot_fma ${M8_HOST_HAS_FMA})
        endif()
    elseif(M8_AOT_FMA STREQUAL "on")
        message(FATAL_ERROR "M8_AOT_FMA=on requires an x86-64 GCC/Clang target")
    endif()
    set(M8_AOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/aot")
    if(NOT M8_AOT_SOURCE)
        set(M8_AOT_FIRMWARE "${M8_FIRMWARE_BIN}"
            CACHE FILEPATH "Original Headless 6.5.2C firmware for AOT generation")
        set(M8_AOT_PYTHON python3 CACHE STRING "Host Python with capstone for AOT generation")
        if(NOT EXISTS "${M8_AOT_FIRMWARE}" OR NOT EXISTS "${M8_AOT_DIR}/translate.py")
            message(FATAL_ERROR "AOT needs its translator and original 6.5.2C firmware; set M8_AOT_FIRMWARE or M8_AOT_SOURCE")
        endif()
        execute_process(COMMAND "${M8_AOT_PYTHON}" -c "import capstone"
            RESULT_VARIABLE capstone_status ERROR_QUIET)
        if(NOT capstone_status EQUAL 0)
            message(FATAL_ERROR "M8_AOT_PYTHON needs capstone; install aot/requirements.txt")
        endif()
        set(M8_AOT_SOURCE "${CMAKE_BINARY_DIR}/aot/generated.c")
        file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/aot/generator-options.txt"
            CONTENT "firmware=${M8_AOT_FIRMWARE}\npython=${M8_AOT_PYTHON}\nflags=${aot_generator_flags}\n")
        file(GLOB aot_generator_inputs CONFIGURE_DEPENDS "${M8_AOT_DIR}/*.py" "${M8_AOT_DIR}/*.txt")
        add_custom_command(OUTPUT "${M8_AOT_SOURCE}"
            BYPRODUCTS "${CMAKE_BINARY_DIR}/aot/manifest.json"
            COMMAND "${M8_AOT_PYTHON}" -B "${M8_AOT_DIR}/translate.py"
                --firmware "${M8_AOT_FIRMWARE}"
                --functions "${M8_AOT_DIR}/functions.txt"
                --hooks "${M8_AOT_DIR}/enabled-maintenance.txt"
                --entry-hooks "${M8_AOT_DIR}/enabled-maintenance-entries.txt"
                --function-fragment "${M8_AOT_DIR}/functions-fm.txt"
                --function-fragment "${M8_AOT_DIR}/functions-braids.txt"
                --function-fragment "${M8_AOT_DIR}/functions-mixed.txt"
                --function-fragment "${M8_AOT_DIR}/functions-coverage.txt"
                --function-fragment "${M8_AOT_DIR}/functions-macro-all.txt"
                --function-fragment "${M8_AOT_DIR}/functions-control.txt"
                --function-fragment "${M8_AOT_DIR}/functions-maintenance.txt"
                ${aot_generator_flags}
                --output "${M8_AOT_SOURCE}" --manifest "${CMAKE_BINARY_DIR}/aot/manifest.json"
            DEPENDS "${M8_AOT_FIRMWARE}" ${aot_generator_inputs}
                "${CMAKE_BINARY_DIR}/aot/generator-options.txt"
            COMMENT "Translate selected original-firmware DSP functions" VERBATIM)
        add_custom_target(m8aot-generate DEPENDS "${M8_AOT_SOURCE}")
    elseif(NOT EXISTS "${M8_AOT_SOURCE}")
        message(FATAL_ERROR "M8_AOT_SOURCE does not exist: ${M8_AOT_SOURCE}")
    endif()
    get_filename_component(aot_include_dir "${M8_AOT_SOURCE}" DIRECTORY)
    if(M8_NATIVE_AOT_CODE_GUARD)
        set(m8_native_guard_python "${M8_AOT_PYTHON}")
        if(NOT m8_native_guard_python)
            set(m8_native_guard_python python3)
        endif()
        file(GLOB m8_native_guard_headers CONFIGURE_DEPENDS "${M8_AOT_DIR}/*.h")
        set(m8_unguarded_aot_source "${M8_AOT_SOURCE}")
        set(M8_AOT_SOURCE "${CMAKE_BINARY_DIR}/native-aot-guard/generated.c")
        add_custom_command(OUTPUT "${M8_AOT_SOURCE}"
            BYPRODUCTS "${CMAKE_BINARY_DIR}/native-aot-guard/manifest.json"
            COMMAND "${m8_native_guard_python}" -B "${M8_AOT_DIR}/native_aot_guard.py"
                --input "${m8_unguarded_aot_source}" --output "${CMAKE_BINARY_DIR}/native-aot-guard"
            DEPENDS "${m8_unguarded_aot_source}" "${M8_AOT_DIR}/native_aot_guard.py"
                ${m8_native_guard_headers}
            COMMENT "Guard code writes in the native playback AOT copy" VERBATIM)
        get_filename_component(aot_include_dir "${M8_AOT_SOURCE}" DIRECTORY)
    endif()
    if(M8_NATIVE_PERFORMANCE_SOURCE_COPY)
        set(m8_performance_aot_input "${M8_AOT_SOURCE}")
        set(M8_AOT_SOURCE "${CMAKE_BINARY_DIR}/native-performance/generated.c")
        add_custom_command(OUTPUT "${M8_AOT_SOURCE}"
            BYPRODUCTS "${CMAKE_BINARY_DIR}/native-performance/manifest.json"
            COMMAND "${m8_native_guard_python}" -B "${M8_AOT_DIR}/native_performance.py"
                --input "${CMAKE_BINARY_DIR}/native-aot-guard"
                --output "${CMAKE_BINARY_DIR}/native-performance" --enable --replace
            DEPENDS "${m8_performance_aot_input}" "${M8_AOT_DIR}/native_performance.py"
                "${M8_AOT_DIR}/audio_pool.py" "${M8_AOT_DIR}/audio_pool.h"
                "${M8_AOT_DIR}/audio_pool_memset.h"
                "${M8_AOT_DIR}/readonly_literals.py"
                ${m8_native_guard_headers}
            COMMENT "Build the checked combined native performance source copy" VERBATIM)
        get_filename_component(aot_include_dir "${M8_AOT_SOURCE}" DIRECTORY)
    elseif(M8_AOT_AUDIO_POOL)
        if(NOT M8_NATIVE_ENGINE OR NOT M8_NATIVE_AOT_CODE_GUARD)
            message(FATAL_ERROR "M8_AOT_AUDIO_POOL requires native engine and guarded AOT code")
        endif()
        set(m8_pool_aot_input "${M8_AOT_SOURCE}")
        set(M8_AOT_SOURCE "${CMAKE_BINARY_DIR}/audio-pool/generated.c")
        add_custom_command(OUTPUT "${M8_AOT_SOURCE}"
            BYPRODUCTS "${CMAKE_BINARY_DIR}/audio-pool/manifest.json"
            COMMAND "${m8_native_guard_python}" -B "${M8_AOT_DIR}/audio_pool.py"
                --input "${m8_pool_aot_input}" --output "${CMAKE_BINARY_DIR}/audio-pool"
            DEPENDS "${m8_pool_aot_input}" "${M8_AOT_DIR}/audio_pool.py"
                "${M8_AOT_DIR}/audio_pool.h" "${M8_AOT_DIR}/audio_pool_memset.h"
            COMMENT "Add exact native audio-buffer pool kernels" VERBATIM)
        get_filename_component(aot_include_dir "${M8_AOT_SOURCE}" DIRECTORY)
    endif()
    if(M8_AOT_CODEGEN_V2)
        if(NOT M8_AOT_PROFILE STREQUAL "fast" OR NOT M8_AOT_FIXED_MEMORY)
            message(FATAL_ERROR "M8_AOT_CODEGEN_V2 requires the fast fixed-memory profile")
        endif()
        get_filename_component(m8_codegen_v2_input_dir "${M8_AOT_SOURCE}" DIRECTORY)
        set(m8_codegen_v2_input "${M8_AOT_SOURCE}")
        # Keep each copier input family in its own complete, checked bundle.
        # Switching parked options must not invalidate another family's files.
        set(m8_codegen_v2_output "${CMAKE_BINARY_DIR}/codegen-v2")
        if(M8_NATIVE_PERFORMANCE_SOURCE_COPY)
            string(APPEND m8_codegen_v2_output "-performance")
        endif()
        set(M8_AOT_SOURCE "${m8_codegen_v2_output}/generated.c")
        add_custom_command(OUTPUT "${M8_AOT_SOURCE}"
            BYPRODUCTS "${m8_codegen_v2_output}/manifest.json"
            COMMAND "${m8_native_guard_python}" -B "${M8_AOT_DIR}/codegen_v2.py"
                --input "${m8_codegen_v2_input_dir}"
                --output "${m8_codegen_v2_output}" --replace
                --structural
            DEPENDS "${m8_codegen_v2_input}" "${M8_AOT_DIR}/codegen_v2.py"
                "${M8_AOT_DIR}/codegen_entry.py"
                ${m8_native_guard_headers}
            COMMENT "Build checked hot entries, cold exits and register calls" VERBATIM)
        get_filename_component(aot_include_dir "${M8_AOT_SOURCE}" DIRECTORY)
    endif()
    set(aot_targets m8aot)
    if(M8_BUILD_AOT_COUNTERS)
        list(APPEND aot_targets m8aot-count)
    endif()
    foreach(aot_target IN LISTS aot_targets)
        if(M8_COMBINED_RUNTIME AND aot_target STREQUAL "m8aot")
            add_library(${aot_target} STATIC ${M8_AOT_SOURCE})
        else()
            add_library(${aot_target} ${M8_AOT_SOURCE})
        endif()
        if(TARGET m8aot-generate)
            add_dependencies(${aot_target} m8aot-generate)
        endif()
        target_include_directories(${aot_target} PRIVATE "${aot_include_dir}"
            "${CMAKE_CURRENT_SOURCE_DIR}/aot")
        target_compile_definitions(${aot_target} PRIVATE ${M8_AOT_DEFINITIONS})
        if(M8_AOT_CODEGEN_V2)
            target_compile_definitions(${aot_target} PRIVATE M8_AOT_CODEGEN_V2)
        endif()
        if(M8_AOT_AUDIO_POOL)
            target_compile_definitions(${aot_target} PRIVATE M8_AOT_AUDIO_POOL)
        endif()
        if(M8_AOT_PROFILE STREQUAL "fast")
            target_compile_definitions(${aot_target} PRIVATE M8_AOT_NATIVE_FP M8_AOT_FAST_MEMORY
                M8_AOT_NARROW_CALLS M8_AOT_DWT_RAM M8_AOT_COOPERATIVE_SCS)
            foreach(option_suffix IN ITEMS FIXED_MEMORY  VALIDITY_FIRST   REGION_SPANS POINTEE_SPANS    ARM64_FP_HARDWARE BRAIDS_MODE0 INT_FLOAT_SCALE        HYPER_VOICE SAMPLER_VOICE SAMPLER_LOOPS      )
                if(M8_AOT_${option_suffix})
                    target_compile_definitions(${aot_target} PRIVATE M8_AOT_${option_suffix})
                endif()
            endforeach()
        elseif(NOT M8_AOT_PROFILE STREQUAL "exact")
            message(FATAL_ERROR "M8_AOT_PROFILE must be fast or exact")
        endif()
        if(aot_fma)
            target_compile_options(${aot_target} PRIVATE -mfma)
        endif()
        target_link_libraries(${aot_target} PRIVATE m)
        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${aot_target} PRIVATE -O2 -frounding-math -ffp-contract=off
                -fno-math-errno -Wall -Wextra -Wno-unused-label -Wno-unused-variable
                -Wno-unused-but-set-variable)
            # Preserve the AOT baseline optimization level during LTO too.
            target_link_options(${aot_target} PRIVATE -O2)
        endif()
        if(M8_LOW_MEMORY_BUILD AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${aot_target} PRIVATE
                --param=ggc-min-expand=10 --param=ggc-min-heapsize=32768)
        endif()
        set_target_properties(${aot_target} PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/aot"
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/aot")
    endforeach()
    if(M8_AOT_CODEGEN_V2 AND BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING AND
            EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_codegen_v2.py")
        add_test(NAME codegen_v2_copy_contract COMMAND "${m8_native_guard_python}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_codegen_v2.py")
        set_tests_properties(codegen_v2_copy_contract PROPERTIES TIMEOUT 90)
        foreach(contract entry)
            add_test(NAME codegen_v2_${contract}_contract COMMAND "${m8_native_guard_python}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_codegen_${contract}.py")
            set_tests_properties(codegen_v2_${contract}_contract PROPERTIES TIMEOUT 90)
        endforeach()
    endif()
    if(M8_NATIVE_PERFORMANCE_SOURCE_COPY AND BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING AND
            EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_native_performance.py")
        add_test(NAME native_performance_copy_contract COMMAND "${m8_native_guard_python}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_native_performance.py" --cc "${CMAKE_C_COMPILER}"
            --firmware "${M8_AOT_FIRMWARE}")
        set_tests_properties(native_performance_copy_contract PROPERTIES TIMEOUT 90)
    endif()
    if(M8_AOT_AUDIO_POOL AND BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING AND
            EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_audio_pool_copy.py")
        add_test(NAME audio_pool_copy_contract COMMAND "${M8_NATIVE_PYTHON}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_audio_pool_copy.py" --cc "${CMAKE_C_COMPILER}"
            --firmware "${M8_AOT_FIRMWARE}")
        set_tests_properties(audio_pool_copy_contract PROPERTIES TIMEOUT 60)
        add_test(NAME audio_pool_original_contract COMMAND "${M8_NATIVE_PYTHON}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_audio_pool.py" --build-helper
            --firmware "${M8_AOT_FIRMWARE}" --output "${CMAKE_BINARY_DIR}/audio-pool-controls")
        set_tests_properties(audio_pool_original_contract PROPERTIES TIMEOUT 120)
        add_test(NAME audio_pool_memset_contract COMMAND "${M8_NATIVE_PYTHON}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_audio_pool_memset.py" --cc "${CMAKE_C_COMPILER}"
            --firmware "${M8_AOT_FIRMWARE}")
        set_tests_properties(audio_pool_memset_contract PROPERTIES TIMEOUT 60)
    endif()
    m8_tune_target(m8aot AOT)
    m8_order_aot(m8aot)
    add_library(m8::aot ALIAS m8aot)
    if(M8_BUILD_AOT_COUNTERS)
        target_compile_definitions(m8aot-count PRIVATE M8_AOT_GUEST_COUNT)
        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            # This diagnostic artifact supplies counts, not timing results.
            target_compile_options(m8aot-count PRIVATE -O1)
        endif()
        # Do not compile both large translations at once on small ARM64 hosts.
        add_dependencies(m8aot-count m8aot)
    endif()
endif()

if(M8_NATIVE_TLS_INITIAL_EXEC)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR
            NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64|aarch64|arm64|ARM64)$" OR
            NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "M8_NATIVE_TLS_INITIAL_EXEC requires Linux x86-64 or AArch64 with GCC or Clang")
    endif()
    include(CheckCCompilerFlag)
    check_c_compiler_flag("-ftls-model=initial-exec" M8_NATIVE_HAS_TLS_INITIAL_EXEC)
    if(NOT M8_NATIVE_HAS_TLS_INITIAL_EXEC)
        message(FATAL_ERROR "M8_NATIVE_TLS_INITIAL_EXEC requires -ftls-model=initial-exec support")
    endif()
    # These libraries retain per-thread state and the existing exported ABI.
    # Initial-exec access requires static TLS space when a library is dlopened.
    # Keep this setting private to the selected playback targets.
    foreach(tls_target IN ITEMS m8aot m8native-step m8native-regions m8rt)
        if(TARGET ${tls_target})
            target_compile_options(${tls_target} PRIVATE -ftls-model=initial-exec)
            target_link_options(${tls_target} PRIVATE -ftls-model=initial-exec)
        endif()
    endforeach()
endif()

if(M8_COMBINED_RUNTIME)
    # Keep all public Unicorn and AOT exports, including optional diagnostic
    # exports found through dlsym. Whole archives also retain every PGO profile
    # constructor during training. The combined library owns one engine copy.
    set_property(TARGET m8rt PROPERTY LINK_LIBRARIES
        "-Wl,--whole-archive;unicorn_static;arm-softmmu;unicorn-common;m8aot;-Wl,--no-whole-archive;m;atomic;Threads::Threads")
    target_include_directories(m8rt PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/third_party/unicorn/include")
    # Calls within this explicitly selected runtime do not support ELF symbol
    # interposition. Bind them locally to remove cross-library PLT branches.
    if(M8_COMBINED_LOCAL_BINDING)
        target_link_options(m8rt PRIVATE "LINKER:-Bsymbolic-functions")
    endif()
    foreach(component IN ITEMS UNICORN AOT)
        if(M8_${component}_PGO STREQUAL "generate")
            # Static archive link options do not reach their shared consumer.
            target_link_options(m8rt PRIVATE "-fprofile-generate=${M8_PGO_DIR}")
        endif()
    endforeach()
    if(M8_UNICORN_PGO STREQUAL "generate")
        # The unused standalone engine still links profiled helper archives.
        target_link_options(unicorn PRIVATE "-fprofile-generate=${M8_PGO_DIR}")
    endif()
    if(M8_COMBINED_TEXT_ORDER_FILE)
        if(NOT EXISTS "${M8_COMBINED_TEXT_ORDER_FILE}")
            message(FATAL_ERROR "M8_COMBINED_TEXT_ORDER_FILE does not exist: ${M8_COMBINED_TEXT_ORDER_FILE}")
        endif()
        foreach(target IN ITEMS m8rt m8aot unicorn_static arm-softmmu unicorn-common)
            target_compile_options(${target} PRIVATE -ffunction-sections -freorder-functions)
        endforeach()
        target_link_options(m8rt PRIVATE "LINKER:-T,${M8_COMBINED_TEXT_ORDER_FILE}")
        set_property(TARGET m8rt APPEND PROPERTY LINK_DEPENDS "${M8_COMBINED_TEXT_ORDER_FILE}")
    elseif(M8_AOT_ORDER_FILE)
        # m8aot is an archive in this build. Its ordering link options must
        # reach the shared consumer where the ELF text layout is selected.
        target_link_options(m8rt PRIVATE "LINKER:-T,${M8_AOT_ORDER_FILE}")
        set_property(TARGET m8rt APPEND PROPERTY LINK_DEPENDS "${M8_AOT_ORDER_FILE}")
    elseif(M8_UNICORN_ORDER_FILE)
        # Archive link options do not reach the final shared runtime link.
        target_link_options(m8rt PRIVATE "LINKER:-T,${M8_UNICORN_ORDER_FILE}")
        set_property(TARGET m8rt APPEND PROPERTY LINK_DEPENDS "${M8_UNICORN_ORDER_FILE}")
    endif()
endif()

if(M8_BUILD_BENCH AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/m8-bench.c")
    add_executable(m8-bench benchmark/m8-bench.c core/platform/pc-profile.c core/platform/host-pmu.c benchmark/playback-contract.c)
    target_link_libraries(m8-bench PRIVATE m8rt ${CMAKE_DL_LIBS} m)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(m8-bench PRIVATE rt)
    endif()
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(m8-bench PRIVATE -Wall -Wextra)
    endif()
    set_target_properties(m8-bench PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        BUILD_RPATH_USE_ORIGIN ON
        INSTALL_RPATH "${M8_RPATH_ORIGIN}/../m8rt")
    if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmark")
        add_executable(m8-playback-contract-test benchmark/test_playback_contract.c benchmark/playback-contract.c)
        target_link_libraries(m8-playback-contract-test PRIVATE m)
        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(m8-playback-contract-test PRIVATE -UNDEBUG -Wall -Wextra -Werror)
        endif()
        add_test(NAME bench_playback_contract COMMAND m8-playback-contract-test)
        set_tests_properties(bench_playback_contract PROPERTIES TIMEOUT 15)
    endif()
    if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmark" AND CMAKE_SYSTEM_NAME STREQUAL "Linux"
            AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|aarch64|arm64|ARM64)$")
        add_executable(m8-host-pmu-test core/platform/host-pmu.c)
        target_compile_definitions(m8-host-pmu-test PRIVATE M8_HOST_PMU_TEST)
        add_executable(m8-pc-profile-test core/platform/pc-profile.c)
        target_compile_definitions(m8-pc-profile-test PRIVATE M8_PROFILE_SELF_TEST)
        target_link_libraries(m8-pc-profile-test PRIVATE ${CMAKE_DL_LIBS} rt)
        foreach(profile_test IN ITEMS m8-host-pmu-test m8-pc-profile-test)
            if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
                target_compile_options(${profile_test} PRIVATE -UNDEBUG -Wall -Wextra -Werror)
            endif()
        endforeach()
        add_test(NAME bench_pmu_ring COMMAND m8-host-pmu-test)
        set_tests_properties(bench_pmu_ring PROPERTIES TIMEOUT 15)
        find_package(Python3 QUIET COMPONENTS Interpreter)
        if(Python3_FOUND AND NOT CMAKE_CROSSCOMPILING)
            foreach(contract materialize_corpus run_corpus curate_corpus run_corpus_pi)
                add_test(NAME bench_${contract} COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/test_${contract}.py")
                set_tests_properties(bench_${contract} PROPERTIES TIMEOUT 30)
            endforeach()
            add_test(NAME bench_device_attribution COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/test_codegen_device_attribution.py")
            set_tests_properties(bench_device_attribution PROPERTIES TIMEOUT 30)
            add_test(NAME bench_device_fast_runner COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/test_device_fast_run.py")
            set_tests_properties(bench_device_fast_runner PROPERTIES TIMEOUT 30)
            add_test(NAME bench_arm_host_attribution COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/test_arm_host_attribution.py")
            set_tests_properties(bench_arm_host_attribution PROPERTIES TIMEOUT 30)
            add_test(NAME bench_pc_classifier COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/test_pc_profile.py"
                "$<TARGET_FILE:m8-pc-profile-test>")
            set_tests_properties(bench_pc_classifier PROPERTIES TIMEOUT 15)
            if(BUILD_SHARED_LIBS)
                add_test(NAME bench_contract_cli COMMAND "${CMAKE_COMMAND}" -E env
                    "M8_BENCH_BINARY=$<TARGET_FILE:m8-bench>" "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/test_contract_cli.py")
                set_tests_properties(bench_contract_cli PROPERTIES TIMEOUT 20)
            endif()
        endif()
    endif()
endif()

set(M8_DESKTOP_AVAILABLE OFF CACHE INTERNAL "Native desktop target is configured" FORCE)
if(M8_BUILD_DESKTOP AND TARGET SDL3::SDL3)
    set(M8_DESKTOP_AVAILABLE ON CACHE INTERNAL "Native desktop target is configured" FORCE)
    add_subdirectory(hosts/sdl)
elseif(M8_BUILD_DESKTOP)
    message(STATUS "SDL3 was not found; skip m8emu. Set M8_SDL_PREFIX or SDL3_DIR to build it.")
else()
    message(STATUS "Native desktop application disabled (M8_BUILD_DESKTOP=OFF)")
endif()

include(CTest)
if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmark")
    if(NOT CMAKE_CROSSCOMPILING)
        find_package(Python3 QUIET COMPONENTS Interpreter)
        if(Python3_FOUND)
            add_test(NAME qualified_arm_profile_contract COMMAND "${Python3_EXECUTABLE}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_qualified_profile.py")
            add_test(NAME performance_options_matrix_contract COMMAND "${Python3_EXECUTABLE}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_performance_options_matrix.py")
            set_tests_properties(qualified_arm_profile_contract performance_options_matrix_contract
                PROPERTIES TIMEOUT 60)
            add_test(NAME native_boot_compose_contract COMMAND "${Python3_EXECUTABLE}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/test_native_boot_compose.py")
            add_test(NAME native_app_session_contract COMMAND "${Python3_EXECUTABLE}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_native_app_session.py")
            add_test(NAME native_app_uc_counter_contract COMMAND "${Python3_EXECUTABLE}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_app_uc_counter.py")
            set_tests_properties(native_boot_compose_contract native_app_session_contract
                native_app_uc_counter_contract PROPERTIES TIMEOUT 60)
            set_tests_properties(native_app_uc_counter_contract PROPERTIES
                ENVIRONMENT "M8_UC_COUNTER_ENGINE=$<TARGET_FILE:unicorn>")
        endif()
    endif()
    if(M8_GUEST_PC_PROFILE)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        add_test(NAME guest_pc_aot_tool COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/test_guest_pc_aot.py")
        add_test(NAME guest_work_tool COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/test_guest_work.py")
        set_tests_properties(guest_pc_aot_tool guest_work_tool PROPERTIES TIMEOUT 30)
    endif()
    add_executable(m8rt-core-check ${CMAKE_CURRENT_SOURCE_DIR}/tests/machine/test_core.c)
    target_link_libraries(m8rt-core-check PRIVATE m8rt ${m8_test_engine} m)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(m8rt-core-check PRIVATE -UNDEBUG)
    endif()
    add_test(NAME runtime_core COMMAND m8rt-core-check)
    set_tests_properties(runtime_core PROPERTIES TIMEOUT 30)
    add_executable(m8rt-usb-service-check
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/machine/usb_service_test.c ${M8RT_SOURCE_DIR}/usb.c)
    target_include_directories(m8rt-usb-service-check PRIVATE
        ${M8RT_INCLUDE_DIRS} "${M8RT_SOURCE_DIR}/third_party")
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(m8rt-usb-service-check PRIVATE -UNDEBUG -Wall -Wextra)
    endif()
    add_test(NAME runtime_usb_services COMMAND m8rt-usb-service-check)
    set_tests_properties(runtime_usb_services PROPERTIES TIMEOUT 30)
    add_executable(m8rt-nor-check ${CMAKE_CURRENT_SOURCE_DIR}/tests/machine/nor_test.c)
    target_link_libraries(m8rt-nor-check PRIVATE m8rt ${m8_test_engine} m)
    add_test(NAME runtime_nor COMMAND m8rt-nor-check)
    set_tests_properties(runtime_nor PROPERTIES TIMEOUT 30)
    add_executable(m8rt-watchdog-check ${CMAKE_CURRENT_SOURCE_DIR}/tests/machine/watchdog_test.c)
    target_link_libraries(m8rt-watchdog-check PRIVATE m8rt ${m8_test_engine} m)
    add_test(NAME runtime_watchdog COMMAND m8rt-watchdog-check)
    set_tests_properties(runtime_watchdog PROPERTIES TIMEOUT 30)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/test-unicorn-dispatch.c")
    add_executable(m8-unicorn-dispatch-check benchmark/test-unicorn-dispatch.c)
    target_link_libraries(m8-unicorn-dispatch-check PRIVATE ${m8_test_engine} Threads::Threads)
    add_test(NAME unicorn_dispatch COMMAND m8-unicorn-dispatch-check)
    set_tests_properties(unicorn_dispatch PROPERTIES TIMEOUT 10)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_native_fp.c")
        add_executable(m8aot-fp-check tests/aot/test_native_fp.c)
        target_include_directories(m8aot-fp-check PRIVATE "${M8_AOT_DIR}")
        target_link_libraries(m8aot-fp-check PRIVATE m)
        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(m8aot-fp-check PRIVATE -frounding-math
                -ffp-contract=off -fno-math-errno)
        endif()
        add_test(NAME aot_native_fp COMMAND m8aot-fp-check)
        set_tests_properties(aot_native_fp PROPERTIES TIMEOUT 30)
    endif()
endif()

if(TARGET m8emu AND M8_BUILD_AOT AND M8_NATIVE_ENGINE AND M8_NATIVE_REGIONS)
    m8_pgo_finalize(TARGET m8emu)
endif()
if(M8_PORTABLE_BUNDLE)
    foreach(m8_bundle_target IN ITEMS m8rt unicorn m8aot m8native-regions m8native-step m8native-boot)
        if(TARGET ${m8_bundle_target})
            set_target_properties(${m8_bundle_target} PROPERTIES BUILD_WITH_INSTALL_RPATH ON INSTALL_RPATH "${M8_RPATH_ORIGIN}")
        endif()
    endforeach()
endif()
