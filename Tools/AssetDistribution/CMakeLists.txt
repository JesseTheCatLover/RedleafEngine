# Tools/AssetDistribution

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# --- Constants ---

set(REDLEAF_ASSET_MANIFEST_FORMAT_VERSION 1)
set(REDLEAF_ASSET_STATE_FORMAT_VERSION 1)
set(REDLEAF_ASSET_PLAN_FORMAT_VERSION 1)

include("${CMAKE_CURRENT_LIST_DIR}/AssetDistribution.cmake")

redleaf_sync_asset_distribution()