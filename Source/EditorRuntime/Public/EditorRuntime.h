//  Copyright 2025-2026 JesseTheCatLover. All Rights Reserved.

#pragma once

#include "EditorEdits/IEditActionSink.h"
#include "EditorAPI/File/FileAPI.h"
#include "EditorAPI/Scene/SceneAPI.h"
#include "EditorAPI/Surface/SurfaceAPI.h"
#include "EditorAPI/Viewport/ViewportAPI.h"
#include "Core/Memory/SmartPointers.h"

namespace Silverware
{
    class IResourceSource;
    class IRenderingContext;
}

class InputSubsystem;
class InputManager;
class ICameraViewSource;
class CameraEditorTool;
class EngineContext;
class SceneManager;
class RendererSubsystem;

class EditorRuntime
{
    friend class InkBlueEditorApp;
    friend class SilverwareEditorApp;
private:
    EditorRuntime();

    EngineContext& m_Context;
    SceneManager& m_SceneManager;
    RendererSubsystem& m_Renderer;
    IPlatformSurface& m_PlatformSurface;
    AssetManager& m_AssetManager;
    VirtualPathMounter& m_VirtualPathMounter;
    ResourceSubsystem& m_Resource;
    InputSubsystem& m_InputSubsystem;
    InputManager& m_InputManager;

    EditorSceneAPI m_SceneAPI;
    EditorViewportAPI m_ViewportAPI;
    EditorSurfaceAPI m_SurfaceAPI;
    EditorFileAPI m_FileAPI;

    IEditActionSink* m_EditSink = nullptr;

public:
    ~EditorRuntime();

    [[nodiscard]] TUniquePtr<Silverware::IRenderingContext> CreateSilverwareRenderingContext();
    [[nodiscard]] TSharedPtr<Silverware::IResourceSource> CreateSilverwareResourceSource(const std::string& virtualRoot) const;

    // Spawns a new engine process. If targetProjectPath is empty, it boots into the Launcher.
    void RestartEditor(const std::string& targetProjectPath = "", const std::string& extraArgs = "");

    // Mutating operations
    [[nodiscard]] EditorSceneAPI& GetScene() { return m_SceneAPI; }
    [[nodiscard]] EditorViewportAPI& GetViewport() { return m_ViewportAPI; }
    [[nodiscard]] EditorSurfaceAPI& GetSurface() { return m_SurfaceAPI; }
    [[nodiscard]] EditorFileAPI& GetFile() { return m_FileAPI; }

    void SetEditSink(IEditActionSink* sink) { m_EditSink = sink; }
    [[nodiscard]] IEditActionSink* GetEditSink() const { return m_EditSink; }

    static constexpr uint32_t kEditorPostProfile = 1; // TODO: Legacy: should be moved

    [[nodiscard]] InputManager& GetInputManager() const noexcept { return m_InputManager; }
};
