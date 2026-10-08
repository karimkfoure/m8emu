# Reject stale enabled cache entries rather than silently build another mode.
# Implementations and measured decisions are recoverable at 2141f1df.
set(m8_retired_performance_options
    M8_AOT_CODEGEN_V3 M8_AOT_STACK_MEMORY M8_NATIVE_DEVICE_OUTLINE
    M8_NATIVE_SD_STREAM M8_AOT_FASTMEM M8_AOT_WAV_BATCH
    M8_AOT_WAVE_SAMPLE_REGION M8_AOT_FP_CALLBACK_GROUPS
    M8_AOT_NARROW_CALLBACKS M8_AOT_SCALAR_BYTE_CALLBACKS
    M8_AOT_F32_RTZ M8_AOT_F32_CMP M8_AOT_FLOAT_NAN_REPAIR
    M8_AOT_STATIC_COLD_PARTITION M8_AOT_NO_JUMP_THREADING
    M8_AOT_AUTO_POINTEE_SPANS M8_AOT_VECTOR_BUFFERS
    M8_AOT_FM_GROUPED_OWNERS M8_AOT_FM_LOCAL_STATE M8_AOT_FM_STABLE_GUARDS
    M8_AOT_ENTRY_SEEDS M8_AOT_LOCAL_TABLE_DISPATCH M8_AOT_CANONICAL_MEMORY
    M8_AOT_FLOAT_OPERANDS M8_AOT_OVERFLOW_BUILTINS
    M8_AOT_SELECTIVE_NO_JUMP_THREADING)
foreach(m8_retired_option IN LISTS m8_retired_performance_options)
    if(DEFINED ${m8_retired_option} AND ${m8_retired_option})
        message(FATAL_ERROR
            "Retired performance option ${m8_retired_option} is enabled. Remove this cache entry or use a fresh build directory. See docs/performance-options.md; recover the implementation from 2141f1df.")
    endif()
endforeach()
unset(m8_retired_option)
unset(m8_retired_performance_options)
