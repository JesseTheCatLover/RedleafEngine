# Tools/AssetDistribution/AssetDistribution.cmake

include_guard(GLOBAL)


# --- Paths ---

set(REDLEAF_ASSET_DISTRIBUTION_CONFIG
        "${CMAKE_CURRENT_LIST_DIR}/configuration.json"
)

set(REDLEAF_ASSET_DISTRIBUTION_SCRIPT
        "${CMAKE_CURRENT_LIST_DIR}/sync_assets.py"
)

set(REDLEAF_ASSET_DISTRIBUTION_STATE_DIR
        "${CMAKE_SOURCE_DIR}/.redleaf"
)

set(REDLEAF_ASSET_DISTRIBUTION_LOCK
        "${REDLEAF_ASSET_DISTRIBUTION_STATE_DIR}/assets-sync.lock"
)

set(REDLEAF_ASSET_DISTRIBUTION_MANIFEST
        "${REDLEAF_ASSET_DISTRIBUTION_STATE_DIR}/assets-manifest.json"
)

set(REDLEAF_ASSET_DISTRIBUTION_STATE
        "${REDLEAF_ASSET_DISTRIBUTION_STATE_DIR}/assets-state.json"
)

set(REDLEAF_ASSET_DISTRIBUTION_PLAN
        "${REDLEAF_ASSET_DISTRIBUTION_STATE_DIR}/assets-plan.json"
)

set(REDLEAF_ASSET_DISTRIBUTION_CACHE_DIR
        "${REDLEAF_ASSET_DISTRIBUTION_STATE_DIR}/assets-cache"
)

set(REDLEAF_ASSET_DISTRIBUTION_MANIFEST_DOWNLOAD
        "${REDLEAF_ASSET_DISTRIBUTION_STATE_DIR}/.assets-manifest.json.download"
)


# --- Constants ---

set(REDLEAF_ASSET_DISTRIBUTION_FORMAT_VERSION 1)
set(REDLEAF_ASSET_MANIFEST_FORMAT_VERSION 1)

set(REDLEAF_ASSET_DISTRIBUTION_TIMEOUT 120)


# --- Configuration ---

function(redleaf_asset_distribution_load_repository OUTPUT_VARIABLE)
    if(NOT EXISTS "${REDLEAF_ASSET_DISTRIBUTION_CONFIG}")
        message(FATAL_ERROR
                "Redleaf Asset Distribution configuration was not found:\n"
                "  ${REDLEAF_ASSET_DISTRIBUTION_CONFIG}"
        )
    endif()

    file(
            READ
            "${REDLEAF_ASSET_DISTRIBUTION_CONFIG}"
            CONFIGURATION_JSON
    )

    string(
            JSON
            REPOSITORY
            ERROR_VARIABLE JSON_ERROR
            GET
            "${CONFIGURATION_JSON}"
            repository
    )

    if(NOT JSON_ERROR STREQUAL "NOTFOUND")
        message(FATAL_ERROR
                "Failed to read 'repository' from:\n"
                "  ${REDLEAF_ASSET_DISTRIBUTION_CONFIG}\n"
                "Error: ${JSON_ERROR}"
        )
    endif()

    if(REPOSITORY STREQUAL "")
        message(FATAL_ERROR
                "Redleaf Asset Distribution repository is empty."
        )
    endif()

    set(
            ${OUTPUT_VARIABLE}
            "${REPOSITORY}"
            PARENT_SCOPE
    )
endfunction()


# --- Manifest ---

function(redleaf_asset_distribution_manifest_is_current OUTPUT_VARIABLE)
    set(
            MANIFEST_IS_CURRENT
            FALSE
    )

    if(EXISTS "${REDLEAF_ASSET_DISTRIBUTION_MANIFEST}")
        file(
                READ
                "${REDLEAF_ASSET_DISTRIBUTION_MANIFEST}"
                MANIFEST_JSON
        )

        string(
                JSON
                MANIFEST_FORMAT_VERSION
                ERROR_VARIABLE FORMAT_ERROR
                GET
                "${MANIFEST_JSON}"
                format_version
        )

        string(
                JSON
                MANIFEST_PACKAGE_VERSION
                ERROR_VARIABLE VERSION_ERROR
                GET
                "${MANIFEST_JSON}"
                package_version
        )

        string(
                JSON
                MANIFEST_FILES_TYPE
                ERROR_VARIABLE FILES_ERROR
                TYPE
                "${MANIFEST_JSON}"
                files
        )

        if(
                FORMAT_ERROR STREQUAL "NOTFOUND"
                AND
                VERSION_ERROR STREQUAL "NOTFOUND"
                AND
                FILES_ERROR STREQUAL "NOTFOUND"
                AND
                MANIFEST_FORMAT_VERSION EQUAL REDLEAF_ASSET_MANIFEST_FORMAT_VERSION
                AND
                MANIFEST_PACKAGE_VERSION STREQUAL REDLEAF_ASSETS_VERSION
                AND
                MANIFEST_FILES_TYPE STREQUAL "OBJECT"
        )
            set(
                    MANIFEST_IS_CURRENT
                    TRUE
            )
        endif()
    endif()

    set(
            ${OUTPUT_VARIABLE}
            "${MANIFEST_IS_CURRENT}"
            PARENT_SCOPE
    )
endfunction()


function(redleaf_asset_distribution_prepare_manifest OUTPUT_PATH)
    redleaf_asset_distribution_manifest_is_current(
            MANIFEST_IS_CURRENT
    )

    if(MANIFEST_IS_CURRENT)
        message(STATUS
                "  Using local asset manifest."
        )

        set(
                ${OUTPUT_PATH}
                "${REDLEAF_ASSET_DISTRIBUTION_MANIFEST}"
                PARENT_SCOPE
        )

        return()
    endif()

    redleaf_asset_distribution_load_repository(
            REDLEAF_ASSET_REPOSITORY
    )

    set(
            MANIFEST_URL
            "https://github.com/${REDLEAF_ASSET_REPOSITORY}/releases/download/assets-${REDLEAF_ASSETS_VERSION}/manifest.json"
    )

    message(STATUS
            "Redleaf Asset Distribution:"
    )

    message(STATUS
            "  Repository: ${REDLEAF_ASSET_REPOSITORY}"
    )

    message(STATUS
            "  Version:    ${REDLEAF_ASSETS_VERSION}"
    )

    message(STATUS
            "  Manifest:   ${MANIFEST_URL}"
    )

    file(
            MAKE_DIRECTORY
            "${REDLEAF_ASSET_DISTRIBUTION_STATE_DIR}"
    )

    file(
            REMOVE
            "${REDLEAF_ASSET_DISTRIBUTION_MANIFEST_DOWNLOAD}"
    )

    file(
            DOWNLOAD
            "${MANIFEST_URL}"
            "${REDLEAF_ASSET_DISTRIBUTION_MANIFEST_DOWNLOAD}"
            TLS_VERIFY ON
            TLS_VERSION 1.2
            TIMEOUT ${REDLEAF_ASSET_DISTRIBUTION_TIMEOUT}
            STATUS DOWNLOAD_STATUS
            LOG DOWNLOAD_LOG
    )

    list(
            GET
            DOWNLOAD_STATUS
            0
            DOWNLOAD_CODE
    )

    if(NOT DOWNLOAD_CODE EQUAL 0)
        list(
                GET
                DOWNLOAD_STATUS
                1
                DOWNLOAD_MESSAGE
        )

        file(
                REMOVE
                "${REDLEAF_ASSET_DISTRIBUTION_MANIFEST_DOWNLOAD}"
        )

        message(FATAL_ERROR
                "Failed to download Redleaf asset manifest.\n"
                "  URL: ${MANIFEST_URL}\n"
                "  Error: ${DOWNLOAD_MESSAGE}\n"
                "  Log: ${DOWNLOAD_LOG}"
        )
    endif()

    if(NOT EXISTS "${REDLEAF_ASSET_DISTRIBUTION_MANIFEST_DOWNLOAD}")
        message(FATAL_ERROR
                "Manifest download reported success, "
                "but the manifest file was not created."
        )
    endif()

    file(
            RENAME
            "${REDLEAF_ASSET_DISTRIBUTION_MANIFEST_DOWNLOAD}"
            "${REDLEAF_ASSET_DISTRIBUTION_MANIFEST}"
    )

    if(NOT EXISTS "${REDLEAF_ASSET_DISTRIBUTION_MANIFEST}")
        message(FATAL_ERROR
                "Failed to finalize the downloaded asset manifest."
        )
    endif()

    set(
            ${OUTPUT_PATH}
            "${REDLEAF_ASSET_DISTRIBUTION_MANIFEST}"
            PARENT_SCOPE
    )
endfunction()


# --- Python ---

function(redleaf_asset_distribution_generate_plan MANIFEST_PATH)
    execute_process(
            COMMAND
            "${Python3_EXECUTABLE}"
            "${REDLEAF_ASSET_DISTRIBUTION_SCRIPT}"
            plan
            --manifest "${MANIFEST_PATH}"
            --state "${REDLEAF_ASSET_DISTRIBUTION_STATE}"
            --project-root "${CMAKE_SOURCE_DIR}"
            --output "${REDLEAF_ASSET_DISTRIBUTION_PLAN}"
            --version "${REDLEAF_ASSETS_VERSION}"

            WORKING_DIRECTORY
            "${CMAKE_SOURCE_DIR}"

            RESULT_VARIABLE
            PYTHON_RESULT

            OUTPUT_VARIABLE
            PYTHON_OUTPUT

            ERROR_VARIABLE
            PYTHON_ERROR
    )

    if(NOT PYTHON_OUTPUT STREQUAL "")
        message(STATUS
                "${PYTHON_OUTPUT}"
        )
    endif()

    if(NOT PYTHON_RESULT EQUAL 0)
        message(FATAL_ERROR
                "Failed to generate Redleaf asset download plan.\n"
                "${PYTHON_ERROR}"
        )
    endif()

    if(NOT EXISTS "${REDLEAF_ASSET_DISTRIBUTION_PLAN}")
        message(FATAL_ERROR
                "Asset download plan was not created:\n"
                "  ${REDLEAF_ASSET_DISTRIBUTION_PLAN}"
        )
    endif()
endfunction()


# --- Blob downloads ---

function(redleaf_asset_distribution_download_blobs)
    if(NOT EXISTS "${REDLEAF_ASSET_DISTRIBUTION_PLAN}")
        message(FATAL_ERROR
                "Asset download plan does not exist."
        )
    endif()

    redleaf_asset_distribution_load_repository(
            REDLEAF_ASSET_REPOSITORY
    )

    file(
            READ
            "${REDLEAF_ASSET_DISTRIBUTION_PLAN}"
            PLAN_JSON
    )

    string(
            JSON
            PLAN_PACKAGE_VERSION
            ERROR_VARIABLE JSON_ERROR
            GET
            "${PLAN_JSON}"
            package_version
    )

    if(
            NOT JSON_ERROR STREQUAL "NOTFOUND"
            OR
            NOT PLAN_PACKAGE_VERSION STREQUAL REDLEAF_ASSETS_VERSION
    )
        message(FATAL_ERROR
                "Asset plan package version mismatch."
        )
    endif()

    string(
            JSON
            DOWNLOAD_COUNT
            ERROR_VARIABLE JSON_ERROR
            LENGTH
            "${PLAN_JSON}"
            downloads
    )

    if(NOT JSON_ERROR STREQUAL "NOTFOUND")
        message(FATAL_ERROR
                "Failed to read asset download plan.\n"
                "Error: ${JSON_ERROR}"
        )
    endif()

    file(
            MAKE_DIRECTORY
            "${REDLEAF_ASSET_DISTRIBUTION_CACHE_DIR}"
    )

    if(DOWNLOAD_COUNT EQUAL 0)
        message(STATUS
                "  No asset downloads required."
        )
        return()
    endif()

    math(
            EXPR
            LAST_INDEX
            "${DOWNLOAD_COUNT} - 1"
    )

    foreach(INDEX RANGE 0 ${LAST_INDEX})
        string(
                JSON
                SHA256
                ERROR_VARIABLE JSON_ERROR
                GET
                "${PLAN_JSON}"
                downloads
                ${INDEX}
                sha256
        )

        if(NOT JSON_ERROR STREQUAL "NOTFOUND")
            message(FATAL_ERROR
                    "Failed to read asset download SHA-256."
            )
        endif()

        string(
                JSON
                SIZE
                ERROR_VARIABLE JSON_ERROR
                GET
                "${PLAN_JSON}"
                downloads
                ${INDEX}
                size
        )

        if(NOT JSON_ERROR STREQUAL "NOTFOUND")
            message(FATAL_ERROR
                    "Failed to read asset download size."
            )
        endif()

        set(
                CACHE_PATH
                "${REDLEAF_ASSET_DISTRIBUTION_CACHE_DIR}/blob-${SHA256}"
        )

        set(
                CACHE_VALID
                FALSE
        )

        if(EXISTS "${CACHE_PATH}")
            if(IS_SYMLINK "${CACHE_PATH}" OR IS_DIRECTORY "${CACHE_PATH}")
                file(
                        REMOVE
                        "${CACHE_PATH}"
                )
            else()
                file(
                        SIZE
                        "${CACHE_PATH}"
                        CACHE_SIZE
                )

                if(CACHE_SIZE EQUAL SIZE)
                    file(
                            SHA256
                            "${CACHE_PATH}"
                            CACHE_HASH
                    )

                    if(CACHE_HASH STREQUAL SHA256)
                        set(
                                CACHE_VALID
                                TRUE
                        )
                    else()
                        file(
                                REMOVE
                                "${CACHE_PATH}"
                        )
                    endif()
                else()
                    file(
                            REMOVE
                            "${CACHE_PATH}"
                    )
                endif()
            endif()
        endif()

        if(CACHE_VALID)
            message(STATUS
                    "  Using cached blob: ${SHA256}"
            )
            continue()
        endif()

        set(
                BLOB_URL
                "https://github.com/${REDLEAF_ASSET_REPOSITORY}/releases/download/assets-${REDLEAF_ASSETS_VERSION}/blob-${SHA256}"
        )

        set(
                CACHE_TEMPORARY
                "${CACHE_PATH}.download"
        )

        file(
                REMOVE
                "${CACHE_TEMPORARY}"
        )

        message(STATUS
                "  Downloading blob: ${SHA256}"
        )

        file(
                DOWNLOAD
                "${BLOB_URL}"
                "${CACHE_TEMPORARY}"
                EXPECTED_HASH
                "SHA256=${SHA256}"
                TLS_VERIFY ON
                TLS_VERSION 1.2
                TIMEOUT ${REDLEAF_ASSET_DISTRIBUTION_TIMEOUT}
                STATUS DOWNLOAD_STATUS
                LOG DOWNLOAD_LOG
        )

        list(
                GET
                DOWNLOAD_STATUS
                0
                DOWNLOAD_CODE
        )

        if(NOT DOWNLOAD_CODE EQUAL 0)
            list(
                    GET
                    DOWNLOAD_STATUS
                    1
                    DOWNLOAD_MESSAGE
            )

            file(
                    REMOVE
                    "${CACHE_TEMPORARY}"
            )

            message(FATAL_ERROR
                    "Failed to download Redleaf asset blob.\n"
                    "  URL:  ${BLOB_URL}\n"
                    "  Hash: ${SHA256}\n"
                    "  Error: ${DOWNLOAD_MESSAGE}\n"
                    "  Log: ${DOWNLOAD_LOG}"
            )
        endif()

        file(
                RENAME
                "${CACHE_TEMPORARY}"
                "${CACHE_PATH}"
        )

        if(NOT EXISTS "${CACHE_PATH}")
            message(FATAL_ERROR
                    "Downloaded asset blob could not be stored in the cache.\n"
                    "  Blob: ${SHA256}"
            )
        endif()

        file(
                SIZE
                "${CACHE_PATH}"
                DOWNLOADED_SIZE
        )

        if(NOT DOWNLOADED_SIZE EQUAL SIZE)
            file(
                    REMOVE
                    "${CACHE_PATH}"
            )

            message(FATAL_ERROR
                    "Asset blob size mismatch.\n"
                    "  Blob:     ${SHA256}\n"
                    "  Expected: ${SIZE}\n"
                    "  Received: ${DOWNLOADED_SIZE}"
            )
        endif()
    endforeach()
endfunction()


# --- Finalization ---

function(redleaf_asset_distribution_finalize MANIFEST_PATH)
    execute_process(
            COMMAND
            "${Python3_EXECUTABLE}"
            "${REDLEAF_ASSET_DISTRIBUTION_SCRIPT}"
            finalize
            --manifest "${MANIFEST_PATH}"
            --state "${REDLEAF_ASSET_DISTRIBUTION_STATE}"
            --plan "${REDLEAF_ASSET_DISTRIBUTION_PLAN}"
            --cache-dir "${REDLEAF_ASSET_DISTRIBUTION_CACHE_DIR}"
            --project-root "${CMAKE_SOURCE_DIR}"
            --version "${REDLEAF_ASSETS_VERSION}"

            WORKING_DIRECTORY
            "${CMAKE_SOURCE_DIR}"

            RESULT_VARIABLE
            PYTHON_RESULT

            OUTPUT_VARIABLE
            PYTHON_OUTPUT

            ERROR_VARIABLE
            PYTHON_ERROR
    )

    if(NOT PYTHON_OUTPUT STREQUAL "")
        message(STATUS
                "${PYTHON_OUTPUT}"
        )
    endif()

    if(NOT PYTHON_RESULT EQUAL 0)
        message(FATAL_ERROR
                "Failed to finalize Redleaf asset synchronization.\n"
                "${PYTHON_ERROR}"
        )
    endif()
endfunction()


# --- Public API ---

function(redleaf_sync_asset_distribution)
    if(
            NOT DEFINED REDLEAF_ASSETS_VERSION
            OR
            REDLEAF_ASSETS_VERSION STREQUAL ""
    )
        message(FATAL_ERROR
                "REDLEAF_ASSETS_VERSION is not defined."
        )
    endif()

    file(
            MAKE_DIRECTORY
            "${REDLEAF_ASSET_DISTRIBUTION_STATE_DIR}"
    )

    file(
            LOCK
            "${REDLEAF_ASSET_DISTRIBUTION_LOCK}"
            GUARD FUNCTION
            TIMEOUT 300
            RESULT_VARIABLE LOCK_RESULT
    )

    if(NOT LOCK_RESULT EQUAL 0)
        message(FATAL_ERROR
                "Failed to acquire Redleaf asset distribution lock.\n"
                "  Lock: ${REDLEAF_ASSET_DISTRIBUTION_LOCK}\n"
                "  Error: ${LOCK_RESULT}"
        )
    endif()

    redleaf_asset_distribution_prepare_manifest(
            MANIFEST_PATH
    )

    redleaf_asset_distribution_generate_plan(
            "${MANIFEST_PATH}"
    )

    redleaf_asset_distribution_download_blobs()

    redleaf_asset_distribution_finalize(
            "${MANIFEST_PATH}"
    )

    file(
            REMOVE
            "${REDLEAF_ASSET_DISTRIBUTION_PLAN}"
    )

    message(STATUS
            "  Asset distribution synchronized."
    )
endfunction()