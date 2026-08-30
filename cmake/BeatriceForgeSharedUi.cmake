# Source files currently used by both the VST and standalone front ends.
#
# Some files still live under src/vst for historical reasons.  Keeping their
# list here prevents either product build from silently omitting a shared UI
# fix while that physical layout is being untangled.

include_guard(GLOBAL)

function(beatrice_forge_collect_shared_ui_sources output_variable source_root)
    set(shared_ui_sources
        "${source_root}/src/ui/control_help_locale.cc"
        "${source_root}/src/vst/controller.cc"
        "${source_root}/src/vst/description_text_layout.cc"
        "${source_root}/src/vst/description_url.cc"
        "${source_root}/src/vst/editor.cc"
        "${source_root}/src/vst/editor_description.cc"
        "${source_root}/src/vst/editor_morph_controller.cc"
        "${source_root}/src/vst/parameter.cc"
    )
    set(${output_variable} "${shared_ui_sources}" PARENT_SCOPE)
endfunction()
