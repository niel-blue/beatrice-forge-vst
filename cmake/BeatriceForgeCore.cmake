# Shared host-independent processing target used by both Forge products.

include_guard(GLOBAL)

function(beatrice_forge_add_core target_name source_root common_root
         beatrice_library_path)
    set(common_source_dir "${common_root}/common")
    if(NOT EXISTS "${common_source_dir}/parameter_schema.h")
        message(FATAL_ERROR
            "Common root must contain common/parameter_schema.h: ${common_root}")
    endif()
    if(NOT EXISTS "${beatrice_library_path}")
        message(FATAL_ERROR
            "The permitted Beatrice processing library was not found: ${beatrice_library_path}")
    endif()

    if(NOT TARGET beatricelib)
        add_library(beatricelib STATIC IMPORTED)
        set_property(TARGET beatricelib PROPERTY IMPORTED_LOCATION
                     "${beatrice_library_path}")
    endif()

    add_library(${target_name} STATIC
        "${common_source_dir}/audio_engine.cc"
        "${common_source_dir}/audio_recorder.cc"
        "${common_source_dir}/recording_paths.cc"
        "${common_source_dir}/wasapi_device_catalog.cc"
        "${common_source_dir}/parameter_schema.cc"
        "${common_source_dir}/parameter_state.cc"
        "${common_source_dir}/preset.cc"
        "${common_source_dir}/output_effects.cc"
        "${common_source_dir}/processor_core_0.cc"
        "${common_source_dir}/processor_core_1.cc"
        "${common_source_dir}/processor_core_2.cc"
        "${common_source_dir}/processor_proxy.cc"
        "${common_source_dir}/voice_morph_parameter.cc"
    )
    set_target_properties(${target_name} PROPERTIES
        POSITION_INDEPENDENT_CODE ON)
    target_include_directories(${target_name}
        PUBLIC "${common_root}"
        PUBLIC "${source_root}/src"
        PUBLIC "${source_root}/lib"
    )
    target_link_libraries(${target_name} PUBLIC beatricelib)
    if(WIN32)
        target_link_libraries(${target_name} PUBLIC
            ole32 propsys shell32 avrt)
    endif()
endfunction()
