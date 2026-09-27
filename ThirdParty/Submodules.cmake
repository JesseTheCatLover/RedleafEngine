# ThirdParty/Submodules.cmake

# --- Validate submodules ---

function(redleaf_require_submodule NAME PATH)
    if(NOT EXISTS "${PATH}")
        message(FATAL_ERROR
                "${NAME} submodule is missing.\n"
                "Initialize the repository dependencies with:\n"
                "    git submodule update --init --recursive\n"
                "from the RedleafEngine root directory."
        )
    endif()
endfunction()

redleaf_require_submodule(
        "GLFW"
        "${CMAKE_CURRENT_SOURCE_DIR}/glfw"
)

redleaf_require_submodule(
        "Assimp"
        "${CMAKE_CURRENT_SOURCE_DIR}/assimp"
)

redleaf_require_submodule(
        "GLM"
        "${CMAKE_CURRENT_SOURCE_DIR}/glm"
)

redleaf_require_submodule(
        "ImGui"
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui"
)

redleaf_require_submodule(
        "nlohmann/json"
        "${CMAKE_CURRENT_SOURCE_DIR}/nlohmann_json"
)


# --- GLFW ---

# Redleaf only needs the GLFW library itself.
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL
        "Build GLFW example programs"
        FORCE
)

set(GLFW_BUILD_TESTS OFF CACHE BOOL
        "Build GLFW test programs"
        FORCE
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL
        "Build GLFW documentation"
        FORCE
)

set(GLFW_INSTALL OFF CACHE BOOL
        "Generate GLFW installation target"
        FORCE
)

add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/glfw"
        "${CMAKE_BINARY_DIR}/ThirdParty/glfw"
        EXCLUDE_FROM_ALL
)


# --- Assimp ---

# Only build the importers Redleaf currently supports.
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL
        "Build all Assimp importers by default"
        FORCE
)

set(ASSIMP_NO_EXPORT ON CACHE BOOL
        "Disable Assimp exporters"
        FORCE
)

set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL
        "Build Assimp tools"
        FORCE
)

set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL
        "Build Assimp samples"
        FORCE
)

set(ASSIMP_BUILD_TESTS OFF CACHE BOOL
        "Build Assimp tests"
        FORCE
)

set(ASSIMP_INSTALL OFF CACHE BOOL
        "Disable Assimp installation"
        FORCE
)

set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL
        "Treat Assimp warnings as errors"
        FORCE
)

set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL
        "Build Assimp OBJ importer"
        FORCE
)

set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL
        "Build Assimp FBX importer"
        FORCE
)

set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL
        "Build Assimp glTF importer"
        FORCE
)

add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/assimp"
        "${CMAKE_BINARY_DIR}/ThirdParty/assimp"
        EXCLUDE_FROM_ALL
)


# --- GLM ---

set(GLM_BUILD_LIBRARY OFF CACHE BOOL
        "Build GLM library"
        FORCE
)

set(GLM_BUILD_TESTS OFF CACHE BOOL
        "Build GLM tests"
        FORCE
)

set(GLM_BUILD_INSTALL OFF CACHE BOOL
        "Generate GLM install target"
        FORCE
)

add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/glm"
        "${CMAKE_BINARY_DIR}/ThirdParty/glm"
        EXCLUDE_FROM_ALL
)


# --- ImGui ---

set(IMGUI_SRC
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_draw.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_tables.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_widgets.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui/misc/cpp/imgui_stdlib.cpp"

        # Redleaf currently uses the GLFW and OpenGL3 backends.
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_glfw.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_opengl3.cpp"
)

add_library(imgui STATIC
        ${IMGUI_SRC}
)

target_include_directories(imgui
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui"
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends"
)

target_link_libraries(imgui PUBLIC
        glfw
)


# --- nlohmann/json ---

set(JSON_BuildTests OFF CACHE BOOL
        "Build nlohmann/json tests"
        FORCE
)

set(JSON_Install OFF CACHE BOOL
        "Install nlohmann/json"
        FORCE
)

add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/nlohmann_json"
        "${CMAKE_BINARY_DIR}/ThirdParty/nlohmann_json"
        EXCLUDE_FROM_ALL
)


# --- IDE organization ---

set_target_properties(
        imgui
        PROPERTIES
        EXCLUDE_FROM_ALL TRUE
        FOLDER "ThirdParty"
)