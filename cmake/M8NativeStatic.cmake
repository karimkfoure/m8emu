# Default-off static cold companion. The top level owns the selecting option.
set(M8_NATIVE_STATIC_SOURCE_DIR "" CACHE PATH "Reuse an exactly matching generated static companion")
set(M8_NATIVE_STATIC_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/aot/catalogs/static-coverage.json"
    CACHE FILEPATH "Protected static firmware coverage inputs")
set(M8_NATIVE_STATIC_PAGES_PER_SHARD 4 CACHE STRING "Guest pages per cold translation unit")
option(M8_NATIVE_STATIC_SHARED_STATE "Use shared CPU registers in static cold regions" OFF)
set(M8_NATIVE_STATIC_PC_FILES "" CACHE STRING "Extra measured PC roots appended to the static config (semicolon-separated)")
option(M8_NATIVE_STATIC_REGIONS_ONLY "Emit bulk regions without the duplicate single-step decoder" OFF)
set(M8_NATIVE_PYTHON "${M8_AOT_PYTHON}" CACHE FILEPATH "Host Python for native generation")
if(NOT M8_NATIVE_PYTHON)
    message(FATAL_ERROR "Static coverage requires M8_NATIVE_PYTHON or M8_AOT_PYTHON")
endif()
if(NOT EXISTS "${M8_FIRMWARE_BIN}")
    message(FATAL_ERROR "Static coverage firmware does not exist: ${M8_FIRMWARE_BIN}")
endif()
set(m8_static_dir "${CMAKE_BINARY_DIR}/native-static")
if(M8_NATIVE_STATIC_SOURCE_DIR)
    get_filename_component(m8_static_dir "${M8_NATIVE_STATIC_SOURCE_DIR}" ABSOLUTE)
endif()
set(m8_static_args --firmware "${M8_FIRMWARE_BIN}" --config "${M8_NATIVE_STATIC_CONFIG}"
    --input-root "${CMAKE_CURRENT_SOURCE_DIR}" --output "${m8_static_dir}"
    --pages-per-shard "${M8_NATIVE_STATIC_PAGES_PER_SHARD}")
foreach(m8_static_pc_file IN LISTS M8_NATIVE_STATIC_PC_FILES)
    if(NOT EXISTS "${m8_static_pc_file}")
        message(FATAL_ERROR "Static coverage PC file does not exist: ${m8_static_pc_file}")
    endif()
    list(APPEND m8_static_args --pc-file "${m8_static_pc_file}")
endforeach()
if(M8_NATIVE_STATIC_SHARED_STATE)
    list(APPEND m8_static_args --shared-state)
endif()
if(M8_NATIVE_STATIC_REGIONS_ONLY)
    list(APPEND m8_static_args --regions-only)
endif()
set(m8_static_current 1)
if(EXISTS "${m8_static_dir}/coverage.json")
    execute_process(COMMAND "${M8_NATIVE_PYTHON}" -B "${CMAKE_CURRENT_SOURCE_DIR}/aot/static_coverage.py"
        ${m8_static_args} --check-output RESULT_VARIABLE m8_static_current
        OUTPUT_VARIABLE m8_static_check_output ERROR_VARIABLE m8_static_check_error)
endif()
if(NOT m8_static_current EQUAL 0)
    if(M8_NATIVE_STATIC_SOURCE_DIR)
        message(FATAL_ERROR "Reused static companion is missing or stale: ${m8_static_dir}\n${m8_static_check_error}")
    endif()
    execute_process(COMMAND "${M8_NATIVE_PYTHON}" -B "${CMAKE_CURRENT_SOURCE_DIR}/aot/static_coverage.py"
        ${m8_static_args} --generate-sources RESULT_VARIABLE m8_static_generated
        OUTPUT_VARIABLE m8_static_generate_output ERROR_VARIABLE m8_static_generate_error)
    if(NOT m8_static_generated EQUAL 0)
        message(FATAL_ERROR "Static coverage generation failed (${m8_static_generated}):\n${m8_static_generate_error}")
    endif()
    message(STATUS "${m8_static_generate_output}")
endif()
include("${m8_static_dir}/sources.cmake")
include("${m8_static_dir}/inputs.cmake")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${M8_NATIVE_STATIC_GENERATION_INPUTS})
file(GLOB m8_static_generator_inputs CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/aot/*.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/aot/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/aot/functions*.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/aot/catalogs/corpus/*.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/aot/catalogs/static/*.txt")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${m8_static_generator_inputs}
    "${M8_NATIVE_STATIC_CONFIG}" "${M8_FIRMWARE_BIN}" ${M8_NATIVE_STATIC_PC_FILES})
# Regions own normal execution. Tune the duplicate cold single-step decoder
# independently for compile cost or size; exact FP and fallback contracts apply.
set(M8_NATIVE_STATIC_HANDLER_OPT_LEVEL s CACHE STRING "Cold single-step optimization level (0, 1, 2 or s)")
set_property(CACHE M8_NATIVE_STATIC_HANDLER_OPT_LEVEL PROPERTY STRINGS 0 1 2 s)
if(NOT M8_NATIVE_STATIC_HANDLER_OPT_LEVEL MATCHES "^[012s]$")
    message(FATAL_ERROR "M8_NATIVE_STATIC_HANDLER_OPT_LEVEL requires 0, 1, 2 or s")
endif()
add_library(m8native-static ${M8_GENERATED_LIBRARY_KIND} ${M8_NATIVE_STATIC_GENERATED_SOURCES})
foreach(m8_static_source IN LISTS M8_NATIVE_STATIC_GENERATED_SOURCES)
    if(NOT M8_NATIVE_STATIC_HANDLER_OPT_LEVEL STREQUAL "2" AND
            m8_static_source MATCHES "-handlers\\.c$")
        set_property(SOURCE "${m8_static_source}" TARGET_DIRECTORY m8native-static
            APPEND PROPERTY COMPILE_OPTIONS "-O${M8_NATIVE_STATIC_HANDLER_OPT_LEVEL}")
    endif()
    if(M8_NATIVE_STATIC_SHARED_STATE AND m8_static_source MATCHES "-regions[.]c$")
        set_property(SOURCE "${m8_static_source}" TARGET_DIRECTORY m8native-static
            APPEND PROPERTY COMPILE_DEFINITIONS M8_NATIVE_REGION_SHARED_STATE)
    endif()
endforeach()
target_include_directories(m8native-static PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/aot")
target_compile_definitions(m8native-static PRIVATE M8_AOT_NATIVE_FP
    M8_NATIVE_REGION_INTEGER_FP_FASTPATH=1 M8_NATIVE_REGION_INLINE_MEMORY=1
    M8_NATIVE_REGION_SERVICE_CONTINUE=1)
if(M8_NATIVE_FP_NO_STICKY)
    target_compile_definitions(m8native-static PRIVATE M8_NATIVE_FP_NO_STICKY)
endif()
target_compile_options(m8native-static PRIVATE -O2 -frounding-math -ffp-contract=off -fno-math-errno
    -Wall -Wextra -Wno-misleading-indentation -Wno-unused-label -Wno-unused-variable -Wno-unused-but-set-variable)
# The no-sticky FP fast path executes guest FMA inline; without hardware FMA
# every FMA falls back to the exact helper (MXCSR flip plus fenv round-trip),
# which dominated boot/LOAD profiles once no-sticky became the native default.
# Select -mfma the same way the AOT engine does (auto probes the build host).
set(M8_NATIVE_STATIC_FMA auto CACHE STRING "x86 FMA for the static companion: auto, on or off")
set_property(CACHE M8_NATIVE_STATIC_FMA PROPERTY STRINGS auto on off)
if(NOT M8_NATIVE_STATIC_FMA MATCHES "^(auto|on|off)$")
    message(FATAL_ERROR "M8_NATIVE_STATIC_FMA must be auto, on or off")
endif()
set(m8_static_fma OFF)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$" AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    if(M8_NATIVE_STATIC_FMA STREQUAL "on")
        set(m8_static_fma ON)
    elseif(M8_NATIVE_STATIC_FMA STREQUAL "auto" AND NOT CMAKE_CROSSCOMPILING AND NOT M8_BUILD_WEB)
        include(CheckCSourceRuns)
        check_c_source_runs("int main(void) { __builtin_cpu_init(); return __builtin_cpu_supports(\"fma\") ? 0 : 1; }" M8_NATIVE_STATIC_HOST_HAS_FMA)
        set(m8_static_fma ${M8_NATIVE_STATIC_HOST_HAS_FMA})
    endif()
elseif(M8_NATIVE_STATIC_FMA STREQUAL "on")
    message(FATAL_ERROR "M8_NATIVE_STATIC_FMA=on requires an x86-64 GCC/Clang target")
endif()
if(m8_static_fma)
    target_compile_options(m8native-static PRIVATE -mfma)
endif()
target_link_libraries(m8native-static PRIVATE m)
set_target_properties(m8native-static PROPERTIES C_VISIBILITY_PRESET hidden
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/aot" BUILD_RPATH_USE_ORIGIN ON
    INSTALL_RPATH "${M8_LOADER_ORIGIN}")
# Keep the static cold companion outside hot compiler PGO.
if(M8_NATIVE_TLS_INITIAL_EXEC)
    target_compile_options(m8native-static PRIVATE -ftls-model=initial-exec)
    target_link_options(m8native-static PRIVATE -ftls-model=initial-exec)
endif()
target_link_libraries(m8rt PRIVATE m8native-static)
target_compile_definitions(m8rt PRIVATE M8RT_STATIC_COVERAGE=1)
if(M8_NATIVE_STATIC_REGIONS_ONLY)
    target_compile_definitions(m8rt PRIVATE M8RT_STATIC_REGIONS_ONLY=1)
endif()
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING AND
        EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_static_coverage.py")
    add_test(NAME native_static_coverage_contract COMMAND "${M8_NATIVE_PYTHON}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_static_coverage.py")
    add_test(NAME native_static_shared_state_contract COMMAND "${M8_NATIVE_PYTHON}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_static_shared_state.py")
    set_tests_properties(native_static_coverage_contract native_static_shared_state_contract
        PROPERTIES TIMEOUT 90)
endif()
