// Copyright 2026 JesseTheCatLover. All Rights Reserved.

#pragma once

#include <span>
#include <string>
#include <string_view>

#include "Core/Memory/SmartPointers.h"
#include "Silverware/Backend/IRenderingContext.h"
#include "Silverware/Backend/SilverwareHost.h"
#include "Silverware/Backend/SilverwareUIContext.h"
#include "Silverware/Core/Fonts/FontRegistry.h"

class InputManager;
class SilverwareInputBridge;
class EditorRuntime;
class IPlatformWindow;

class SilverwareBackend
{
private:
    TUniquePtr<Silverware::IRenderingContext> m_RenderingContext;
    TUniquePtr<SilverwareInputBridge> m_InputBridge;

    // These are populated during initialization and transferred into SilverwareHost.
    TUniquePtr<Silverware::IMountableResourceProvider> m_ResourceProvider;
    TUniquePtr<Silverware::FontRegistry> m_FontRegistry;

    TUniquePtr<Silverware::SilverwareHost> m_Host;
    Silverware::SilverwareUIContext* m_EditorUIContext = nullptr;

public:
    SilverwareBackend();
    ~SilverwareBackend();

    SilverwareBackend(const SilverwareBackend&) = delete;
    SilverwareBackend& operator=(const SilverwareBackend&) = delete;

    [[nodiscard]] bool Initialize(EditorRuntime& editorRuntime, InputManager& inputManager, IPlatformWindow& window,
        std::span<const Silverware::FResourcePath> registeredFonts);

    [[nodiscard]] bool Initialize(EditorRuntime& editorRuntime, InputManager& inputManager, IPlatformWindow& window);

    void Shutdown();

    [[nodiscard]] bool Synchronize(Silverware::CoreWidget& root, Silverware::EBackendSyncMode mode);

    [[nodiscard]] bool RegisterTheme(Silverware::FThemeDefinition theme, std::string* outError = nullptr);

    [[nodiscard]] bool RegisterStylesheet(std::string_view source,
        Silverware::FStylesheetParseError* outParseError = nullptr,
        std::string* outRegistrationError = nullptr);

    [[nodiscard]] bool UnregisterTheme(const Silverware::FThemeID& themeID, std::string* outError = nullptr);

    [[nodiscard]] bool SetActiveTheme(Silverware::FThemeID themeID, std::string* outError = nullptr);

    void ClearActiveTheme() noexcept;

    [[nodiscard]] bool TryGetWidgetSize(const Silverware::CoreWidget& widget,float& outWidth, float& outHeight) const;

    [[nodiscard]] Silverware::SilverwareUIContext* GetEditorUIContext() noexcept;
    [[nodiscard]] Silverware::SilverwareHost* GetHost() noexcept;

    void Render(IPlatformWindow& window);

    [[nodiscard]] bool IsInitialized() const noexcept;

private:
    [[nodiscard]] bool MountResources(EditorRuntime& editorRuntime);
    [[nodiscard]] bool InitializeRuntime(IPlatformWindow& window);
};