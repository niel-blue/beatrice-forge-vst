# Forge-specific privacy integration for the bundled VSTGUI targets.
#
# The upstream Windows standalone backend persists preferences in HKCU and the
# upstream file selector allows Windows to add selected paths to Recent
# Documents.  Forge owns its state in either the standalone cfg or the VST host
# stream, so neither platform persistence mechanism is appropriate here.

include_guard(GLOBAL)

function(_beatrice_forge_make_upstream_source_header_only target source)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "Required VSTGUI target does not exist: ${target}")
    endif()
    if(NOT EXISTS "${source}")
        message(FATAL_ERROR "Required VSTGUI source does not exist: ${source}")
    endif()

    # Source properties normally use the directory scope in which the target
    # was created. TARGET_DIRECTORY makes the replacement reliable even though
    # this integration function is called by a parent project.
    set_property(SOURCE "${source}"
        TARGET_DIRECTORY "${target}"
        PROPERTY HEADER_FILE_ONLY TRUE)
endfunction()

function(beatrice_forge_apply_vstgui_file_dialog_privacy source_root)
    if(NOT WIN32)
        return()
    endif()
    if(NOT TARGET vstgui)
        message(FATAL_ERROR "VSTGUI must be configured before applying Forge privacy rules")
    endif()

    get_property(already_applied TARGET vstgui
        PROPERTY BEATRICE_FORGE_FILE_DIALOG_PRIVACY)
    if(already_applied)
        return()
    endif()

    set(upstream_source
        "${source_root}/lib/vst3sdk/vstgui4/vstgui/lib/platform/win32/winfileselector.cpp")
    set(overlay_source
        "${source_root}/src/common/vstgui_file_dialog_privacy_win32.cc")
    if(NOT EXISTS "${overlay_source}")
        message(FATAL_ERROR "Forge file-dialog privacy overlay is missing: ${overlay_source}")
    endif()

    _beatrice_forge_make_upstream_source_header_only(
        vstgui "${upstream_source}")
    target_sources(vstgui PRIVATE "${overlay_source}")
    set_property(TARGET vstgui
        PROPERTY BEATRICE_FORGE_FILE_DIALOG_PRIVACY TRUE)
endfunction()
