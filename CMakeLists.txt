cmake_minimum_required(VERSION 3.20)
project(m8emu VERSION 0.4.0 LANGUAGES C)
include(cmake/M8QualifiedProfile.cmake)
include(cmake/M8RetiredOptions.cmake)
include(cmake/M8Firmware.cmake)
m8emu_prepare_firmware()
# Product builds never construct the qualification CPU. Lab tools opt in.
option(M8_LAB_UNICORN "Build the lab-only Unicorn qualification reference" OFF)
if(M8_LAB_UNICORN AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/m8-bench.c")
    message(FATAL_ERROR "M8_LAB_UNICORN is available only in the lab source tree")
endif()
if(DEFINED CACHE{M8_WITH_UNICORN})
    if(M8_WITH_UNICORN AND NOT M8_LAB_UNICORN)
        message(FATAL_ERROR "M8_WITH_UNICORN is retired; lab reference builds require M8_LAB_UNICORN=ON")
    endif()
    unset(M8_WITH_UNICORN CACHE)
endif()
# Internal backend predicate, never a product configure switch.
set(M8_WITH_UNICORN ${M8_LAB_UNICORN})
set(m8_native_engine_default ON)
if(M8_WITH_UNICORN)
    set(m8_native_engine_default OFF)
endif()
option(M8_NATIVE_ENGINE "Build the native execution backend" ${m8_native_engine_default})
if(NOT M8_WITH_UNICORN AND NOT M8_NATIVE_ENGINE)
    message(FATAL_ERROR "Unicorn-free execution requires M8_NATIVE_ENGINE=ON")
endif()
include(cmake/M8Python.cmake)
if(M8_BUILD_AOT OR M8_NATIVE_ENGINE OR M8_PGO STREQUAL "AUTO")
    m8emu_prepare_python()
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/M8BuildPGO.cmake")
    include(cmake/M8BuildPGO.cmake)
    m8_pgo_orchestrate(m8_outer_pgo)
    if(m8_outer_pgo)
        return()
    endif()
else()
    set(M8_PGO OFF CACHE STRING "Compiler PGO")
    set(M8_PGO_PHASE NONE)
    if(NOT M8_PGO STREQUAL "OFF")
        message(FATAL_ERROR "This source export requires M8_PGO=OFF")
    endif()
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
option(BUILD_SHARED_LIBS "Build shared runtime libraries" ON)
option(M8_BUILD_WEB "Build the static browser application" OFF)
option(M8_WEB_COMPACT_POOL "Use checked integer pool entries in the compact web AOT" OFF)
option(M8_WEB_GUEST_REGISTER_ABI "Use scalar guest parameters and multi-value returns in WASI AOT" OFF)
if(M8_WEB_GUEST_REGISTER_ABI AND (NOT M8_BUILD_WEB OR NOT CMAKE_SYSTEM_NAME STREQUAL "WASI"))
    message(FATAL_ERROR "M8_WEB_GUEST_REGISTER_ABI requires the WASI web build")
endif()
if(M8_BUILD_WEB)
    # Declare before the AOT splitter selects its generated profile.
    option(M8_WEB_COMPACT_TABLE "Use checked compact lookup and proved span caches" ON)
    # Compile and link share one pool: parallel Binaryen/LLVM jobs can exceed
    # desktop RAM even when each generated source unit is small.
    set(M8_WEB_BUILD_JOBS 1 CACHE STRING "Maximum simultaneous web compile/link commands")
    set(M8_WEB_MEMORY_MIB 7680 CACHE STRING "Memory bound per web compiler command")
    foreach(value IN ITEMS M8_WEB_BUILD_JOBS M8_WEB_MEMORY_MIB)
        if(NOT ${value} MATCHES "^[1-9][0-9]*$")
            message(FATAL_ERROR "${value} must be a positive integer")
        endif()
    endforeach()
    set_property(GLOBAL APPEND PROPERTY JOB_POOLS m8_web_memory=${M8_WEB_BUILD_JOBS})
    set(CMAKE_JOB_POOL_COMPILE m8_web_memory)
    set(CMAKE_JOB_POOL_LINK m8_web_memory)
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        set(m8_web_budget "\"${Python3_EXECUTABLE}\" \"${PROJECT_SOURCE_DIR}/hosts/web/budget.py\" --memory-mib ${M8_WEB_MEMORY_MIB} --report \"${CMAKE_BINARY_DIR}/web-memory.log\" --")
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${m8_web_budget}")
        set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${m8_web_budget}")
    endif()
endif()
set(m8_compact_default OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "WASI")
    set(m8_compact_default ON)
endif()
option(M8_COMPACT_MEMORY "Use checked compact guest backing instead of a sparse arena" ${m8_compact_default})
if(M8_COMPACT_MEMORY)
    add_compile_definitions(M8_AOT_COMPACT_MEMORY M8RT_COMPACT_MEMORY_DEFAULT=1)
endif()
set(m8_split_default 0)
if(M8_COMPACT_MEMORY)
    set(m8_split_default 24)
endif()
set(M8_AOT_SPLIT_UNITS ${m8_split_default} CACHE STRING "Split AOT into this many hot/cold units (0 disables)")
if(NOT M8_AOT_SPLIT_UNITS MATCHES "^[0-9]+$" OR M8_AOT_SPLIT_UNITS GREATER 64)
    message(FATAL_ERROR "M8_AOT_SPLIT_UNITS must be 0..64")
endif()
if(M8_WEB_GUEST_REGISTER_ABI AND M8_AOT_SPLIT_UNITS EQUAL 0)
    message(FATAL_ERROR "M8_WEB_GUEST_REGISTER_ABI requires M8_AOT_SPLIT_UNITS greater than zero")
endif()
include(cmake/M8AOTSplit.cmake)
set(M8_GENERATED_LIBRARY_KIND SHARED)
if(CMAKE_SYSTEM_NAME STREQUAL "WASI")
    set(M8_GENERATED_LIBRARY_KIND STATIC)
endif()
option(M8_NATIVE_INTERPRETER "Use live-byte native fallback interpretation" OFF)
option(M8_BUILD_BENCH "Build the headless benchmark when available" ON)
set(M8_INTERPRETER_PROFILE_DEFAULT OFF)
if(M8_BUILD_BENCH AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/m8-bench.c")
    set(M8_INTERPRETER_PROFILE_DEFAULT ON)
endif()
option(M8_NATIVE_INTERPRETER_PROFILE_DEFAULT "Collect portable interpreter PCs in lab builds" ${M8_INTERPRETER_PROFILE_DEFAULT})
option(M8_NATIVE_STATIC_COVERAGE "Build conservative full-firmware cold coverage" OFF)
option(M8_WEB_STATIC_COVERAGE "Build scoped static cold companion for the web module" OFF)
option(M8_BUILD_AOT "Build the original-firmware DSP AOT library" OFF)
option(M8_AOT_REAL_CALLBACKS "Include real-project output shapers when generating AOT" OFF)
option(M8_AOT_CODEGEN_V2 "Use the default-off structural AOT source copy" OFF)
option(M8_WEB_CODEGEN_V2 "Use checked shared-register Codegen V2 on compact web memory" OFF)
if(M8_WEB_CODEGEN_V2)
    # Keep the single opt-in active when the release gate reapplies preset web.
    set(M8_AOT_CODEGEN_V2 ON CACHE BOOL "Use the default-off structural AOT source copy" FORCE)
    if(NOT M8_BUILD_WEB OR NOT M8_COMPACT_MEMORY)
        message(FATAL_ERROR "M8_WEB_CODEGEN_V2 requires web and compact memory")
    endif()
endif()
option(M8_BUILD_AOT_COUNTERS "Build a separate AOT instruction-counting library" ON)
option(M8RT_DIRECT_MEMORY "Use guarded owned-buffer memory copies by default" ON)
option(M8_RT_CALLBACK_TIMING "Collect host callback duration statistics" ON)
option(M8_NATIVE_TLS_INITIAL_EXEC "Use static TLS access in native playback libraries" OFF)
option(M8_NATIVE_DEVICE_FAST "Use qualified native device callbacks" OFF)
# The NVIC/plain fast paths are plain C with no codegen-V2, fixed-memory or
# compact dependency (see docs/native-mmio-fast.md): they only need the native
# engine's device_direct/service_continue state, which the web build owns too.
# The web build has an independent switch that defaults ON there.
set(M8_WEB_DEVICE_FAST_DEFAULT OFF)
if(M8_BUILD_WEB)
    set(M8_WEB_DEVICE_FAST_DEFAULT ON)
endif()
option(M8_WEB_DEVICE_FAST "Use qualified native device callbacks in the web build (independent of codegen V2)" ${M8_WEB_DEVICE_FAST_DEFAULT})
option(M8_NATIVE_BOOT "Build the checked native cold-boot library" OFF)
option(M8_NATIVE_BOOT_FIFO_CONTINUE_DEFAULT "Continue proven interior boot FIFO reads by default" OFF)
option(M8_NATIVE_REGIONS "Compile the measured playback tail into native regions" OFF)
option(M8_NATIVE_PGO_SCOPED "Use the retained bounded native instrumentation scope" OFF)
option(M8_PORTABLE_BUNDLE "Use portable relative runtime paths" OFF)
option(M8_NATIVE_CORPUS_CATALOG "Use the default-off real-project native coverage catalog" OFF)
option(M8_NATIVE_AOT_CODE_GUARD "Guard code writes in a candidate AOT copy" OFF)
option(M8_NATIVE_REGION_INTEGER_FP_FASTPATH "Skip FP environment setup in integer-only native pages" OFF)
option(M8_NATIVE_REGION_SERVICE_CONTINUE "Continue native regions after qualified device callbacks" OFF)
option(M8_NATIVE_REGION_INLINE_MEMORY "Inline fixed-width native region memory accesses" OFF)
option(M8_NATIVE_REGION_MEMORY_SPANS "Use direct host pointers for proven native region memory spans" OFF)
option(M8_NATIVE_REGION_CHAIN "Chain qualified native region exits directly" OFF)
option(M8_NATIVE_REGION_COMPUTED_REDIRECTS "Continue measured computed transfers inside native groups" OFF)
option(M8_NATIVE_REGION_INLINE_SERVICES "Compile exact barriers and RAM exclusives in native regions" OFF)
option(M8_NATIVE_REGION_RAM_EXCLUSIVES "Compile RAM word exclusives without inline barriers or prefetch" OFF)
option(M8_NATIVE_REGION_DIRECT_CALLS "Inline hot flag/FP helper calls in native regions" OFF)
option(M8_NATIVE_REGION_LOCAL_NZCV "Keep region condition flags in locals, packed only at state boundaries" OFF)
option(M8_NATIVE_REGION_DEAD_NZCV "Omit provably dead whole NZCV writes in native regions" OFF)
option(M8_NATIVE_REGION_EVENT_LOOP "Run exact scheduling and IRQ boundaries in native region chains" OFF)
option(M8_NATIVE_FP_NO_STICKY "Remove host FP environment switching in native builds (result bits only, no cumulative flags)" OFF)
option(M8_NATIVE_REGION_SHARED_STATE "Keep region registers in the caller CPU" OFF)
option(M8_NATIVE_ENTRY_CLONES "Clone hot secondary entries in the native AOT build (GCC and Clang)" OFF)
set(m8_rt_local_calls_default ON)
if(APPLE)
    set(m8_rt_local_calls_default OFF)
endif()
option(M8_RT_LOCAL_CALLS "Bind private ELF runtime helper calls locally" ${m8_rt_local_calls_default})
set(M8_LOADER_ORIGIN "$ORIGIN")
if(APPLE)
    set(M8_LOADER_ORIGIN "@loader_path")
endif()
include(cmake/M8Tuning.cmake)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/M8Bolt.cmake")
    include(cmake/M8Bolt.cmake)
else()
    set(M8_BOLT OFF CACHE STRING "Post-link optimization")
    if(NOT M8_BOLT STREQUAL "OFF")
        message(FATAL_ERROR "This source export requires M8_BOLT=OFF")
    endif()
endif()
include(CTest)
if((M8_NATIVE_BOOT OR M8_NATIVE_REGIONS OR M8_NATIVE_AOT_CODE_GUARD OR M8_NATIVE_CORPUS_CATALOG) AND NOT M8_NATIVE_ENGINE)
    message(FATAL_ERROR "Native boot, region and code-guard candidates require M8_NATIVE_ENGINE=ON")
endif()
if(M8_NATIVE_INTERPRETER AND NOT M8_NATIVE_ENGINE)
    message(FATAL_ERROR "M8_NATIVE_INTERPRETER requires M8_NATIVE_ENGINE=ON")
endif()
if(M8_AOT_CODEGEN_V2 AND
        (NOT M8_BUILD_AOT OR NOT M8_NATIVE_ENGINE OR NOT M8_NATIVE_AOT_CODE_GUARD))
    message(FATAL_ERROR "M8_AOT_CODEGEN_V2 requires AOT, native engine and code guards")
endif()
if(M8_WEB_COMPACT_POOL)
    foreach(required IN ITEMS M8_BUILD_WEB M8_BUILD_AOT M8_NATIVE_AOT_CODE_GUARD)
        if(NOT ${required})
            message(FATAL_ERROR "M8_WEB_COMPACT_POOL first_false=${required} expected=ON actual=${${required}}")
        endif()
    endforeach()
    if(NOT M8_COMPACT_MEMORY)
        message(FATAL_ERROR "M8_WEB_COMPACT_POOL requires compact memory")
    endif()
endif()
if(M8_NATIVE_REGION_EVENT_LOOP AND NOT M8_NATIVE_REGION_CHAIN)
    message(FATAL_ERROR "M8_NATIVE_REGION_EVENT_LOOP requires M8_NATIVE_REGION_CHAIN=ON")
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
if(NOT CMAKE_SYSTEM_NAME STREQUAL "WASI")
    find_package(SDL3 QUIET CONFIG HINTS "${M8_SDL_PREFIX}/lib/cmake/SDL3")
endif()
option(M8_BUILD_DESKTOP "Build the SDL3 native desktop application" ${SDL3_FOUND})
if(M8_BUILD_DESKTOP AND NOT SDL3_FOUND)
    include(cmake/M8SDL.cmake)
    m8emu_prepare_sdl()
endif()


if(NOT M8_WITH_UNICORN)
    set(M8_NATIVE_BOOT ON)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "WASI")
        set(M8_NATIVE_STATIC_COVERAGE ON)
    endif()
    if(M8_BUILD_WEB AND M8_WEB_STATIC_COVERAGE)
        set(M8_NATIVE_STATIC_COVERAGE ON)
        set(M8_NATIVE_STATIC_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/aot/catalogs/static-web.json"
            CACHE FILEPATH "Scoped static firmware coverage inputs for web")
        set(M8_NATIVE_STATIC_PC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/hosts/web/static-roots.txt"
            CACHE STRING "Extra measured PC roots for scoped static coverage")
        set(M8_NATIVE_STATIC_REGIONS_ONLY ON CACHE BOOL "Web ships bulk regions without the duplicate decoder")
    endif()
    set(M8_NATIVE_INTERPRETER ON)
endif()
if(M8_LAB_UNICORN)
    include(cmake/lab/Unicorn.cmake)
endif()

# The public core uses one runtime ABI with separate implementation areas.
set(M8RT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/core/machine")
set(M8RT_ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/core/engine")
set(M8RT_STORAGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/core/storage")
set(M8RT_PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/core/platform")
set(M8RT_STALL_SOURCE "${M8RT_PLATFORM_DIR}/stall.c")
if(M8_BUILD_WEB)
    set(M8RT_STALL_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/hosts/web/stall.c")
endif()
set(M8RT_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/core/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/include/m8emu"
    "${M8RT_SOURCE_DIR}" "${M8RT_ENGINE_DIR}" "${M8RT_STORAGE_DIR}"
    "${M8RT_PLATFORM_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/aot")
add_library(m8rt
    ${M8RT_SOURCE_DIR}/session.c
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
    ${M8RT_ENGINE_DIR}/native-profile-runtime.c
    ${M8RT_STALL_SOURCE})
add_library(m8::runtime ALIAS m8rt)
if(M8_NATIVE_FP_NO_STICKY)
    target_compile_definitions(m8rt PRIVATE M8_NATIVE_FP_NO_STICKY)
endif()
target_compile_definitions(m8rt PRIVATE M8RT_DIRECT_MEMORY_DEFAULT=$<BOOL:${M8RT_DIRECT_MEMORY}>)
target_compile_definitions(m8rt PRIVATE M8RT_CALLBACK_TIMING_DEFAULT=$<BOOL:${M8_RT_CALLBACK_TIMING}>)
target_compile_definitions(m8rt PRIVATE M8RT_INTERPRETER_PROFILE_DEFAULT=$<BOOL:${M8_NATIVE_INTERPRETER_PROFILE_DEFAULT}>)
target_compile_definitions(m8rt PRIVATE M8RT_NATIVE_BOOT_FIFO_CONTINUE_DEFAULT=$<BOOL:${M8_NATIVE_BOOT_FIFO_CONTINUE_DEFAULT}>)
if(M8_NATIVE_DEVICE_FAST OR M8_WEB_DEVICE_FAST)
    target_compile_definitions(m8rt PRIVATE M8RT_NATIVE_DEVICE_FAST=1)
endif()
target_include_directories(m8rt PUBLIC ${M8RT_INCLUDE_DIRS})
if(M8_WITH_UNICORN)
    target_include_directories(m8rt BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/unicorn/include")
else()
    target_compile_definitions(m8rt PUBLIC M8RT_PURE_NATIVE=1)
    target_sources(m8rt PRIVATE ${M8RT_ENGINE_DIR}/cpu-backend.c)
endif()
if(M8_NATIVE_INTERPRETER)
    target_compile_definitions(m8rt PRIVATE M8RT_INTERPRETER=1)
    target_sources(m8rt PRIVATE ${M8RT_ENGINE_DIR}/thumb-interpreter.c
        ${M8RT_ENGINE_DIR}/native-fallback.c ${M8RT_ENGINE_DIR}/thumb-services.c
        ${M8RT_ENGINE_DIR}/thumb-code-cache.c
        ${M8RT_ENGINE_DIR}/native-interpreter-profile.c)
    set_source_files_properties(${M8RT_ENGINE_DIR}/thumb-interpreter.c PROPERTIES
        COMPILE_OPTIONS "-fno-fast-math;-frounding-math;-ffp-contract=off")
endif()
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
        add_library(m8native-step ${M8_GENERATED_LIBRARY_KIND} "${M8_NATIVE_STEP_SOURCE}")
        target_include_directories(m8native-step PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/aot")
        if(M8_NATIVE_CORPUS_CATALOG)
            add_dependencies(m8native-step m8-native-corpus-generate)
            target_include_directories(m8native-step BEFORE PRIVATE "${m8_native_corpus_dir}/headers")
        endif()
        target_compile_definitions(m8native-step PRIVATE M8_AOT_NATIVE_FP)
        if(M8_NATIVE_FP_NO_STICKY)
            target_compile_definitions(m8native-step PRIVATE M8_NATIVE_FP_NO_STICKY)
        endif()
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
        if(M8_NATIVE_REGION_MEMORY_SPANS)
            list(APPEND m8_native_region_generator_options --region-memory-spans)
        endif()
        if(M8_NATIVE_REGION_RAM_EXCLUSIVES)
            list(APPEND m8_native_region_generator_options --ram-exclusives)
        endif()
        if(M8_NATIVE_REGION_DIRECT_CALLS)
            list(APPEND m8_native_region_generator_options --direct-calls)
        endif()
        if(M8_NATIVE_REGION_LOCAL_NZCV)
            list(APPEND m8_native_region_generator_options --local-nzcv)
        endif()
        if(M8_NATIVE_REGION_DEAD_NZCV)
            list(APPEND m8_native_region_generator_options --dead-nzcv)
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
            CONTENT "chain=${M8_NATIVE_REGION_CHAIN}\nfragments=${M8_NATIVE_REGION_PC_FRAGMENTS}\nlayout=${M8_NATIVE_REGION_LAYOUT}\ncomputed=${M8_NATIVE_REGION_COMPUTED_REDIRECTS}\ninline_services=${M8_NATIVE_REGION_INLINE_SERVICES}\nram_exclusives=${M8_NATIVE_REGION_RAM_EXCLUSIVES}\ndirect_calls=${M8_NATIVE_REGION_DIRECT_CALLS}\nlocal_nzcv=${M8_NATIVE_REGION_LOCAL_NZCV}\ndead_nzcv=${M8_NATIVE_REGION_DEAD_NZCV}\nmemory_spans=${M8_NATIVE_REGION_MEMORY_SPANS}\nevent_loop=${M8_NATIVE_REGION_EVENT_LOOP}\nshared_state=${M8_NATIVE_REGION_SHARED_STATE}\n")
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
                "${m8_native_region_dir}/region_memory_spans.py"
                "${m8_native_region_dir}/native_region_services.py"
                "${m8_native_region_dir}/native_region_services.h"
                "${m8_native_region_dir}/native_region_layout.py"
                "${m8_native_region_dir}/native_region_chain.py"
                "${m8_native_region_dir}/native_region_chain.h"
                "${m8_native_region_dir}/native_region_event_chain.h"
                "${m8_native_region_dir}/native_region_state.py"
                "${m8_native_region_dir}/codegen_v2.py"
                "${m8_native_region_dir}/local_flags.py"
                "${m8_native_region_dir}/local_flags.h"
                "${m8_native_region_dir}/nzcv.py"
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
        add_library(m8native-regions ${M8_GENERATED_LIBRARY_KIND} "${M8_NATIVE_REGION_SOURCE}")
        if(M8_NATIVE_CORPUS_CATALOG)
            add_dependencies(m8native-regions m8-native-corpus-generate)
            target_sources(m8native-regions PRIVATE "${m8_native_corpus_dir}/hot.c")
            if(NOT M8_BUILD_WEB)
                target_sources(m8native-regions PRIVATE "${m8_native_corpus_dir}/cold.c")
            endif()
            target_include_directories(m8native-regions BEFORE PRIVATE "${m8_native_corpus_dir}/headers")
        endif()
        target_include_directories(m8native-regions PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/aot")
        target_compile_definitions(m8native-regions PRIVATE M8_AOT_NATIVE_FP)
        if(M8_NATIVE_FP_NO_STICKY)
            target_compile_definitions(m8native-regions PRIVATE M8_NATIVE_FP_NO_STICKY)
        endif()
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
            M8_NATIVE_REGION_MEMORY_SPANS=$<BOOL:${M8_NATIVE_REGION_MEMORY_SPANS}>
            M8_NATIVE_REGION_INLINE_MEMORY=$<BOOL:${M8_NATIVE_REGION_INLINE_MEMORY}>)
        target_compile_options(m8native-regions PRIVATE -O2 -frounding-math -ffp-contract=off -fno-math-errno
            -Wall -Wextra -Wno-unused-label -Wno-unused-variable -Wno-unused-but-set-variable)
        target_link_libraries(m8native-regions PRIVATE m)
        set_target_properties(m8native-regions PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/aot")
    endif()
    if(BUILD_TESTING AND M8_WITH_UNICORN AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmark" AND NOT CMAKE_CROSSCOMPILING)
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
        foreach(control quiet default)
            add_test(NAME native_boot_fifo_${control} COMMAND "${M8_NATIVE_PYTHON}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/machine/test_native_boot_fifo_${control}.py")
            set_tests_properties(native_boot_fifo_${control} PROPERTIES TIMEOUT 180
                ENVIRONMENT "LIBUNICORN_PATH=$<TARGET_FILE_DIR:unicorn>")
        endforeach()
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
                    ENVIRONMENT "LIBUNICORN_PATH=$<TARGET_FILE_DIR:unicorn>;PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/tests/aot")
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
            add_test(NAME native_region_direct_call_contract COMMAND "${M8_NATIVE_PYTHON}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_native_region_direct_calls.py")
            set_tests_properties(native_region_direct_call_contract PROPERTIES TIMEOUT 60)
        endif()
        if(M8_NATIVE_AOT_CODE_GUARD)
            add_test(NAME native_aot_guard_contract COMMAND "${M8_NATIVE_PYTHON}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_native_aot_guard.py")
            set_tests_properties(native_aot_guard_contract PROPERTIES TIMEOUT 120
                ENVIRONMENT "LIBUNICORN_PATH=$<TARGET_FILE_DIR:unicorn>")
            add_test(NAME native_aot_guard_outputs COMMAND "${M8_NATIVE_PYTHON}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_native_aot_guard_outputs.py")
            set_tests_properties(native_aot_guard_outputs PROPERTIES TIMEOUT 30)
            add_test(NAME native_voice_copy_contract COMMAND "${M8_NATIVE_PYTHON}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_voice_copy.py")
            set_tests_properties(native_voice_copy_contract PROPERTIES TIMEOUT 30)
        endif()
    endif()
endif()
if(M8_RT_LOCAL_CALLS)
    target_compile_definitions(m8rt PRIVATE M8RT_LOCAL_CALLS)
endif()
if(M8_NATIVE_STATIC_COVERAGE)
    include(cmake/M8NativeStatic.cmake)
endif()
target_link_libraries(m8rt PRIVATE ${m8_runtime_engine} m)
if(NOT M8_BUILD_WEB)
    find_package(Threads REQUIRED)
    target_link_libraries(m8rt PRIVATE Threads::Threads)
endif()
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
    INSTALL_RPATH "${M8_LOADER_ORIGIN}")
add_executable(m8rt-cli ${M8RT_SOURCE_DIR}/cli.c)
target_link_libraries(m8rt-cli PRIVATE m8rt)
set_target_properties(m8rt-cli PROPERTIES
    OUTPUT_NAME m8rt RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/m8rt"
    BUILD_RPATH_USE_ORIGIN ON INSTALL_RPATH "${M8_LOADER_ORIGIN}")

if(M8_BUILD_AOT)
    set(M8_AOT_SOURCE ""
        CACHE FILEPATH "Generated AOT C source")
    set(M8_AOT_PROFILE fast CACHE STRING "AOT floating-point profile: fast or exact")
    set_property(CACHE M8_AOT_PROFILE PROPERTY STRINGS fast exact)
    set(M8_AOT_DEFINITIONS "" CACHE STRING "Additional compile definitions for the selected AOT source")
    option(M8_AOT_MEMORY_SPANS "Qualify invariant function memory spans" ON)
    option(M8_AOT_LOCAL_NZCV "Keep condition flags in local values" ON)
    option(M8_AOT_IT_PREFLIGHT "Qualify memory before IT block effects" ON)
    option(M8_AOT_FIXED_MEMORY "Require fixed-offset guest memory in the fast library" ON)
    option(M8_AOT_VALIDITY_FIRST "Check fixed scalar spans before forming host pointers" ${M8_AOT_FIXED_MEMORY})
    if(M8_COMPACT_MEMORY)
        set(M8_AOT_FIXED_MEMORY OFF)
        set(M8_AOT_VALIDITY_FIRST OFF)
        if(M8_AOT_CODEGEN_V2 AND NOT M8_WEB_CODEGEN_V2)
            message(FATAL_ERROR "Compact memory currently requires M8_AOT_CODEGEN_V2=OFF")
        endif()
    endif()
    option(M8_AOT_CALL_DISPATCH "Use tagged computed-call lookup" ON)
    option(M8_AOT_EXACT_ENTRY_LEAVES "Use exact-entry register leaves" ON)
    option(M8_AOT_REGION_SPANS "Use bounded internal memory spans" ON)
    option(M8_AOT_POINTEE_SPANS "Cache checked pointers in bounded sections" ON)
    option(M8_AOT_BRAIDS_MODE0 "Use the bounded MACROSYNTH filter quantum" ON)
    option(M8_AOT_SAMPLER_EXTENDED "Use checked stereo/PCM24/filter SAMPLER blocks" ON)
    option(M8_AOT_SAMPLER_POSITION_FP "Use bounded exact SAMPLER position arithmetic" OFF)
    option(M8_AOT_SAMPLER_SHAPE "Use checked shape-five SAMPLER methods" ON)
    option(M8_AOT_OSCILLATOR_VOICE "Use the checked whole WAVSYNTH voice" ON)
    option(M8_AOT_OSCILLATOR_SPANS "Cache checked spans in the whole WAVSYNTH voice" OFF)
    if(M8_AOT_OSCILLATOR_SPANS AND NOT M8_AOT_OSCILLATOR_VOICE)
        message(FATAL_ERROR "WAVSYNTH spans first_false=M8_AOT_OSCILLATOR_VOICE expected=ON actual=OFF")
    endif()
    if((M8_AOT_SAMPLER_POSITION_FP OR M8_AOT_SAMPLER_SHAPE) AND NOT M8_AOT_SAMPLER_EXTENDED)
        message(FATAL_ERROR "SAMPLER options first_false=M8_AOT_SAMPLER_EXTENDED expected=ON actual=OFF")
    endif()
    option(M8_AOT_ARM64_FP_HARDWARE "Use qualified ARM64 FP operations without NaN guards" OFF)
    option(M8_AOT_INT_FLOAT_SCALE "Use direct F32 fixed-point conversion scaling" ON)
    set(M8_AOT_FMA auto CACHE STRING "x86 FMA: auto, on or off")
    set_property(CACHE M8_AOT_FMA PROPERTY STRINGS auto on off)
    if(M8_AOT_PROFILE STREQUAL "fast" AND M8_AOT_VALIDITY_FIRST AND NOT M8_AOT_FIXED_MEMORY)
        message(FATAL_ERROR "M8_AOT_VALIDITY_FIRST requires M8_AOT_FIXED_MEMORY")
    endif()
    set(aot_generator_flags)
    foreach(option_suffix IN ITEMS MEMORY_SPANS LOCAL_NZCV IT_PREFLIGHT CALL_DISPATCH EXACT_ENTRY_LEAVES)
        if(M8_AOT_${option_suffix})
            string(TOLOWER "${option_suffix}" generator_flag)
            string(REPLACE "_" "-" generator_flag "${generator_flag}")
            list(APPEND aot_generator_flags "--${generator_flag}")
        endif()
    endforeach()
    set(aot_fma OFF)
    string(REGEX MATCH "(^|;)(M8_AOT_DIAGNOSTIC|M8_AOT_GUEST_COUNT)(=[^;]*)?(;|$)"
        aot_instrumented "${M8_AOT_DEFINITIONS}")
    if(NOT M8_AOT_FMA MATCHES "^(auto|on|off)$")
        message(FATAL_ERROR "M8_AOT_FMA must be auto, on or off")
    endif()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$" AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        if(M8_AOT_FMA STREQUAL "on")
            set(aot_fma ON)
        elseif(M8_AOT_FMA STREQUAL "auto" AND NOT CMAKE_CROSSCOMPILING AND NOT M8_BUILD_WEB)
            include(CheckCSourceRuns)
            check_c_source_runs("int main(void) { __builtin_cpu_init(); return __builtin_cpu_supports(\"fma\") ? 0 : 1; }" M8_HOST_HAS_FMA)
            set(aot_fma ${M8_HOST_HAS_FMA})
        endif()
    elseif(M8_AOT_FMA STREQUAL "on")
        message(FATAL_ERROR "M8_AOT_FMA=on requires an x86-64 GCC/Clang target")
    endif()
    # The fast FM engine requires a qualified inline FP profile for its
    # native default. Web defaults on: the engine uses the exact web FP
    # helpers, and its structural guard falls back to translated execution
    # when the host FP mode is unsuitable. Other profiles can still select
    # it explicitly for differential testing.
    set(M8_AOT_FM_FAST_DEFAULT OFF)
    if((M8_NATIVE_ENGINE AND
             ((M8_NATIVE_FP_NO_STICKY AND aot_fma) OR
              (M8_AOT_ARM64_FP_HARDWARE AND
               CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$"))) OR
            M8_BUILD_WEB)
        set(M8_AOT_FM_FAST_DEFAULT ON)
    endif()
    option(M8_AOT_FM_FAST "Use the exact FM batch engine with inline native FP" ${M8_AOT_FM_FAST_DEFAULT})
    # The HYPERSYNTH fast engine follows the FM fast engine: inline host FP
    # with the fixed FZ/DN/RN mode, state in host locals, structural checks
    # hoisted to block entry, covering the filter-1/filter-2 shapes that real
    # songs use. It defaults on exactly where the FM engine does.
    set(M8_AOT_HYPER_FAST_DEFAULT ${M8_AOT_FM_FAST_DEFAULT})
    option(M8_AOT_HYPER_FAST "Use the exact HYPERSYNTH batch engine with inline native FP" ${M8_AOT_HYPER_FAST_DEFAULT})
    # The effect fast engine follows the FM fast engine: inline host FP
    # with the fixed FZ/DN/RN mode, state in host locals, structural checks
    # hoisted to block entry, covering the bypass-mode 64-sample block that
    # all observed songs use. It defaults on exactly where the FM engine does.
    set(M8_AOT_EFFECT_FAST_DEFAULT ${M8_AOT_FM_FAST_DEFAULT})
    option(M8_AOT_EFFECT_FAST "Use the exact effect-block engine with inline native FP" ${M8_AOT_EFFECT_FAST_DEFAULT})
    option(M8_AOT_MIXER_FAST "Use the exact constant-step mixer block engine" ${M8_AOT_FM_FAST_DEFAULT})
    set(M8_AOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/aot")
    if(M8_AOT_REAL_CALLBACKS)
        list(APPEND aot_generator_flags --function-fragment
            "${M8_AOT_DIR}/functions-real-callbacks.txt")
    endif()
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
        # Standalone default-off transforms do not feed the C generator.
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
    set(m8_native_performance_source OFF)
    if(M8_NATIVE_AOT_CODE_GUARD AND M8_AOT_PROFILE STREQUAL "fast" AND M8_AOT_FIXED_MEMORY)
        set(m8_native_performance_source ON)
    endif()
    if(m8_native_performance_source)
        set(m8_performance_aot_input "${M8_AOT_SOURCE}")
        set(M8_AOT_SOURCE "${CMAKE_BINARY_DIR}/native-performance/generated.c")
        add_custom_command(OUTPUT "${M8_AOT_SOURCE}"
            BYPRODUCTS "${CMAKE_BINARY_DIR}/native-performance/manifest.json"
            COMMAND "${m8_native_guard_python}" -B "${M8_AOT_DIR}/native_performance.py"
                --input "${CMAKE_BINARY_DIR}/native-aot-guard"
                --output "${CMAKE_BINARY_DIR}/native-performance" --replace
            DEPENDS "${m8_performance_aot_input}" "${M8_AOT_DIR}/native_performance.py"
                "${M8_AOT_DIR}/audio_pool.py" "${M8_AOT_DIR}/audio_pool.h"
                "${M8_AOT_DIR}/audio_pool_memset.h"
                "${M8_AOT_DIR}/readonly_literals.py"
                ${m8_native_guard_headers}
            COMMENT "Build the checked combined native performance source copy" VERBATIM)
        get_filename_component(aot_include_dir "${M8_AOT_SOURCE}" DIRECTORY)
    endif()
    if(M8_WEB_COMPACT_POOL)
        if(NOT M8_AOT_PROFILE STREQUAL "fast")
            message(FATAL_ERROR "M8_WEB_COMPACT_POOL first_false=M8_AOT_PROFILE==fast actual=${M8_AOT_PROFILE}")
        endif()
        set(m8_compact_pool_input "${M8_AOT_SOURCE}")
        set(M8_AOT_SOURCE "${CMAKE_BINARY_DIR}/web-compact-pool/generated.c")
        add_custom_command(OUTPUT "${M8_AOT_SOURCE}"
            BYPRODUCTS "${CMAKE_BINARY_DIR}/web-compact-pool/manifest.json"
            COMMAND "${m8_native_guard_python}" -B "${M8_AOT_DIR}/audio_pool.py"
                --input "${m8_compact_pool_input}" --output "${CMAKE_BINARY_DIR}/web-compact-pool"
            DEPENDS "${m8_compact_pool_input}" "${M8_AOT_DIR}/audio_pool.py"
                "${M8_AOT_DIR}/audio_pool.h" "${M8_AOT_DIR}/audio_pool_memset.h"
                ${m8_native_guard_headers}
            COMMENT "Build the checked compact integer pool source copy" VERBATIM)
        get_filename_component(aot_include_dir "${M8_AOT_SOURCE}" DIRECTORY)
    endif()
    if(M8_AOT_CODEGEN_V2)
        if(NOT M8_AOT_PROFILE STREQUAL "fast" OR
                (NOT M8_AOT_FIXED_MEMORY AND NOT M8_WEB_CODEGEN_V2))
            message(FATAL_ERROR "M8_AOT_CODEGEN_V2 requires the fast fixed-memory profile")
        endif()
        get_filename_component(m8_codegen_v2_input_dir "${M8_AOT_SOURCE}" DIRECTORY)
        set(m8_codegen_v2_input "${M8_AOT_SOURCE}")
        # Keep each admitted source family in its own checked bundle.
        set(m8_codegen_v2_output "${CMAKE_BINARY_DIR}/codegen-v2")
        if(m8_native_performance_source)
            string(APPEND m8_codegen_v2_output "-performance")
        endif()
        set(M8_AOT_SOURCE "${m8_codegen_v2_output}/generated.c")
        set(m8_codegen_v2_entry_clone_args)
        set(m8_codegen_v2_entry_clone_deps)
        if(M8_NATIVE_ENTRY_CLONES OR (M8_WEB_CODEGEN_V2 AND M8_WEB_ENTRY_CLONES))
            if(NOT M8_AOT_CODEGEN_V2)
                message(FATAL_ERROR "M8_NATIVE_ENTRY_CLONES requires M8_AOT_CODEGEN_V2=ON")
            endif()
            set(m8_codegen_v2_entry_clone_args --entry-clones "${PROJECT_SOURCE_DIR}/aot/catalogs/entry-clones.json")
            set(m8_codegen_v2_entry_clone_deps "${PROJECT_SOURCE_DIR}/aot/catalogs/entry-clones.json")
        endif()
        add_custom_command(OUTPUT "${M8_AOT_SOURCE}"
            BYPRODUCTS "${m8_codegen_v2_output}/manifest.json"
            COMMAND "${m8_native_guard_python}" -B "${M8_AOT_DIR}/codegen_v2.py"
                --input "${m8_codegen_v2_input_dir}"
                --output "${m8_codegen_v2_output}" --replace
                --structural ${m8_codegen_v2_entry_clone_args}
            DEPENDS "${m8_codegen_v2_input}" "${M8_AOT_DIR}/codegen_v2.py"
                "${M8_AOT_DIR}/codegen_entry.py"
                ${m8_codegen_v2_entry_clone_deps}
                ${m8_native_guard_headers}
            COMMENT "Build checked hot entries, cold exits and register calls" VERBATIM)
        get_filename_component(aot_include_dir "${M8_AOT_SOURCE}" DIRECTORY)
    endif()
    set(aot_targets m8aot)
    if(M8_BUILD_AOT_COUNTERS)
        list(APPEND aot_targets m8aot-count)
    endif()
    foreach(aot_target IN LISTS aot_targets)
        add_library(${aot_target} ${M8_AOT_SOURCE})
        if(TARGET m8aot-generate)
            add_dependencies(${aot_target} m8aot-generate)
        endif()
        target_include_directories(${aot_target} PRIVATE "${aot_include_dir}"
            "${CMAKE_CURRENT_SOURCE_DIR}/aot")
        if(M8_AOT_SPLIT_UNITS GREATER 0)
            m8_split_aot(${aot_target} "${M8_AOT_SOURCE}")
        endif()
        target_compile_definitions(${aot_target} PRIVATE ${M8_AOT_DEFINITIONS})
        if(M8_AOT_FM_FAST AND aot_target STREQUAL "m8aot" AND M8_AOT_PROFILE STREQUAL "fast")
            target_sources(${aot_target} PRIVATE "${M8_AOT_DIR}/fm_fast.c" "${M8_AOT_DIR}/fm_fast_modes.c")
            target_compile_definitions(${aot_target} PRIVATE M8_AOT_FM_FAST=1)
        endif()
        if(M8_AOT_HYPER_FAST AND aot_target STREQUAL "m8aot" AND M8_AOT_PROFILE STREQUAL "fast")
            target_sources(${aot_target} PRIVATE "${M8_AOT_DIR}/hyper_fast.c")
            target_compile_definitions(${aot_target} PRIVATE M8_AOT_HYPER_FAST=1)
        endif()
        if(M8_AOT_EFFECT_FAST AND aot_target STREQUAL "m8aot" AND M8_AOT_PROFILE STREQUAL "fast")
            target_sources(${aot_target} PRIVATE "${M8_AOT_DIR}/effect_fast.c")
            target_compile_definitions(${aot_target} PRIVATE M8_AOT_EFFECT_FAST=1)
        endif()
        if(M8_AOT_MIXER_FAST AND aot_target STREQUAL "m8aot" AND M8_AOT_PROFILE STREQUAL "fast")
            target_sources(${aot_target} PRIVATE "${M8_AOT_DIR}/mixer_fast.c")
            target_compile_definitions(${aot_target} PRIVATE M8_AOT_MIXER_FAST=1)
        endif()
        if(M8_AOT_CODEGEN_V2)
            target_compile_definitions(${aot_target} PRIVATE M8_AOT_CODEGEN_V2)
        endif()
        if(M8_WEB_CODEGEN_V2)
            target_compile_definitions(${aot_target} PRIVATE M8_WEB_CODEGEN_V2)
        endif()
        if(M8_NATIVE_FP_NO_STICKY)
            target_compile_definitions(${aot_target} PRIVATE M8_NATIVE_FP_NO_STICKY)
        endif()
        if(M8_AOT_PROFILE STREQUAL "fast")
            target_compile_definitions(${aot_target} PRIVATE M8_AOT_NATIVE_FP M8_AOT_FAST_MEMORY
                M8_AOT_NARROW_CALLS M8_AOT_DWT_RAM M8_AOT_COOPERATIVE_SCS)
            foreach(option_suffix IN ITEMS FIXED_MEMORY VALIDITY_FIRST REGION_SPANS POINTEE_SPANS ARM64_FP_HARDWARE BRAIDS_MODE0 INT_FLOAT_SCALE SAMPLER_EXTENDED SAMPLER_POSITION_FP SAMPLER_SHAPE OSCILLATOR_VOICE OSCILLATOR_SPANS)
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
        foreach(contract IN ITEMS direct_regions quality_it quality_inventory)
            add_test(NAME codegen_${contract}_contract COMMAND "${m8_native_guard_python}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_codegen_${contract}.py")
            set_tests_properties(codegen_${contract}_contract PROPERTIES TIMEOUT 120)
        endforeach()
        add_test(NAME fp_constant_bounds_contract COMMAND "${m8_native_guard_python}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_fp_constant_bounds.py")
        set_tests_properties(fp_constant_bounds_contract PROPERTIES TIMEOUT 120)
        add_test(NAME fp_float_int_scale_contract COMMAND "${m8_native_guard_python}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_fp_float_int_scale.py")
        set_tests_properties(fp_float_int_scale_contract PROPERTIES TIMEOUT 120)
        add_test(NAME codegen_auto_pointee_contract COMMAND "${m8_native_guard_python}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_auto_pointee_spans.py")
        set_tests_properties(codegen_auto_pointee_contract PROPERTIES TIMEOUT 120)
    endif()
    if(m8_native_performance_source AND BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING AND
            EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_native_performance.py")
        add_test(NAME native_performance_copy_contract COMMAND "${m8_native_guard_python}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_native_performance.py" --cc "${CMAKE_C_COMPILER}"
            --firmware "${M8_AOT_FIRMWARE}")
        set_tests_properties(native_performance_copy_contract PROPERTIES TIMEOUT 90)
    endif()
    if(m8_native_performance_source AND BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING AND
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
        INSTALL_RPATH "${M8_LOADER_ORIGIN}/../m8rt")
    if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/machine/cpu_backend_test.c")
    add_executable(m8-cpu-backend-check tests/machine/cpu_backend_test.c
        core/engine/cpu-backend.c)
    target_include_directories(m8-cpu-backend-check PRIVATE ${M8RT_INCLUDE_DIRS})
    target_link_libraries(m8-cpu-backend-check PRIVATE Threads::Threads)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(m8-cpu-backend-check PRIVATE -UNDEBUG)
    endif()
    add_test(NAME native_cpu_backend COMMAND m8-cpu-backend-check)
    set_tests_properties(native_cpu_backend PROPERTIES TIMEOUT 30)
    if(NOT M8_WITH_UNICORN AND NOT CMAKE_CROSSCOMPILING AND
       EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/machine/native_product_code_test.c")
        add_executable(m8-native-product-code-check tests/machine/native_product_code_test.c)
        target_link_libraries(m8-native-product-code-check PRIVATE m8rt)
        target_compile_definitions(m8-native-product-code-check PRIVATE M8RT_INTERPRETER=1)
        add_test(NAME native_product_code_invalidation
            COMMAND m8-native-product-code-check "${M8_FIRMWARE_BIN}")
        set_tests_properties(native_product_code_invalidation PROPERTIES TIMEOUT 30)
    endif()
endif()
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
            if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/machine/test_native_interpreter_profile.py")
                add_test(NAME native_interpreter_profile COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/machine/test_native_interpreter_profile.py")
                set_tests_properties(native_interpreter_profile PROPERTIES TIMEOUT 30)
            endif()
            foreach(contract materialize_corpus run_corpus curate_corpus run_corpus_pi)
                add_test(NAME bench_${contract} COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/test_${contract}.py")
                set_tests_properties(bench_${contract} PROPERTIES TIMEOUT 30)
            endforeach()
            if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/test_native_interpreter_corpus.py")
                add_test(NAME bench_native_interpreter_corpus COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/test_native_interpreter_corpus.py")
                set_tests_properties(bench_native_interpreter_corpus PROPERTIES TIMEOUT 30)
            endif()
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

if(M8_BUILD_WEB)
    add_subdirectory(hosts/web)
endif()

include(CTest)
if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmark")
    if(NOT CMAKE_CROSSCOMPILING)
        find_package(Python3 QUIET COMPONENTS Interpreter)
        if(Python3_FOUND)
            add_test(NAME macos_source_portability COMMAND "${Python3_EXECUTABLE}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_macos_build.py")
            set_tests_properties(macos_source_portability PROPERTIES TIMEOUT 30)
            add_test(NAME pgo_pipeline_contract COMMAND "${Python3_EXECUTABLE}" -B
                -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/pgo" -p "test_*.py")
            set_tests_properties(pgo_pipeline_contract PROPERTIES TIMEOUT 90)
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
            if(M8_WITH_UNICORN)
            add_test(NAME native_app_uc_counter_contract COMMAND "${Python3_EXECUTABLE}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_app_uc_counter.py")
            set_tests_properties(native_boot_compose_contract native_app_session_contract
                native_app_uc_counter_contract PROPERTIES TIMEOUT 60)
            set_tests_properties(native_app_uc_counter_contract PROPERTIES
                ENVIRONMENT "M8_UC_COUNTER_ENGINE=$<TARGET_FILE:unicorn>")
            endif()
        endif()
    endif()
    if(M8_WITH_UNICORN)
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
        # The standalone USB controls do not call session serialization. Drop
        # that unreferenced section and its full-runtime queue dependency.
        target_compile_options(m8rt-usb-service-check PRIVATE
            -UNDEBUG -Wall -Wextra -ffunction-sections -fdata-sections)
        if(APPLE)
            target_link_options(m8rt-usb-service-check PRIVATE -Wl,-dead_strip)
        else()
            target_link_options(m8rt-usb-service-check PRIVATE -Wl,--gc-sections)
        endif()
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
    endif() # reference-dependent machine tests
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_native_fp.c")
        add_executable(m8aot-fp-check tests/aot/test_native_fp.c)
        target_include_directories(m8aot-fp-check PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/aot")
        target_link_libraries(m8aot-fp-check PRIVATE m)
        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(m8aot-fp-check PRIVATE -frounding-math
                -ffp-contract=off -fno-math-errno)
        endif()
        add_test(NAME aot_native_fp COMMAND m8aot-fp-check)
        set_tests_properties(aot_native_fp PROPERTIES TIMEOUT 30)
    endif()
endif()

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/web/CMakeLists.txt")
    add_subdirectory(tests/web)
endif()

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING AND M8_BUILD_BENCH AND
        CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    foreach(contract IN ITEMS probe instrument model validation)
        add_test(NAME speculative_${contract}_tool COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/test_speculative_${contract}.py")
        set_tests_properties(speculative_${contract}_tool PROPERTIES TIMEOUT 60)
        if(TARGET unicorn)
            set_tests_properties(speculative_${contract}_tool PROPERTIES
                ENVIRONMENT "M8_SPEC_UNICORN_LIBRARY=$<TARGET_FILE:unicorn>")
        endif()
    endforeach()
endif()

if(COMMAND m8_pgo_finalize AND TARGET m8emu AND M8_BUILD_AOT AND M8_NATIVE_ENGINE AND M8_NATIVE_REGIONS)
    m8_pgo_finalize(TARGET m8emu)
endif()
if(M8_PORTABLE_BUNDLE)
    foreach(m8_bundle_target IN ITEMS m8rt unicorn m8aot m8native-regions m8native-step m8native-boot m8native-static)
        if(TARGET ${m8_bundle_target})
            set_target_properties(${m8_bundle_target} PROPERTIES BUILD_WITH_INSTALL_RPATH ON INSTALL_RPATH "${M8_LOADER_ORIGIN}")
        endif()
    endforeach()
    if(TARGET m8rt)
        # Installed bundles keep these libraries beside m8rt. Direct build-tree
        # consumers also need the generated libraries in the sibling aot directory.
        set_target_properties(m8rt PROPERTIES INSTALL_RPATH
            "${M8_LOADER_ORIGIN};${M8_LOADER_ORIGIN}/../aot")
    endif()
endif()

if(APPLE)
    foreach(m8_dylib IN ITEMS m8rt unicorn m8aot m8native-regions m8native-step m8native-boot m8native-static)
        if(TARGET ${m8_dylib})
            set_target_properties(${m8_dylib} PROPERTIES MACOSX_RPATH ON
                INSTALL_NAME_DIR "@rpath" BUILD_WITH_INSTALL_NAME_DIR ON)
        endif()
    endforeach()
endif()
