# Beatrice Forge product version shared by the VST and standalone products.
#
# This file is the single local source of truth for product versioning.  The
# upstream Beatrice version is provenance only and must not be used as the
# Forge product version.

include_guard(GLOBAL)

set(FORGE_VERSION_MAJOR 0)
set(FORGE_VERSION_MINOR 9)
set(FORGE_VERSION_PATCH 0)
set(FORGE_VERSION
    "${FORGE_VERSION_MAJOR}.${FORGE_VERSION_MINOR}.${FORGE_VERSION_PATCH}")

set(BEATRICE_BASE_VERSION "2.0.0-rc.3")
set(BEATRICE_PRODUCT_NAME "Beatrice Forge")
set(BEATRICE_PRODUCT_SLUG "beatrice_forge")
set(BEATRICE_AUTHOR_NAME "Niel")

# Resolves the revision suffix in exactly the same way for both products.
# BEATRICE_DEV_VERSION must be declared by the caller before invoking this
# function.  Release builds retain only FORGE_VERSION (for example 0.9.0).
function(beatrice_forge_resolve_build_version source_root)
    execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY "${source_root}"
        RESULT_VARIABLE git_result
        OUTPUT_VARIABLE resolved_git_hash
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    if(NOT git_result EQUAL 0 OR
       NOT resolved_git_hash MATCHES "^[0-9A-Fa-f]+$")
        # Source archives may not contain .git.  Keep the build deterministic
        # and visibly non-release instead of failing during configure.
        set(resolved_git_hash "0000000")
    endif()

    string(SUBSTRING "${resolved_git_hash}" 0 7 resolved_git_hash)
    math(EXPR resolved_build_number "0x${resolved_git_hash}")

    if(BEATRICE_DEV_VERSION)
        set(resolved_full_version
            "${FORGE_VERSION}-dev.${resolved_git_hash}")
    else()
        set(resolved_full_version "${FORGE_VERSION}")
    endif()

    set(GIT_HASH "${resolved_git_hash}" PARENT_SCOPE)
    set(build_number "${resolved_build_number}" PARENT_SCOPE)
    set(FORGE_FULL_VERSION_STR "${resolved_full_version}" PARENT_SCOPE)
endfunction()
