# Names are fixed before the generated input exists. The splitter writes only
# changed files, so source and compiler caches survive identical regeneration.
option(M8_WEB_ENTRY_CLONES
    "Clone hot secondary resume entries in the web multi-value split" ON)
function(m8_split_aot target input)
    set(compact_args)
    if(M8_BUILD_WEB AND M8_WEB_COMPACT_TABLE)
        list(APPEND compact_args --compact-spans)
    endif()
    set(splitter "${PROJECT_SOURCE_DIR}/hosts/web/split_aot.py")
    set(splitter_dependencies "${splitter}")
    if(M8_WEB_CODEGEN_V2)
        set(splitter "${PROJECT_SOURCE_DIR}/hosts/web/split_native_codegen_v2.py")
        list(APPEND splitter_dependencies "${splitter}" "${PROJECT_SOURCE_DIR}/aot/codegen_v2.py"
            "${PROJECT_SOURCE_DIR}/aot/codegen_entry.py" "${PROJECT_SOURCE_DIR}/aot/local_flags.py")
    endif()
    if(M8_WEB_GUEST_REGISTER_ABI AND NOT M8_WEB_CODEGEN_V2)
        if(NOT M8_WEB_NATIVE_FP OR M8_AOT_CODEGEN_V2 OR NOT M8_AOT_PROFILE STREQUAL "fast")
            message(FATAL_ERROR "M8_WEB_GUEST_REGISTER_ABI requires native-register FP, fast AOT and Codegen V2 OFF")
        endif()
        set(splitter "${PROJECT_SOURCE_DIR}/tools/web_guest_abi_split.py")
        list(APPEND splitter_dependencies "${splitter}"
            "${PROJECT_SOURCE_DIR}/tools/web_guest_abi.py"
            "${PROJECT_SOURCE_DIR}/tools/web_guest_abi_entry.py"
            "${PROJECT_SOURCE_DIR}/tools/web_guest_abi_return.py"
            "${PROJECT_SOURCE_DIR}/tools/web_entry_clones.py"
            "${PROJECT_SOURCE_DIR}/benchmark/wasm-resume-entry.py"
            "${PROJECT_SOURCE_DIR}/benchmark/wasm-normal-entry.py"
            "${PROJECT_SOURCE_DIR}/aot/catalogs/entry-clones.json"
            "${PROJECT_SOURCE_DIR}/aot/codegen_v2.py")
        if(M8_WEB_GUEST_REGISTER_ABI)
            target_compile_options(${target} PRIVATE -mmultivalue
                "SHELL:-Xclang -target-abi -Xclang experimental-mv")
        endif()
    endif()
    if(M8_WEB_CODEGEN_V2 AND M8_WEB_GUEST_REGISTER_ABI)
        target_compile_options(${target} PRIVATE -mmultivalue
            "SHELL:-Xclang -target-abi -Xclang experimental-mv")
    endif()
    set(entry_clone_args)
    if(M8_BUILD_WEB AND M8_WEB_ENTRY_CLONES AND
            (M8_WEB_GUEST_REGISTER_ABI AND NOT M8_WEB_CODEGEN_V2))
        set(entry_clone_args --entry-clones "${PROJECT_SOURCE_DIR}/aot/catalogs/entry-clones.json")
    endif()
    set(output "${CMAKE_BINARY_DIR}/${target}-split")
    set(sources "${output}/dispatch.c")
    if(M8_BUILD_WEB)
        # Avoid Chromium's phi-input ceiling for dense guest-PC tables.
        # Keep the kernels' jump tables intact.
        set_property(SOURCE "${output}/dispatch.c" TARGET_DIRECTORY ${target}
            APPEND PROPERTY COMPILE_OPTIONS -fno-jump-tables)
    endif()
    math(EXPR last "${M8_AOT_SPLIT_UNITS}-1")
    math(EXPR hot_last "${M8_AOT_SPLIT_UNITS}/3-1")
    foreach(index RANGE 0 ${last})
        if(index LESS 10)
            set(number "0${index}")
        else()
            set(number "${index}")
        endif()
        set(source "${output}/unit-${number}.c")
        list(APPEND sources "${source}")
        set(opt -O2)
        if(index LESS_EQUAL hot_last)
            set(opt -O3)
        endif()
        set_property(SOURCE "${source}" TARGET_DIRECTORY ${target}
            APPEND PROPERTY COMPILE_OPTIONS ${opt})
    endforeach()
    # The guard copier can update included headers without changing a split
    # C unit. Depend on its source inputs too: Ninja scans copied headers
    # before the copier runs, so their old timestamps cannot invalidate the
    # objects reliably in the same build invocation.
    file(GLOB split_headers CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/aot/*.h")
    set_property(SOURCE ${sources} TARGET_DIRECTORY ${target} APPEND PROPERTY
        OBJECT_DEPENDS ${split_headers} "${PROJECT_SOURCE_DIR}/aot/native_aot_guard.py")
    add_custom_command(OUTPUT ${sources}
        BYPRODUCTS "${output}/split_shared.h" "${output}/split_runtime.h"
            "${output}/sources.cmake" "${output}/split.json"
        COMMAND "${M8_AOT_PYTHON}" "${splitter}"
            --input "${input}" --output "${output}"
            --runtime "${PROJECT_SOURCE_DIR}/aot/runtime.h"
            --units "${M8_AOT_SPLIT_UNITS}" --namespace "m8_web_aot"
            ${compact_args} ${entry_clone_args}
        DEPENDS "${input}" "${PROJECT_SOURCE_DIR}/aot/runtime.h"
            "${PROJECT_SOURCE_DIR}/aot/compact_spans.py"
            ${splitter_dependencies}
        COMMENT "Split ${target} into cached hot/cold C units" VERBATIM)
    set_property(SOURCE "${input}" TARGET_DIRECTORY ${target} PROPERTY HEADER_FILE_ONLY TRUE)
    target_sources(${target} PRIVATE ${sources})
    target_include_directories(${target} BEFORE PRIVATE "${output}")
endfunction()
