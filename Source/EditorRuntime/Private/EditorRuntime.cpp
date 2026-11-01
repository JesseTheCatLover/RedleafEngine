//  Copyright 2025-2026 JesseTheCatLover. All Rights Reserved.

#include "EditorRuntime.h"

#include "Core/EngineContext.h"
#include "Core/EngineGlobals.h"
#include "Core/JEngine.h"
#include "Core/Project/ProjectContext.h"
#include "Core/Project/VirtualPathMounter.h"
#include "EditorInput/EditorInputBootstrap.h"
#include "Framework/PostProcessManager.h"
#include "EditorInput/EditorInputDefaults.h"
#include "InputSystem/InputSubsystem.h"
#include "InputSystem/MappingStyles/ActionAxis/ActionAxisConfig.h"
#include "InputSystem/MappingStyles/ActionAxis/ActionAxisStyle.h"
#include "InputSystem/MappingStyles/Composite/CompositeInputMappingStyle.h"
#include "InputSystem/MappingStyles/HotkeyChord/HotkeyChordStyle.h"
#include "InputSystem/MappingStyles/HotkeyChord/HotkeyPlatformUtils.h"
#include "InputSystem/MappingStyles/HotkeyChord/HotkeySerialization.h"
#include "Rendering/FRenderView.h"
#include "Rendering/IPlatformSurface.h"
#include "Rendering/RendererSubsystem.h"
#include "Rendering/Silverware/SilverwareRenderingContext.h"
#include "Resources/ResourceSubsystem.h"
#include "Resources/Silverware/FVirtualPathSource.h"
#include "Utilities/UFileSystem.h"
#include "Utilities/UPath.h"

EditorRuntime::EditorRuntime()
    : m_Context(JEngine::Get().GetEngineContext())
    , m_SceneManager(*JEngine::Get().GetSceneManager())
    , m_Renderer(*JEngine::Get().GetRenderer())
    , m_PlatformSurface(*JEngine::Get().GetPlatformSurface())
    , m_VirtualPathMounter(JEngine::Get().GetVirtualPathMounter())
    , m_AssetManager(*JEngine::Get().GetAssetManager())
    , m_Resource(*JEngine::Get().GetResourceSubsystem())
    , m_InputSubsystem(*JEngine::Get().GetInputSubsystem())
    , m_InputManager(*JEngine::Get().GetInputManager())
    , m_SceneAPI(m_Context, m_SceneManager, *JEngine::Get().GetDebugDraw())
    , m_ViewportAPI(m_Context, m_Renderer)
    , m_SurfaceAPI(m_Context, m_PlatformSurface, m_InputManager)
    , m_FileAPI(m_Context, m_Resource, m_VirtualPathMounter, m_AssetManager)
{
    // Editor takes over rendering, so don't render directly to platform surface
    m_Context.SetShouldRenderToPlatformSurface(false);

    if (!EditorInputBootstrap::InstallEditorInputMapping(m_InputSubsystem))
    {
        std::cerr << "[EditorRuntime]: Failed to install editor input mappings.\n";
    }

    auto& chain = GetPostProcessManager()->EditChain(kEditorPostProfile); // TODO: LEGACY
    chain.clear();

    FPostPassDesc outline{};
    outline.name = "Outline";
    outline.bEnabled = true;

    // Optional defaults (can be overridden per-view via frame params)
    // Edge/occlusion tuning
    outline.params.floats["u_DepthEpsilon"] = 0.0005f;
    outline.params.floats["u_Thickness"]    = 1.5f;   // try 1..3

    // Defaults
    outline.params.floats["u_Occlusion"] = 0.0f;  // 1=hide behind walls, 0=x-ray outlines
    outline.params.floats["u_FillAlpha"] = 0.08f; // subtle fill inside selection

    // Outline color (linear space)
    outline.params.floats["u_OutlineR"] = 1.0f;
    outline.params.floats["u_OutlineG"] = 0.65f;
    outline.params.floats["u_OutlineB"] = 0.10f;
    outline.params.floats["u_OutlineA"] = 1.0f;

    // outline.params.floats["u_OutlineR"] = 0.18f;
    // outline.params.floats["u_OutlineG"] = 0.28f;
    // outline.params.floats["u_OutlineB"] = 0.40f;
    // outline.params.floats["u_OutlineA"] = 1.0f;

    chain.push_back(std::move(outline));

    FPostPassDesc fxaa{};
    fxaa.name = "FXAA";
    fxaa.bEnabled = true;

    // Optional tuning (sane defaults)
    fxaa.params.floats["u_FXAA_ReduceMin"] = 1.0f / 128.0f;
    fxaa.params.floats["u_FXAA_ReduceMul"] = 1.0f / 8.0f;
    fxaa.params.floats["u_FXAA_SpanMax"]   = 4.0f;

    chain.push_back(fxaa);
}

EditorRuntime::~EditorRuntime()
{

}

TUniquePtr<Silverware::IRenderingContext> EditorRuntime::CreateSilverwareRenderingContext()
{
    return MakeUnique<SilverwareRenderingContext>(m_Renderer);
}

TSharedPtr<Silverware::IResourceSource> EditorRuntime::CreateSilverwareResourceSource(
    const std::string& virtualRoot) const
{
    if (!m_VirtualPathMounter.IsMounted(virtualRoot))
        return {};

    return MakeShared<Silverware::Adapters::RedleafEngine::FVirtualPathSource>(m_VirtualPathMounter, virtualRoot);
}

void EditorRuntime::RestartEditor(const std::string &targetProjectPath, const std::string& extraArgs)
{
    JEngine::Get().RestartEditor(targetProjectPath, extraArgs);
}
