# Engine/CMakeLists.txt

# ---------------- Engine Target ----------------

redleaf_add_module(
        NAME Engine

        PUBLIC_DEPS glad stb glfw assimp glm nlohmann_json nfd
        PRIVATE_DEPS SilverwareCoreBackend
)

target_compile_definitions(
        Engine
        PRIVATE HAVE_GLFW HAVE_OPENGL
)

redleaf_enable_reflection(
        TARGET Engine
        MODULE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
)