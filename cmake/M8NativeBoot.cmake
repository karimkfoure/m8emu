# Explicit cold-boot candidate. Ordinary builds do not generate these handlers.
set(M8_NATIVE_PYTHON "${M8_REFERENCE_ROOT}/teensy-emu/.venv/bin/python"
    CACHE FILEPATH "Host Python for native handler generation and checks")
set(M8_NATIVE_BOOT_FIRMWARE "${M8_FIRMWARE_BIN}"
    CACHE FILEPATH "Original Headless 6.5.2C firmware for checked native boot")
set(M8_NATIVE_BOOT_SOURCE "" CACHE FILEPATH "Optional generated checked native boot C")
set(m8_boot_dir "${CMAKE_CURRENT_SOURCE_DIR}/aot")
if(NOT M8_NATIVE_BOOT_SOURCE)
    if(NOT EXISTS "${M8_NATIVE_BOOT_FIRMWARE}")
        message(FATAL_ERROR "M8_NATIVE_BOOT_FIRMWARE does not exist: ${M8_NATIVE_BOOT_FIRMWARE}")
    endif()
    set(M8_NATIVE_BOOT_SOURCE "${CMAKE_BINARY_DIR}/native-boot/generated.c")
    if(M8_WITH_UNICORN AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/profiles/native-coverage")
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
    endif() # Lab captures are optional; the static companion owns other PCs.
    # Include hardware-only boot PCs without retained profile coverage.
    set(m8_boot_hw_scan "${CMAKE_CURRENT_SOURCE_DIR}/aot/catalogs/static/cold-boot-hw-scan.pcs.txt")
    if(NOT EXISTS "${m8_boot_hw_scan}")
        message(FATAL_ERROR "Native boot hardware-scan PCs do not exist: ${m8_boot_hw_scan}")
    endif()
    list(APPEND m8_boot_args --pc-file "${m8_boot_hw_scan}")
    list(APPEND m8_boot_dependencies "${m8_boot_hw_scan}")
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
add_library(m8native-boot ${M8_GENERATED_LIBRARY_KIND} "${M8_NATIVE_BOOT_SOURCE}"
    "${m8_boot_dir}/native_boot_entry.c")
if(CMAKE_SYSTEM_NAME STREQUAL "WASI")
    target_compile_definitions(m8native-boot PRIVATE m8_native_step=m8_web_cold_step)
endif()
if(NOT M8_WITH_UNICORN)
    target_link_libraries(m8rt PRIVATE m8native-boot)
endif()
if(UNIX AND NOT APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "WASI")
    target_link_options(m8native-boot PRIVATE "LINKER:-Bsymbolic-functions")
endif()
target_include_directories(m8native-boot PRIVATE "${m8_boot_dir}")
if(M8_NATIVE_FP_NO_STICKY)
    target_compile_definitions(m8native-boot PRIVATE M8_NATIVE_FP_NO_STICKY)
endif()
# GCC misleading-indentation analysis is costly on the generated switch.
# Keep the other warnings and all warnings on handwritten runtime sources.
target_compile_options(m8native-boot PRIVATE -O2 -frounding-math -ffp-contract=off
    -Wall -Wextra -Wno-misleading-indentation
    -Wno-unused-label -Wno-unused-variable -Wno-unused-but-set-variable)
target_link_libraries(m8native-boot PRIVATE m)
set_target_properties(m8native-boot PROPERTIES C_VISIBILITY_PRESET hidden
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/aot")
if(M8_WITH_UNICORN AND BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING AND
        EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_native_boot.py")
    add_test(NAME native_boot_handler_contract COMMAND "${M8_NATIVE_PYTHON}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot/test_native_boot.py" --firmware "${M8_NATIVE_BOOT_FIRMWARE}"
        --library "$<TARGET_FILE:m8native-boot>")
    set_tests_properties(native_boot_handler_contract PROPERTIES TIMEOUT 90
        ENVIRONMENT "LIBUNICORN_PATH=$<TARGET_FILE_DIR:unicorn>")
endif()
