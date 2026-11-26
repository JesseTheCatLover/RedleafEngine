# Editor/CMakeLists.txt

redleaf_add_module(
        NAME Editor

        PUBLIC_DEPS
        EditorRuntime
        imgui

        PRIVATE_DEPS SilverwareCoreBackend SilverwareBackends SilverwareUI
)

redleaf_set_folder(Editor "EditorLibs")