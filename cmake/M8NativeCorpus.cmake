# Source-only corpus expansion. All existing native profile switches stay explicit.
foreach(required IN ITEMS M8_NATIVE_REGIONS M8_NATIVE_REGION_CHAIN
        M8_NATIVE_REGION_EVENT_LOOP M8_NATIVE_REGION_SHARED_STATE
        M8_NATIVE_REGION_COMPUTED_REDIRECTS
        M8_NATIVE_REGION_INLINE_SERVICES M8_NATIVE_REGION_RAM_EXCLUSIVES
        M8_NATIVE_REGION_SERVICE_CONTINUE M8_NATIVE_REGION_INLINE_MEMORY
        M8_NATIVE_REGION_INTEGER_FP_FASTPATH)
    if(NOT ${required})
        message(FATAL_ERROR "Corpus native catalog: ${required}=${${required}}; required=ON")
    endif()
endforeach()
if(M8_NATIVE_STEP_SOURCE OR M8_NATIVE_REGION_SOURCE)
    message(FATAL_ERROR "Corpus native catalog: step=${M8_NATIVE_STEP_SOURCE}; regions=${M8_NATIVE_REGION_SOURCE}; both must be empty for automatic generation")
endif()
set(M8_NATIVE_PYTHON "${M8_AOT_PYTHON}"
    CACHE FILEPATH "Host Python for native playback generation and checks")
set(M8_NATIVE_REGION_FIRMWARE "${M8_FIRMWARE_BIN}"
    CACHE FILEPATH "Original firmware for measured playback regions")
set(m8_native_corpus_dir "${CMAKE_BINARY_DIR}/corpus-native")
set(m8_native_region_dir "${CMAKE_CURRENT_SOURCE_DIR}/aot")
set(M8_NATIVE_STEP_SOURCE "${m8_native_corpus_dir}/handlers.c")
set(M8_NATIVE_REGION_SOURCE "${m8_native_corpus_dir}/wrapper.c")
if(M8_BUILD_WEB)
    # WASI keeps one compiled region set and the checked interpreter fallback.
    # The existing object aliases let this link trim reuse the hot compilation.
    set(M8_NATIVE_REGION_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/hosts/web/regions.c")
endif()
set(m8_corpus_inputs "${CMAKE_CURRENT_SOURCE_DIR}/aot/catalogs/corpus")
set(m8_native_corpus_layout_default "${m8_corpus_inputs}/layout.json")
if(M8_BUILD_WEB)
    set(m8_native_corpus_layout_default "${CMAKE_CURRENT_SOURCE_DIR}/hosts/web/region-layout.json")
endif()
set(M8_NATIVE_CORPUS_LAYOUT "${m8_native_corpus_layout_default}"
    CACHE FILEPATH "Checked native region page layout")
file(GLOB m8_corpus_generator_inputs CONFIGURE_DEPENDS
    "${m8_native_region_dir}/*.py" "${m8_native_region_dir}/*.h")
set(m8_corpus_scope_args "")
if(M8_NATIVE_PGO_SCOPED)
    list(APPEND m8_corpus_scope_args --pgo-scope "${m8_native_region_dir}/catalogs/pgo-scope.json")
endif()
set(m8_corpus_region_extra_args "")
if(M8_NATIVE_REGION_DIRECT_CALLS)
    list(APPEND m8_corpus_region_extra_args --direct-calls)
endif()
if(M8_NATIVE_REGION_LOCAL_NZCV)
    list(APPEND m8_corpus_region_extra_args --local-nzcv)
endif()
if(M8_NATIVE_REGION_DEAD_NZCV)
    list(APPEND m8_corpus_region_extra_args --dead-nzcv)
endif()
if(M8_NATIVE_REGION_MEMORY_SPANS)
    list(APPEND m8_corpus_region_extra_args --region-memory-spans)
endif()
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/corpus-native/generator-options.txt"
    CONTENT "direct_calls=${M8_NATIVE_REGION_DIRECT_CALLS}\nlocal_nzcv=${M8_NATIVE_REGION_LOCAL_NZCV}\ndead_nzcv=${M8_NATIVE_REGION_DEAD_NZCV}\nmemory_spans=${M8_NATIVE_REGION_MEMORY_SPANS}\n")
set(m8_macho_source_command "")
if(APPLE)
    set(m8_macho_source_command COMMAND "${M8_NATIVE_PYTHON}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/macho-native-source.py"
        "${m8_native_corpus_dir}/wrapper.c")
endif()
set(m8_native_corpus_stamp "${m8_native_corpus_dir}/generated.stamp")
add_custom_command(OUTPUT "${m8_native_corpus_stamp}"
    BYPRODUCTS "${m8_native_corpus_dir}/hot.c" "${m8_native_corpus_dir}/cold.c"
        "${m8_native_corpus_dir}/handlers.c" "${m8_native_corpus_dir}/wrapper.c"
        "${m8_native_corpus_dir}/generation.json"
    COMMAND "${M8_NATIVE_PYTHON}" -B "${CMAKE_CURRENT_SOURCE_DIR}/aot/prepare-corpus-native.py"
        --firmware "${M8_NATIVE_REGION_FIRMWARE}"
        --pc-file "${m8_corpus_inputs}/playback.txt"
        --handler-pc-file "${m8_corpus_inputs}/handlers-complete.txt"
        --cold-pc-file "${m8_corpus_inputs}/baseline-cold.txt"
        --layout-input "${M8_NATIVE_CORPUS_LAYOUT}"
        --output "${m8_native_corpus_dir}" ${m8_corpus_scope_args}
        ${m8_corpus_region_extra_args}
    ${m8_macho_source_command}
    COMMAND "${CMAKE_COMMAND}" -E touch "${m8_native_corpus_stamp}"
    DEPENDS "${M8_NATIVE_REGION_FIRMWARE}" ${m8_corpus_generator_inputs}
        "${CMAKE_BINARY_DIR}/corpus-native/generator-options.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/aot/prepare-corpus-native.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/macho-native-source.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/aot/corpus-coverage-catalog.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/aot/native-engine-redirects.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/aot/native-session-link.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/aot/native-session-compose.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/aot/catalogs/redirect-targets-services.txt"
        "${m8_native_region_dir}/functions-playback-native-pcs.txt"
        "${m8_native_region_dir}/functions-playback-native-groove-pcs.txt"
        "${m8_native_region_dir}/functions-playback-native-layout.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/aot/catalogs/session-pcs.txt"
        "${m8_corpus_inputs}/playback.txt" "${m8_corpus_inputs}/handlers-complete.txt"
        "${m8_corpus_inputs}/baseline-cold.txt" "${M8_NATIVE_CORPUS_LAYOUT}"
    COMMENT "Generate default-off corpus native coverage" VERBATIM)
# One owner prevents parallel Make targets from running the same generator.
# A stamp records success even when unchanged C files retain old timestamps.
add_custom_target(m8-native-corpus-generate DEPENDS "${m8_native_corpus_stamp}")
# Preserve separate translation units and the original public wrapper ABI.
# The object linker uses exactly these symbol aliases for qualified hot/cold code.
set(m8_corpus_public m8_native_region_step m8_native_region_verify_memory
    m8_native_region_chain_cache_slots m8_native_region_chain
    m8_native_region_ram_exclusives m8_native_region_inline_services
    m8_native_region_event_loop m8_native_region_event_chain)
foreach(kind IN ITEMS hot cold)
    set(aliases "")
    foreach(symbol IN LISTS m8_corpus_public)
        list(APPEND aliases "${symbol}=m8_session_${kind}_${symbol}")
    endforeach()
    set_source_files_properties("${m8_native_corpus_dir}/${kind}.c"
        PROPERTIES COMPILE_DEFINITIONS "${aliases}")
endforeach()
