# Tools/AssetDistribution

find_package(Python3 REQUIRED COMPONENTS Interpreter)

include("${CMAKE_CURRENT_LIST_DIR}/AssetDistribution.cmake")

redleaf_sync_asset_distribution()