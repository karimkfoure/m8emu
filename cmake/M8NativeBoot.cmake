# Explicit cold-boot candidate. Ordinary builds do not generate these handlers.
set(M8_NATIVE_PYTHON "${M8_REFERENCE_ROOT}/teensy-emu/.venv/bin/python"
    CACHE FILEPATH "Host Python for native handler generation and checks")
set(M8_NATIVE_BOOT_FIRMWARE "${M8_REFERENCE_ROOT}/teensy-emu/firmware/M8_V6_5_2C_HEADLESS.bin"
    CACHE FILEPATH "Original Headless 6.5.2C firmware for checked native boot")
set(M8_NATIVE_BOOT_SOURCE "" CACHE FILEPATH "Optional generated checked native boot C")
set(M8_NATIVE_BOOT_PC_FRAGMENTS "" CACHE STRING "Additional measured cold native PC fragments")
option(M8_NATIVE_BOOT_SESSION_COMPOSE "Keep measured app handlers in a separate cold companion" OFF)
set(M8_NATIVE_BOOT_SESSION_PCS "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/native-coverage-pcs/cold-app-session.pcs.txt"
    CACHE FILEPATH "Measured complete cold app companion membership")
set(m8_boot_dir "${CMAKE_CURRENT_SOURCE_DIR}/aot")
set(m8_boot_auto_source FALSE)
if(NOT M8_NATIVE_BOOT_SOURCE)
    set(m8_boot_auto_source TRUE)
    if(NOT EXISTS "${M8_NATIVE_BOOT_FIRMWARE}")
        message(FATAL_ERROR "M8_NATIVE_BOOT_FIRMWARE does not exist: ${M8_NATIVE_BOOT_FIRMWARE}")
    endif()
    set(M8_NATIVE_BOOT_SOURCE "${CMAKE_BINARY_DIR}/native-boot/generated.c")
    set(m8_boot_inputs "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/profiles/native-coverage")
    set(m8_boot_manifest "${m8_boot_inputs}/postboot-handlers-manifest.json.gz")
    set(m8_boot_pcs "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/native-coverage-pcs/union-before-missing.txt")
    set(m8_boot_args --handlers-manifest "${m8_boot_manifest}" --pc-file "${m8_boot_pcs}")
    set(m8_boot_dependencies "${m8_boot_manifest}" "${m8_boot_pcs}")
    foreach(m8_boot_fixture core sample midi-in midi-out)
        set(m8_boot_profile "${m8_boot_inputs}/reference-${m8_boot_fixture}/boot-pc.json.gz")
        list(APPEND m8_boot_args --profile "${m8_boot_profile}")
        list(APPEND m8_boot_dependencies "${m8_boot_profile}")
    endforeach()
    foreach(m8_boot_fragment IN LISTS M8_NATIVE_BOOT_PC_FRAGMENTS)
        if(NOT EXISTS "${m8_boot_fragment}")
            message(FATAL_ERROR "Native boot PC fragment does not exist: ${m8_boot_fragment}")
        endif()
        list(APPEND m8_boot_args --pc-file "${m8_boot_fragment}")
        list(APPEND m8_boot_dependencies "${m8_boot_fragment}")
    endforeach()
    file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/native-boot/generator-options.txt"
        CONTENT "firmware=${M8_NATIVE_BOOT_FIRMWARE}\npython=${M8_NATIVE_PYTHON}\ninputs=${m8_boot_args}\n")
    add_custom_command(OUTPUT "${M8_NATIVE_BOOT_SOURCE}"
        BYPRODUCTS "${CMAKE_BINARY_DIR}/native-boot/manifest.json"
        COMMAND "${M8_NATIVE_PYTHON}" -B "${m8_boot_dir}/native_boot.py"
            --firmware "${M8_NATIVE_BOOT_FIRMWARE}" ${m8_boot_args}
            --output "${M8_NATIVE_BOOT_SOURCE}" --manifest "${CMAKE_BINARY_DIR}/native-boot/manifest.json"
        DEPENDS "${M8_NATIVE_BOOT_FIRMWARE}" ${m8_boot_dependencies}
            "${m8_boot_dir}/native_boot.py" "${m8_boot_dir}/native_boot_support.h"
            "${m8_boot_dir}/native_step.py" "${m8_boot_dir}/native_regions.py"
            "${m8_boot_dir}/translate.py" "${m8_boot_dir}/m8_aot.h"
            "${m8_boot_dir}/native_step.h" "${m8_boot_dir}/native_step_special.h"
            "${m8_boot_dir}/runtime.h" "${m8_boot_dir}/fp.h" "${m8_boot_dir}/integer.h"
            "${CMAKE_BINARY_DIR}/native-boot/generator-options.txt"
        COMMENT "Generate byte-checked native cold-boot handlers" VERBATIM)
elseif(NOT EXISTS "${M8_NATIVE_BOOT_SOURCE}")
    message(FATAL_ERROR "M8_NATIVE_BOOT_SOURCE does not exist: ${M8_NATIVE_BOOT_SOURCE}")
endif()
add_library(m8native-boot SHARED "${M8_NATIVE_BOOT_SOURCE}")
target_include_directories(m8native-boot PRIVATE "${m8_boot_dir}")
# GCC misleading-indentation analysis is costly on the generated switch.
# Keep the other warnings and all warnings on handwritten runtime sources.
target_compile_options(m8native-boot PRIVATE -O2 -frounding-math -ffp-contract=off
    -Wall -Wextra -Wno-misleading-indentation
    -Wno-unused-label -Wno-unused-variable -Wno-unused-but-set-variable)
target_link_libraries(m8native-boot PRIVATE m)
set_target_properties(m8native-boot PROPERTIES C_VISIBILITY_PRESET hidden
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/aot")
if(M8_NATIVE_BOOT_SESSION_COMPOSE)
    if(NOT EXISTS "${M8_NATIVE_BOOT_SESSION_PCS}")
        message(FATAL_ERROR "M8_NATIVE_BOOT_SESSION_PCS does not exist: ${M8_NATIVE_BOOT_SESSION_PCS}")
    endif()
    if(M8_NATIVE_BOOT_PC_FRAGMENTS OR NOT m8_boot_auto_source)
        message(FATAL_ERROR "Cold app composition requires automatic unchanged base inputs (no boot fragments/source override)")
    endif()
    set_target_properties(m8native-boot PROPERTIES OUTPUT_NAME m8native-boot-base)
    set(m8_compose_dir "${CMAKE_BINARY_DIR}/native-boot-session")
    # Preserve base declines and prefix boundaries. A unified instruction set
    # can erase a logical USB slice, even when each instruction is correct.
    add_custom_command(OUTPUT "${m8_compose_dir}/generated.c" "${m8_compose_dir}/wrapper.c"
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${m8_compose_dir}"
        COMMAND "${M8_NATIVE_PYTHON}" -B "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/native-boot-compose.py"
            --firmware "${M8_NATIVE_BOOT_FIRMWARE}" --base-library "$<TARGET_FILE:m8native-boot>"
            --base-manifest "${CMAKE_BINARY_DIR}/native-boot/manifest.json"
            --pc-file "${M8_NATIVE_BOOT_SESSION_PCS}" --no-default-profiles --generate-only --output "${m8_compose_dir}"
        DEPENDS m8native-boot "${M8_NATIVE_BOOT_SESSION_PCS}"
            "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/native-boot-compose.py"
        COMMENT "Generate measured cold app companion without changing base boundaries" VERBATIM)
    add_library(m8native-boot-session SHARED "${m8_compose_dir}/generated.c" "${m8_compose_dir}/wrapper.c")
    set_source_files_properties("${m8_compose_dir}/generated.c" PROPERTIES
        COMPILE_DEFINITIONS m8_native_step=m8_native_boot_supplement_step)
    target_include_directories(m8native-boot-session PRIVATE "${m8_boot_dir}")
    target_compile_options(m8native-boot-session PRIVATE -O2 -frounding-math -ffp-contract=off
        -Wall -Wextra -Wno-misleading-indentation -Wno-unused-label -Wno-unused-variable -Wno-unused-but-set-variable)
    target_link_libraries(m8native-boot-session PRIVATE m ${CMAKE_DL_LIBS})
    target_link_options(m8native-boot-session PRIVATE -Wl,-Bsymbolic)
    set_target_properties(m8native-boot-session PROPERTIES C_VISIBILITY_PRESET hidden
        OUTPUT_NAME m8native-boot LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/aot")
endif()
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING AND
        EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_native_boot.py")
    add_test(NAME native_boot_handler_contract COMMAND "${M8_NATIVE_PYTHON}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_native_boot.py" --firmware "${M8_NATIVE_BOOT_FIRMWARE}"
        --library "$<TARGET_FILE:m8native-boot>")
    set_tests_properties(native_boot_handler_contract PROPERTIES TIMEOUT 90
        ENVIRONMENT "LIBUNICORN_PATH=$<TARGET_FILE_DIR:unicorn>")
endif()
