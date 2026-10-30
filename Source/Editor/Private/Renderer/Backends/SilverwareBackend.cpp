// Copyright 2026 JesseTheCatLover. All Rights Reserved.

#include "SilverwareBackend.h"

#include "EditorRuntime.h"
#include "Rendering/IPlatformWindow.h"

#include "Silverware/Backends/RmlUi/RmlUiBackendRuntime.h"
#include "Silverware/Core/Resources/FDirectoryResource.h"
#include "Silverware/Core/Resources/ResourcePath.h"
#include "Silverware/Core/Resources/IMountableResourceProvider.h"
#include "Silverware/Core/Resources/LayeredResourceProvider.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>

#include "Utilities/UFileSystem.h"

namespace
{
    constexpr char kSilverwareRootDocument[] =
        "silverware:/Backends/RmlUi/Documents/SilverwareRoot.rml";

    constexpr float kDefaultUIScale = 1.0f;

    float GetDisplayScale(const IPlatformWindow& window)
    {
        float xScale = 1.0f;
        float yScale = 1.0f;
        window.GetContentScale(xScale, yScale);

        if (std::isfinite(xScale) && xScale > 0.0f)
            return xScale;

        if (std::isfinite(yScale) && yScale > 0.0f)
            return yScale;

        return 1.0f;
    }

    const char* GetFontRegistrationStatusName(Silverware::EFontRegistrationStatus status)
    {
        using Silverware::EFontRegistrationStatus;

        switch (status)
        {
        case EFontRegistrationStatus::Registered: return "Registered";
        case EFontRegistrationStatus::AlreadyRegistered: return "AlreadyRegistered";
        case EFontRegistrationStatus::InvalidResourcePath: return "InvalidResourcePath";
        case EFontRegistrationStatus::InvalidOptions: return "InvalidOptions";
        case EFontRegistrationStatus::UnsupportedFileType: return "UnsupportedFileType";
        case EFontRegistrationStatus::NotFound: return "NotFound";
        case EFontRegistrationStatus::InvalidFontData: return "InvalidFontData";
        case EFontRegistrationStatus::ResourceError: return "ResourceError";
        }

        return "Unknown";
    }
}

SilverwareBackend::~SilverwareBackend()
{
    Shutdown();
}

bool SilverwareBackend::Initialize(EditorRuntime& editorRuntime, IPlatformWindow& window,
    std::span<const Silverware::FResourcePath> registeredFonts)
{
    Shutdown();

    m_RenderingContext = editorRuntime.CreateSilverwareRenderingContext();

    if (!m_RenderingContext)
    {
        std::cerr << "[SilverwareBackend]: Failed to create rendering context.\n";
        Shutdown();
        return false;
    }

    if (!MountResources(editorRuntime))
    {
        Shutdown();
        return false;
    }

    // RedleafEngine owns this registry and registers its own fonts.
    m_FontRegistry = Silverware::MakeUnique<Silverware::FontRegistry>(*m_ResourceProvider);

    Silverware::FFontDirectoryOptions engineFontOptions;
    engineFontOptions.recursive = true;

    const auto engineFontReport = m_FontRegistry->RegisterFontDirectory("engine:/Editor/Fonts", engineFontOptions);

    if (!engineFontReport.Completed())
        std::cerr << "[SilverwareBackend]: Could not enumerate engine editor fonts.\n";

    for (const auto& diagnostic : engineFontReport.diagnostics)
    {
        std::cerr << "[SilverwareBackend]: Rejected engine font "
                  << diagnostic.path.ToString() << " ("
                  << GetFontRegistrationStatusName(diagnostic.status) << ").\n";
    }

    // Import the prepared host inventory, including synchronized project fonts.
    const auto preparedFontReport = m_FontRegistry->RegisterFonts(registeredFonts);

    for (const auto& diagnostic : preparedFontReport.diagnostics)
    {
        std::cerr << "[SilverwareBackend]: Rejected prepared font "
                  << diagnostic.path.ToString() << " ("
                  << GetFontRegistrationStatusName(diagnostic.status) << ").\n";
    }

    if (m_FontRegistry->IsEmpty())
        std::cerr << "[SilverwareBackend]: No host fonts passed registration validation; "
                     "continuing without host fonts.\n";

    if (!InitializeRuntime(window))
    {
        Shutdown();
        return false;
    }

    return true;
}

bool SilverwareBackend::Initialize(EditorRuntime& editorRuntime, IPlatformWindow& window)
{
    return Initialize(editorRuntime, window, std::span<const Silverware::FResourcePath>{});
}

bool SilverwareBackend::MountResources(EditorRuntime& editorRuntime)
{
    // The host owns the provider and mounts only host namespaces.
    m_ResourceProvider = MakeUnique<Silverware::FLayeredResourceProvider>();

    auto engineSource = editorRuntime.CreateSilverwareResourceSource("/Engine");

    if (!engineSource || !m_ResourceProvider->Mount("engine", 0, engineSource))
    {
        std::cerr << "[SilverwareBackend]: Failed to mount engine resources.\n";
        return false;
    }

    // Projects may not always have a mounted project resource root.
    auto projectSource = editorRuntime.CreateSilverwareResourceSource("/Project");

    if (projectSource && !m_ResourceProvider->Mount("project", 0, projectSource))
    {
        std::cerr << "[SilverwareBackend]: Failed to mount project resources.\n";
        return false;
    }

    return true;
}

bool SilverwareBackend::InitializeRuntime(IPlatformWindow& window)
{
    if (!m_RenderingContext || !m_ResourceProvider || !m_FontRegistry)
    {
        std::cerr << "[SilverwareBackend]: Resources and rendering context must be ready before host initialization.\n";
        return false;
    }

    int width = 0;
    int height = 0;
    window.GetFramebufferSize(width, height);

    if (width <= 0 || height <= 0)
    {
        width = 1;
        height = 1;
    }

    const float displayScale = GetDisplayScale(window);

    Silverware::Backends::RmlUi::FRmlUiBackendRuntimeConfig backendConfig;

    if (!Silverware::FResourcePath::TryParse(kSilverwareRootDocument, backendConfig.rootDocument))
    {
        std::cerr << "[SilverwareBackend]: Invalid root document resource path.\n";
        return false;
    }

    Silverware::FSilverwareHostConfig hostConfig;
    hostConfig.backendRuntime =
        MakeUnique<Silverware::Backends::RmlUi::RmlUiBackendRuntime>(TakeUniqueOwnership(backendConfig));
    hostConfig.resourceProvider = Silverware::TakeUniqueOwnership(m_ResourceProvider);
    hostConfig.fontRegistry = Silverware::TakeUniqueOwnership(m_FontRegistry);

    // Resolve Silverware's installation path; the runtime mounts its resources itself.
    hostConfig.silverwareRoot = std::filesystem::path(UFileSystem::GetEngineRoot() / "Frameworks" / "Silverware");

    m_Host = MakeUnique<Silverware::SilverwareHost>();

    std::string error;

    if (!m_Host->Initialize(Silverware::TakeUniqueOwnership(hostConfig), &error))
    {
        std::cerr << "[SilverwareBackend]: Silverware host initialization failed: "
                  << error << '\n';

        m_Host.reset();
        return false;
    }

    m_EditorUIContext = m_Host->CreateUIContext(
        "Redleaf.Editor",
        *m_RenderingContext,
        width,
        height,
        displayScale,
        kDefaultUIScale,
        &error);

    if (!m_EditorUIContext)
    {
        std::cerr << "[SilverwareBackend]: Failed to create editor UI context: "
                  << error << '\n';

        m_Host->Shutdown();
        m_Host.reset();
        return false;
    }

    // std::cout << "[SilverwareBackend]: Editor UI scale: display="
    //           << m_EditorUIContext->GetDisplayScale()
    //           << ", ui=" << m_EditorUIContext->GetUIScale()
    //           << ", effective=" << m_EditorUIContext->GetEffectiveScale()
    //           << '\n';

    return true;
}

bool SilverwareBackend::Synchronize(Silverware::CoreWidget& root, Silverware::EBackendSyncMode mode)
{
    return m_EditorUIContext &&
        m_EditorUIContext->Synchronize(root, mode);
}

bool SilverwareBackend::RegisterTheme(Silverware::FThemeDefinition theme, std::string* outError)
{
    if (!m_EditorUIContext)
    {
        if (outError)
            *outError = "Editor UI context is unavailable.";

        return false;
    }

    return m_EditorUIContext->RegisterTheme(Silverware::TakeUniqueOwnership(theme), outError);
}

bool SilverwareBackend::RegisterStylesheet(
    std::string_view source,
    Silverware::FStylesheetParseError* outParseError,
    std::string* outRegistrationError)
{
    if (!m_EditorUIContext)
    {
        if (outRegistrationError)
            *outRegistrationError = "Editor UI context is unavailable.";

        return false;
    }

    return m_EditorUIContext->RegisterStylesheet(
        source, outParseError, outRegistrationError);
}

bool SilverwareBackend::UnregisterTheme(const Silverware::FThemeID& themeID, std::string* outError)
{
    if (!m_EditorUIContext)
    {
        if (outError)
            *outError = "Editor UI context is unavailable.";

        return false;
    }

    return m_EditorUIContext->UnregisterTheme(themeID, outError);
}

bool SilverwareBackend::SetActiveTheme(Silverware::FThemeID themeID, std::string* outError)
{
    if (!m_EditorUIContext)
    {
        if (outError)
            *outError = "Editor UI context is unavailable.";

        return false;
    }

    return m_EditorUIContext->SetActiveTheme(TakeUniqueOwnership(themeID), outError);
}

void SilverwareBackend::ClearActiveTheme() noexcept
{
    if (m_EditorUIContext)
        m_EditorUIContext->ClearActiveTheme();
}

bool SilverwareBackend::TryGetWidgetSize(const Silverware::CoreWidget& widget, float& outWidth, float& outHeight) const
{
    outWidth = 0.0f;
    outHeight = 0.0f;

    return m_EditorUIContext &&
        m_EditorUIContext->TryGetWidgetSize(widget, outWidth, outHeight);
}

Silverware::SilverwareUIContext* SilverwareBackend::GetEditorUIContext() noexcept
{
    return m_EditorUIContext;
}

Silverware::SilverwareHost* SilverwareBackend::GetHost() noexcept
{
    return m_Host.get();
}

void SilverwareBackend::Render(IPlatformWindow& window)
{
    if (!IsInitialized())
        return;

    int width = 0;
    int height = 0;
    window.GetFramebufferSize(width, height);

    if (width <= 0 || height <= 0)
        return;

    m_EditorUIContext->Resize(width, height);
    m_EditorUIContext->Update();
    m_EditorUIContext->Render();
}

void SilverwareBackend::Shutdown()
{
    // Contexts must be destroyed before the shared backend and resources.
    m_EditorUIContext = nullptr;

    if (m_Host)
    {
        m_Host->Shutdown();
        m_Host.reset();
    }

    // These remain non-null only if initialization failed before ownership transfer.
    m_FontRegistry.reset();
    m_ResourceProvider.reset();

    // The editor rendering context must outlive its backend context.
    m_RenderingContext.reset();
}

bool SilverwareBackend::IsInitialized() const noexcept
{
    return m_Host && m_Host->IsInitialized() &&
        m_EditorUIContext && m_EditorUIContext->IsInitialized();
}