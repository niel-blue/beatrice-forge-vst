cmake_minimum_required(VERSION 3.25)
set(CMAKE_CXX_STANDARD 20)

set(FORGE_VERSION_MAJOR 0)
set(FORGE_VERSION_MINOR 9)
set(FORGE_VERSION_PATCH 0)
set(FORGE_VERSION "${FORGE_VERSION_MAJOR}.${FORGE_VERSION_MINOR}.${FORGE_VERSION_PATCH}")
# This identifies the upstream Beatrice build used as the processing base. It
# is provenance information, not the Beatrice Forge product version.
set(BEATRICE_BASE_VERSION "2.0.0-rc.3")
set(BEATRICE_PRODUCT_NAME "Beatrice Forge")
set(BEATRICE_PRODUCT_SLUG "beatrice_forge")
set(BEATRICE_AUTHOR_NAME "Niel")
execute_process(
    COMMAND git rev-parse --short HEAD
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
math(EXPR build_number 0x${GIT_HASH})
message(STATUS "build_number: ${build_number}")

set(BEATRICE_PROCESSOR_UID "0x7a417351, 0x80d847b2, 0xbf4bfca8, 0x1183e747" CACHE STRING "Beatrice Forge VST3 processor UUID")
set(BEATRICE_CONTROLLER_UID "0xbabcd3fb, 0x14564a2a, 0x84b71b24, 0xf6c5e661" CACHE STRING "Beatrice Forge VST3 controller UUID")
message(STATUS "BEATRICE_PROCESSOR_UID: ${BEATRICE_PROCESSOR_UID}")
message(STATUS "BEATRICE_CONTROLLER_UID: ${BEATRICE_CONTROLLER_UID}")

project(beatrice_forge VERSION ${FORGE_VERSION_MAJOR}.${FORGE_VERSION_MINOR}.${FORGE_VERSION_PATCH})
option(BEATRICE_DEV_VERSION "Development version" ON)
if(BEATRICE_DEV_VERSION)
    set(FORGE_FULL_VERSION_STR ${FORGE_VERSION}-dev.${GIT_HASH})
else()
    set(FORGE_FULL_VERSION_STR ${FORGE_VERSION})
endif()
set(target ${BEATRICE_PRODUCT_SLUG}_${FORGE_FULL_VERSION_STR})

add_compile_options("$<$<C_COMPILER_ID:MSVC>:/MT>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/MT>")
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/arch:AVX2>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/arch:AVX2>")

set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES 1)
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES 0)
set(SMTG_ENABLE_VSTGUI_SUPPORT 1)
if(MSVC)
    # The prebuilt processor library uses the static CRT.  Ask the SDK to
    # use the same CRT for its helper libraries and standalone targets.
    set(SMTG_USE_STATIC_CRT ON CACHE BOOL
        "Use the static MSVC runtime for the Beatrice build" FORCE)
endif()

set(vst3sdk_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/vst3sdk)

# The common processing layer can live in a separate local repository while
# the VST and standalone clients are being developed together.  The default
# keeps the current checkout layout; a sibling shared checkout can be used by
# passing -DBEATRICE_COMMON_ROOT=<path containing common/>.
set(BEATRICE_COMMON_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src"
    CACHE PATH "Root directory containing the shared common/ sources")
set(BEATRICE_COMMON_SOURCE_DIR "${BEATRICE_COMMON_ROOT}/common")
if(NOT EXISTS "${BEATRICE_COMMON_SOURCE_DIR}/parameter_schema.h")
    message(FATAL_ERROR
        "BEATRICE_COMMON_ROOT must contain common/parameter_schema.h: ${BEATRICE_COMMON_ROOT}")
endif()

add_subdirectory(lib/vst3sdk)

smtg_enable_vst3_sdk()

if(SMTG_MAC)
    target_compile_options(vstgui PRIVATE -Wno-error=deprecated-declarations)
endif()

configure_file(src/vst/metadata.h.in src/vst/metadata.h @ONLY)
configure_file(resource/Info.plist.in resource/Info.plist @ONLY)
set(SMTG_PACKAGE_ICON_PATH ${CMAKE_CURRENT_SOURCE_DIR}/resource/icon.ico)

add_library(beatricelib STATIC IMPORTED)
if(SMTG_WIN)
    set(BEATRICE_LIBRARY_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib/beatricelib/beatrice.lib)
elseif(SMTG_MAC)
    set(BEATRICE_LIBRARY_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib/beatricelib/libbeatrice.a)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()
set_property(
    TARGET beatricelib
    PROPERTY IMPORTED_LOCATION ${BEATRICE_LIBRARY_PATH}
)

# Host-independent processing and state used by the VST.
set(BEATRICE_COMMON_SOURCES
    ${BEATRICE_COMMON_SOURCE_DIR}/audio_engine.cc
    ${BEATRICE_COMMON_SOURCE_DIR}/audio_recorder.cc
    ${BEATRICE_COMMON_SOURCE_DIR}/recording_paths.cc
    ${BEATRICE_COMMON_SOURCE_DIR}/wasapi_device_catalog.cc
    ${BEATRICE_COMMON_SOURCE_DIR}/parameter_schema.cc
    ${BEATRICE_COMMON_SOURCE_DIR}/parameter_state.cc
    ${BEATRICE_COMMON_SOURCE_DIR}/preset.cc
    ${BEATRICE_COMMON_SOURCE_DIR}/output_effects.cc
    ${BEATRICE_COMMON_SOURCE_DIR}/processor_core_0.cc
    ${BEATRICE_COMMON_SOURCE_DIR}/processor_core_1.cc
    ${BEATRICE_COMMON_SOURCE_DIR}/processor_core_2.cc
    ${BEATRICE_COMMON_SOURCE_DIR}/processor_proxy.cc
    ${BEATRICE_COMMON_SOURCE_DIR}/voice_morph_parameter.cc
)
add_library(beatrice_core STATIC
    ${BEATRICE_COMMON_SOURCES}
)
add_library(beatrice::core ALIAS beatrice_core)
set_target_properties(beatrice_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(beatrice_core
    PUBLIC ${BEATRICE_COMMON_ROOT}
    PUBLIC src
    PUBLIC lib
)
target_link_libraries(beatrice_core PUBLIC beatricelib)
if(SMTG_WIN)
    target_link_libraries(beatrice_core PUBLIC ole32 propsys shell32 avrt)
endif()

smtg_add_vst3plugin(${target}
    src/ui/control_help_locale.cc
    src/vst/controller.cc
    src/vst/description_text_layout.cc
    src/vst/description_url.cc
    src/vst/editor.cc
    src/vst/editor_description.cc
    src/vst/editor_morph_controller.cc
    src/vst/factory.cc
    src/vst/parameter.cc
    src/vst/processor.cc
    src/vst/direct_wasapi_output.cc
)

target_include_directories(${target}
    PRIVATE src
    PRIVATE lib
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/src/vst
)

target_link_libraries(${target}
    PRIVATE beatrice::core
    PRIVATE sdk
    PRIVATE vstgui_support
)

if(SMTG_MAC)
    find_library(CORE_TEXT_FRAMEWORK CoreText REQUIRED)
    target_link_libraries(${target} PRIVATE ${CORE_TEXT_FRAMEWORK})
    set(platform mac)
    smtg_target_add_plugin_resource(${target} resource/logo.png
        resource/logo_forge.png)
    smtg_target_set_bundle(${target}
        BUNDLE_IDENTIFIER "com.niel.beatriceforge.vst3"
        INFOPLIST "${CMAKE_CURRENT_BINARY_DIR}/resource/Info.plist" PREPROCESS
    )
elseif(SMTG_WIN)
    set(platform win)
    target_sources(${target}
        PRIVATE resource/beatrice.rc
    )
endif()

# ----------------------------------------------------------------

set(ARTIFACT_ROOT ${CMAKE_CURRENT_BINARY_DIR}/dist)
file(MAKE_DIRECTORY ${ARTIFACT_ROOT})
get_target_property(PLUGIN_PACKAGE_PATH ${target} SMTG_PLUGIN_PACKAGE_PATH)

set(PARAPHERNALIA_COMMANDS)
if(NOT BEATRICE_DEV_VERSION)
    set(PARAPHERNALIA_DIR ${CMAKE_CURRENT_BINARY_DIR}/beatrice_paraphernalia_jvs)
    file(MAKE_DIRECTORY ${PARAPHERNALIA_DIR})
    set(PARAPHERNALIA_BASE_URL "https://huggingface.co/fierce-cats/beatrice-2.0.0-alpha/resolve/rc.0/rc.0/beatrice_paraphernalia_jvs_156_03000000")
    set(PARAPHERNALIA_FILES
        beatrice_paraphernalia_jvs.toml
        phone_extractor.bin
        pitch_estimator.bin
        speaker_embeddings.bin
        waveform_generator.bin
        embedding_setter.bin
        noimage.png
    )
    foreach(file ${PARAPHERNALIA_FILES})
        file(DOWNLOAD ${PARAPHERNALIA_BASE_URL}/${file} ${PARAPHERNALIA_DIR}/${file})
    endforeach()
    list(APPEND PARAPHERNALIA_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PARAPHERNALIA_DIR} ${target}/beatrice_paraphernalia_jvs
    )
endif()

add_custom_target(distribution
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${target}
    COMMAND ${CMAKE_COMMAND} -E rm -f ${target}_${platform}.zip
    COMMAND ${CMAKE_COMMAND} -E make_directory ${target}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PLUGIN_PACKAGE_PATH} ${target}/${target}.vst3
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt ${target}
    ${PARAPHERNALIA_COMMANDS}
    COMMAND ${CMAKE_COMMAND} -E tar cfv ${target}_${platform}.zip --format=zip ${target}
    WORKING_DIRECTORY ${ARTIFACT_ROOT}
)
add_dependencies(distribution ${target})
