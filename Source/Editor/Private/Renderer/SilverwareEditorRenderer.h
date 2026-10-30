// Copyright 2026 JesseTheCatLover. All Rights Reserved.

#pragma once

#include "Core/Memory/SmartPointers.h"
#include "Silverware/Memory/SmartPointers.h"

class IPlatformWindow;
class EditorHost;
class EditorRuntime;
class SilverwareBackend;

namespace Silverware
{
    class IRenderingContext;
    class SilverwareUIContext;
    class ContainerWidget;
    class ImageWidget;
    class TextWidget;
}

class SilverwareEditorRenderer
{
private:
    [[nodiscard]] bool RegisterEditorThemes();
    [[nodiscard]] bool RegisterGameHUDThemes();
    [[nodiscard]] bool BuildEditorLayout();
    [[nodiscard]] bool BuildGameHUDLayout();

    void UpdateThemeDemo(float deltaTime);
    void UpdateHUDTelemetry(float deltaTime);
    void UpdateGameHUDLayout(int width, int height);

    EditorHost* m_EditorHost = nullptr;
    EditorRuntime* m_EditorRuntime = nullptr;
    SilverwareBackend* m_Backend = nullptr;

    Silverware::TUniquePtr<Silverware::ContainerWidget> m_RootWidget;
    Silverware::ImageWidget* m_ViewportImage = nullptr;
    Silverware::TextWidget* m_EditorThemeLabel = nullptr;
    Silverware::TextWidget* m_EditorThemeChainLabel = nullptr;

    TUniquePtr<Silverware::IRenderingContext> m_GameHUDRenderingContext;
    Silverware::TUniquePtr<Silverware::ContainerWidget> m_GameHUDRootWidget;
    Silverware::SilverwareUIContext* m_GameHUDUIContext = nullptr;
    Silverware::ContainerWidget* m_GameHUDCard = nullptr;
    Silverware::ContainerWidget* m_GameHUDHealthFill = nullptr;
    Silverware::ContainerWidget* m_GameHUDShieldFill = nullptr;
    Silverware::TextWidget* m_GameHUDThemeLabel = nullptr;
    Silverware::TextWidget* m_GameHUDHealthLabel = nullptr;
    Silverware::TextWidget* m_GameHUDClockLabel = nullptr;

    int m_GameHUDContextWidth = 0;
    int m_GameHUDContextHeight = 0;

    float m_EditorThemeSwitchElapsed = 0.0f;
    float m_GameHUDThemeSwitchElapsed = 0.0f;
    float m_HUDTelemetryElapsed = 0.0f;
    float m_HUDClockSeconds = 0.0f;
    bool m_UseAmberEditorTheme = false;
    bool m_UseOverdriveHUDTheme = false;

public:
    SilverwareEditorRenderer();
    ~SilverwareEditorRenderer();

    SilverwareEditorRenderer(const SilverwareEditorRenderer&) = delete;
    SilverwareEditorRenderer& operator=(const SilverwareEditorRenderer&) = delete;

    [[nodiscard]] bool Initialize(
        EditorHost& editorHost,
        EditorRuntime& editorRuntime,
        SilverwareBackend& backend,
        IPlatformWindow& window);

    void RenderGameHUDContext(IPlatformWindow& window);
    void Tick(float deltaTime);
    void Shutdown();
};
