cmake_minimum_required(VERSION 3.30)
project(RedleafEngine VERSION 0.1 LANGUAGES C CXX)
if(APPLE)
    enable_language(OBJC)
endif()

set(REDLEAF_DOC_VERSION "Alpha")
set(REDLEAF_ASSETS_VERSION "0.1.0")

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# -- Include build utilities ---
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/BuildSystem")

include(RedleafOptions)
include(RedleafPlatform)
include(RedleafUtilities)
include(RedleafExecutable)
include(RedleafReflection)
include(RedleafModule)


# --- ThirdParty libraries ---
add_subdirectory(ThirdParty)

# --- Tools ---
add_subdirectory(Tools/JReflectGen) # (must come BEFORE Engine so JREFLECTGEN_EXE is exported)

# --- Engine modules ---
add_subdirectory(Source/Engine)

if(JENGINE_WITH_SOURCE_ASSETS) # Engine configuration options
    target_compile_definitions(Engine PUBLIC JENGINE_WITH_SOURCE_ASSETS=1)
endif()

add_subdirectory(Source/EditorRuntime)
add_subdirectory(Source/Editor)
add_subdirectory(Source/Launch) # Executable is defined here

# ---------------- Engine Globals ----------------
target_compile_definitions(
        Engine
        PUBLIC
        ENGINE_DIRECTORY="$<SHELL_PATH:${CMAKE_SOURCE_DIR}>"
)
# ---------------- Folder organization ----------------
redleaf_set_folder(JApplication "EngineLibs")
redleaf_set_folder(Engine "EngineLibs")
redleaf_set_folder(EditorRuntime "EngineLibs")