# One source for the qualified ARM app settings and the Pi script queries.
include_guard(GLOBAL)

set(M8_QUALIFIED_PROFILE none CACHE STRING "Qualified build profile (none or arm64-app)")
set_property(CACHE M8_QUALIFIED_PROFILE PROPERTY STRINGS none arm64-app)

# Keep these quoted NAME:TYPE=VALUE rows readable by qualified-profile.py.
set(M8_QUALIFIED_ARM64_APP_CORE
    "M8_BUILD_AOT:BOOL=ON"
    "M8_NATIVE_ENGINE:BOOL=ON"
    "M8_NATIVE_REGIONS:BOOL=ON"
    "M8_NATIVE_AOT_CODE_GUARD:BOOL=ON"
    "M8_AOT_CODEGEN_V2:BOOL=ON"
    "M8_AOT_REAL_CALLBACKS:BOOL=ON"
    "M8_NATIVE_DEVICE_FAST:BOOL=ON"
    "M8_AOT_ARM64_FP_HARDWARE:BOOL=ON"
    "M8_NATIVE_REGION_SHARED_STATE:BOOL=ON"
    "M8_NATIVE_FP_NO_STICKY:BOOL=ON"
    "M8_NATIVE_REGION_DIRECT_CALLS:BOOL=ON"
    "M8_NATIVE_REGION_LOCAL_NZCV:BOOL=ON"
    "M8_NATIVE_REGION_DEAD_NZCV:BOOL=ON"
    "M8_NATIVE_REGION_MEMORY_SPANS:BOOL=ON"
    "M8_NATIVE_ENTRY_CLONES:BOOL=ON"
    "M8_NATIVE_CORPUS_CATALOG:BOOL=ON"
    "M8_NATIVE_TLS_INITIAL_EXEC:BOOL=ON"
    "M8_NATIVE_REGION_INTEGER_FP_FASTPATH:BOOL=ON"
    "M8_NATIVE_REGION_INLINE_MEMORY:BOOL=ON"
    "M8_NATIVE_REGION_SERVICE_CONTINUE:BOOL=ON"
    "M8_NATIVE_REGION_CHAIN:BOOL=ON"
    "M8_NATIVE_REGION_EVENT_LOOP:BOOL=ON"
    "M8_NATIVE_REGION_INLINE_SERVICES:BOOL=ON"
    "M8_NATIVE_REGION_RAM_EXCLUSIVES:BOOL=ON"
    "M8_NATIVE_REGION_COMPUTED_REDIRECTS:BOOL=ON"
    "M8_RT_CALLBACK_TIMING:BOOL=OFF"
)
set(M8_QUALIFIED_ARM64_APP_APP
    "M8_NATIVE_APP_DEFAULT:BOOL=ON"
    "M8_NATIVE_BOOT_DEFAULT:BOOL=OFF"
    "M8_NATIVE_AUDIO_QUEUE_MS:STRING=150"
    "M8_NATIVE_POLL_MS:STRING=20"
    "M8_NATIVE_DEADLINE_YIELD_DEFAULT:BOOL=ON"
)
# These settings select script recipes, not additional runtime code paths.
set(M8_QUALIFIED_ARM64_APP_POLICY
    "M8_QUALIFIED_SERVICE_REGION_RECIPE:BOOL=ON"
    "M8_QUALIFIED_EXCLUSIVE_TRANSFER:BOOL=ON"
    "M8_QUALIFIED_PGO:STRING=auto"
)

if(M8_QUALIFIED_PROFILE STREQUAL "none")
    return()
endif()
if(NOT M8_QUALIFIED_PROFILE STREQUAL "arm64-app")
    message(FATAL_ERROR "Unknown M8_QUALIFIED_PROFILE: actual=${M8_QUALIFIED_PROFILE}; supported=none,arm64-app")
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
        NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    message(FATAL_ERROR "M8_QUALIFIED_PROFILE=arm64-app requires Linux ARM64: system=${CMAKE_SYSTEM_NAME}; processor=${CMAKE_SYSTEM_PROCESSOR}")
endif()

# Never replace an explicit cache value or a parent project's normal variable.
# Use a new build directory when changing profiles; existing values are inputs.
foreach(m8_profile_row IN LISTS M8_QUALIFIED_ARM64_APP_CORE
        M8_QUALIFIED_ARM64_APP_APP M8_QUALIFIED_ARM64_APP_POLICY)
    if(NOT m8_profile_row MATCHES "^([A-Z0-9_]+):(BOOL|STRING)=(.+)$")
        message(FATAL_ERROR "Invalid qualified profile row: ${m8_profile_row}")
    endif()
    set(m8_profile_name "${CMAKE_MATCH_1}")
    set(m8_profile_type "${CMAKE_MATCH_2}")
    set(m8_profile_value "${CMAKE_MATCH_3}")
    # The previous source recipe has no redirect catalog. A caller's explicit
    # computed-redirect value still takes precedence over this derived default.
    if(m8_profile_name STREQUAL "M8_NATIVE_REGION_COMPUTED_REDIRECTS" AND
            NOT M8_NATIVE_CORPUS_CATALOG)
        set(m8_profile_value OFF)
    endif()
    if(NOT DEFINED ${m8_profile_name})
        set(${m8_profile_name} "${m8_profile_value}" CACHE ${m8_profile_type}
            "Default from the qualified arm64-app profile")
    endif()
endforeach()
unset(m8_profile_row)
unset(m8_profile_name)
unset(m8_profile_type)
unset(m8_profile_value)
