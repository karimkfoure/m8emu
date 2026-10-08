include_guard(GLOBAL)

# These pins describe the one firmware supported by the generated engine.
set(M8_FIRMWARE_COMMIT "2392d93f39fa0ac9ae6fcbf46a4d758421b60695")
set(M8_FIRMWARE_HEX_SHA256 "434dd3d2e06765b4c3c523f21142bc57cd0811b6a7dc6a7b9596876fcb744f5c")
set(M8_FIRMWARE_BIN_SHA256 "20498b7637cee17baae249372db3b8377afd4e12a5e7062754c2ddd83621367f")
get_filename_component(_M8_FIRMWARE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_M8_FIRMWARE_CONVERTER "${CMAKE_CURRENT_LIST_DIR}/firmware.py")
set(_M8_FIRMWARE_MODULE "${CMAKE_CURRENT_LIST_FILE}")

function(_m8emu_verify_firmware input input_kind submodule)
    if(NOT EXISTS "${input}")
        message(FATAL_ERROR "Firmware is missing. Run git submodule update --init third_party/M8HeadlessFirmware.")
    endif()
    if(submodule)
        find_package(Git REQUIRED QUIET)
        execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${submodule}" rev-parse HEAD
            RESULT_VARIABLE result OUTPUT_VARIABLE actual_commit ERROR_VARIABLE error
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NOT result EQUAL 0 OR NOT actual_commit STREQUAL M8_FIRMWARE_COMMIT)
            message(FATAL_ERROR "Firmware submodule pin mismatch: expected ${M8_FIRMWARE_COMMIT}, got '${actual_commit}'. ${error}")
        endif()
    endif()
    if(input_kind STREQUAL "bin")
        set(expected "${M8_FIRMWARE_BIN_SHA256}")
    else()
        set(expected "${M8_FIRMWARE_HEX_SHA256}")
    endif()
    file(SHA256 "${input}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Firmware SHA-256 mismatch: expected ${expected}, got ${actual}.")
    endif()
endfunction()

# The ALL target checks again, so an input changed after configure cannot pass.
if(M8_FIRMWARE_VERIFY_ONLY)
    _m8emu_verify_firmware("${M8_FIRMWARE_VERIFY_INPUT}" "${M8_FIRMWARE_VERIFY_KIND}"
        "${M8_FIRMWARE_VERIFY_SUBMODULE}")
    return()
endif()

function(m8emu_prepare_firmware)
    set(submodule "${_M8_FIRMWARE_ROOT}/third_party/M8HeadlessFirmware")
    set(input "${submodule}/Releases/M8_V6_5_2C_HEADLESS.hex")
    set(input_kind hex)
    if(EXISTS "${_M8_FIRMWARE_ROOT}/GOAL.md" AND IS_DIRECTORY "${_M8_FIRMWARE_ROOT}/benchmark")
        set(M8_LAB_FIRMWARE "" CACHE FILEPATH "Lab-only override for the exact supported HEX or binary")
        if(M8_LAB_FIRMWARE)
            get_filename_component(input "${M8_LAB_FIRMWARE}" ABSOLUTE BASE_DIR "${_M8_FIRMWARE_ROOT}")
            set(submodule "")
            if(input MATCHES "\\.bin$")
                set(input_kind bin)
            endif()
        endif()
    elseif(M8_LAB_FIRMWARE)
        message(FATAL_ERROR "M8_LAB_FIRMWARE is available only in the research checkout.")
    endif()
    _m8emu_verify_firmware("${input}" "${input_kind}" "${submodule}")
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(binary "${CMAKE_BINARY_DIR}/firmware/M8_V6_5_2C_HEADLESS.bin")
    if(input_kind STREQUAL "bin")
        set(input_sha256 "${M8_FIRMWARE_BIN_SHA256}")
        set(hex "")
    else()
        set(input_sha256 "${M8_FIRMWARE_HEX_SHA256}")
        set(hex "${input}")
    endif()
    set(convert_args --input "${input}" --output "${binary}" --input-kind "${input_kind}"
        --sha256 "${input_sha256}" --binary-sha256 "${M8_FIRMWARE_BIN_SHA256}")
    # Generators need this file during configure; avoid a separate prepare step.
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${_M8_FIRMWARE_CONVERTER}" ${convert_args}
        RESULT_VARIABLE result ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Firmware conversion failed: ${error}")
    endif()
    if(NOT TARGET m8emu-firmware)
        add_custom_target(m8emu-firmware ALL
            COMMAND "${CMAKE_COMMAND}" -DM8_FIRMWARE_VERIFY_ONLY=ON
                "-DM8_FIRMWARE_VERIFY_INPUT=${input}" "-DM8_FIRMWARE_VERIFY_KIND=${input_kind}"
                "-DM8_FIRMWARE_VERIFY_SUBMODULE=${submodule}" -P "${_M8_FIRMWARE_MODULE}"
            COMMAND "${Python3_EXECUTABLE}" "${_M8_FIRMWARE_CONVERTER}" ${convert_args}
            BYPRODUCTS "${binary}"
            COMMENT "Verify the pinned M8 Headless firmware"
            VERBATIM)
    endif()
    set(M8_FIRMWARE_HEX "${hex}" PARENT_SCOPE)
    set(M8_FIRMWARE_BIN "${binary}" PARENT_SCOPE)
endfunction()
