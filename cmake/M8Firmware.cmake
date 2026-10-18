include_guard(GLOBAL)

# These pins describe the one firmware supported by the generated engine.
set(M8_FIRMWARE_HEX_SHA256 "434dd3d2e06765b4c3c523f21142bc57cd0811b6a7dc6a7b9596876fcb744f5c")
set(M8_FIRMWARE_BIN_SHA256 "20498b7637cee17baae249372db3b8377afd4e12a5e7062754c2ddd83621367f")
get_filename_component(_M8_FIRMWARE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_M8_FIRMWARE_CONVERTER "${CMAKE_CURRENT_LIST_DIR}/firmware.py")
set(_M8_FIRMWARE_MODULE "${CMAKE_CURRENT_LIST_FILE}")
set(_M8_FIRMWARE_HELP
    "Required input: Headless firmware 6.5.2C (M8_V6_5_2C_HEADLESS.hex). Expected HEX SHA-256: ${M8_FIRMWARE_HEX_SHA256}. Configure with -DM8_FIRMWARE=PATH or set the M8_FIRMWARE environment variable.")

function(_m8emu_verify_firmware input input_kind submodule)
    if(NOT EXISTS "${input}" OR IS_DIRECTORY "${input}")
        message(FATAL_ERROR "Firmware file is missing: '${input}'. ${_M8_FIRMWARE_HELP}")
    endif()
    if(input_kind STREQUAL "bin")
        set(expected "${M8_FIRMWARE_BIN_SHA256}")
    else()
        set(expected "${M8_FIRMWARE_HEX_SHA256}")
    endif()
    file(SHA256 "${input}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Firmware SHA-256 mismatch for '${input}': expected ${expected}, got ${actual}. ${_M8_FIRMWARE_HELP}")
    endif()
endfunction()

# The ALL target checks again, so an input changed after configure cannot pass.
if(M8_FIRMWARE_VERIFY_ONLY)
    _m8emu_verify_firmware("${M8_FIRMWARE_VERIFY_INPUT}" "${M8_FIRMWARE_VERIFY_KIND}"
        "${M8_FIRMWARE_VERIFY_SUBMODULE}")
    return()
endif()

function(m8emu_prepare_firmware)
    set(M8_FIRMWARE "$ENV{M8_FIRMWARE}" CACHE FILEPATH "Official M8 Headless 6.5.2C HEX input")
    set(submodule "")
    set(input "")
    set(input_kind hex)
    if(M8_FIRMWARE)
        get_filename_component(input "${M8_FIRMWARE}" ABSOLUTE BASE_DIR "${_M8_FIRMWARE_ROOT}")
        # Keep an absolute HEX path in the cache for nested PGO builds.
        set(M8_FIRMWARE "${input}" CACHE FILEPATH "Official M8 Headless 6.5.2C HEX input" FORCE)
        set(submodule "")
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
