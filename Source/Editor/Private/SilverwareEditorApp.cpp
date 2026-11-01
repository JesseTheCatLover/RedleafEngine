// Copyright 2026 JesseTheCatLover. All Rights Reserved.

#include "SilverwareEditorApp.h"

#include "EditorRuntime.h"
#include "EditorCore/EditorHost.h"
#include "Rendering/IPlatformWindow.h"

#include "Silverware/Core/Resources/LayeredResourceProvider.h"

#include <iostream>

#include "Renderer/SilverwareEditorRenderer.h"
#include "Renderer/Backends/SilverwareBackend.h"

SilverwareEditorApp::SilverwareEditorApp()
{
}

SilverwareEditorApp::~SilverwareEditorApp()
{
    Shutdown();
}

void SilverwareEditorApp::Shutdown()
{
    if (m_SilverwareRenderer)
    {
        m_SilverwareRenderer->Shutdown();
        m_SilverwareRenderer.reset();
    }

    if (m_SilverwareBackend)
    {
        m_SilverwareBackend->Shutdown();
        m_SilverwareBackend.reset();
    }

    if (m_EditorHost)
    {
        m_EditorHost->Shutdown();
        m_EditorHost.reset();
    }

    m_Window = nullptr;
    m_EditorRuntime.reset();
}

void SilverwareEditorApp::OnProjectInitialized(IPlatformWindow* window, ProjectContext& projectCtx)
{
    if (!window)
    {
        std::cerr << "[SilverwareEditorApp]: Window is null.\n";
        return;
    }

    Shutdown();

    m_Window = window;
    m_EditorRuntime = TUniquePtr<EditorRuntime>(new EditorRuntime());
    m_EditorHost = TUniquePtr<EditorHost>(new EditorHost(*m_EditorRuntime));

    m_SilverwareBackend = MakeUnique<SilverwareBackend>();

    if (!m_SilverwareBackend->Initialize(*m_EditorRuntime, m_EditorRuntime->GetInputManager(), *window))
    {
        std::cerr << "[SilverwareEditorApp]: Backend initialization failed.\n";
        Shutdown();
        return;
    }

    m_SilverwareRenderer = MakeUnique<SilverwareEditorRenderer>();

    if (!m_SilverwareRenderer->Initialize(*m_EditorHost, *m_EditorRuntime, *m_SilverwareBackend, *window))
    {
        std::cerr << "[SilverwareEditorApp]: Renderer initialization failed.\n";
        Shutdown();
        return;
    }

    (void)projectCtx;
}

void SilverwareEditorApp::OnSceneLoaded(const std::string&)
{
}

void SilverwareEditorApp::OnTick(float deltaTime)
{
    if (m_EditorHost)
        m_EditorHost->Tick(deltaTime);

    if (m_SilverwareRenderer)
        m_SilverwareRenderer->Tick(deltaTime);
}

void SilverwareEditorApp::OnEngineShuttingDown()
{
    Shutdown();
}

void SilverwareEditorApp::OnRenderOverlay(float)
{
    if (!m_Window || !m_SilverwareBackend)
        return;

    m_SilverwareBackend->Render(*m_Window);

    if (m_SilverwareRenderer)
        m_SilverwareRenderer->RenderGameHUDContext(*m_Window);
}
