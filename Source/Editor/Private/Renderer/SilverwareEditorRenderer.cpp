// Copyright 2026 JesseTheCatLover. All Rights Reserved.

#include "SilverwareEditorRenderer.h"

#include "EditorCore/EditorHost.h"
#include "EditorRuntime.h"
#include "Backends/SilverwareBackend.h"
#include "Panels/Controllers/Inputs/FViewportPanelInput.h"
#include "Panels/Controllers/Outputs/FViewportOutput.h"
#include "Panels/Subsystems/ViewportSubsystem.h"
#include "Rendering/IPlatformWindow.h"

#include "Silverware/Backend/IRenderingContext.h"
#include "Silverware/Backend/SilverwareHost.h"
#include "Silverware/Backend/SilverwareUIContext.h"
#include "Silverware/Core/Resources/ResourcePath.h"
#include "Silverware/Core/Styling/StyleModel.h"
#include "Silverware/Core/Widgets/ContainerWidget.h"
#include "Silverware/Core/Widgets/CoreWidget.h"
#include "Silverware/Core/Widgets/WidgetLayout.h"
#include "Silverware/Core/Widgets/Styling/ContainerStyle.h"
#include "Silverware/Core/Widgets/Styling/ImageStyle.h"
#include "Silverware/Core/Widgets/Styling/TextStyle.h"
#include "Silverware/UI/ImageWidget.h"
#include "Silverware/UI/TextWidget.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <utility>

namespace
{
    using namespace Silverware;

    constexpr char kViewportPanelKey[] = "Viewport:0";

    constexpr char kBaseThemeID[] = "Redleaf.Playground.Base";
    constexpr char kEditorCyanThemeID[] = "Redleaf.Editor.Cyan";
    constexpr char kEditorAmberThemeID[] = "Redleaf.Editor.Amber";
    constexpr char kHUDTacticalThemeID[] = "Redleaf.GameHUD.Tactical";
    constexpr char kHUDOverdriveThemeID[] = "Redleaf.GameHUD.Overdrive";
    constexpr char kGameHUDContextID[] = "Redleaf.GameHUD";

    constexpr float kEditorThemeSwitchIntervalSeconds = 5.0f;
    constexpr float kHUDThemeSwitchIntervalSeconds = 3.5f;
    constexpr float kHUDTelemetryIntervalSeconds = 0.15f;

    FStyleValue Token(const char* tokenID)
    {
        return FStyleTokenReference{std::string(tokenID)};
    }

    void AddToken(FThemeDefinition& theme, const char* tokenID, FStyleValue value)
    {
        const std::string id(tokenID);
        const std::size_t firstDot = id.find('.');
        const std::size_t lastDot = id.find_last_of('.');
        const std::string category = firstDot == std::string::npos
            ? std::string{}
            : id.substr(0, firstDot);
        const std::string displayName = lastDot == std::string::npos
            ? id
            : id.substr(lastDot + 1);

        theme.tokens.push_back({id, displayName, category, std::move(value)});
    }

    void AddVariant(FThemeDefinition& theme, const char* widgetType, const char* variantID, const char* displayName)
    {
        theme.variants.push_back({widgetType, variantID, displayName});
    }

    void AddRule(
        FThemeDefinition& theme,
        const char* widgetType,
        const char* variantID,
        FWidgetStateMask requiredStates,
        FStylePropertyMap properties)
    {
        FStyleRule rule;
        rule.selector.widgetType = widgetType;

        if (variantID && *variantID)
            rule.selector.variant = variantID;

        rule.selector.requiredStates = requiredStates;
        rule.properties = std::move(properties);
        theme.rules.push_back(std::move(rule));
    }

    FThemeDefinition MakeBaseTheme()
    {
        FThemeDefinition theme;
        theme.ID = kBaseThemeID;

        AddToken(theme, "color.canvas", FColor{0.035f, 0.043f, 0.063f, 1.0f});
        AddToken(theme, "color.surface", FColor{0.065f, 0.078f, 0.106f, 1.0f});
        AddToken(theme, "color.surfaceRaised", FColor{0.090f, 0.108f, 0.145f, 1.0f});
        AddToken(theme, "color.border", FColor{0.18f, 0.22f, 0.29f, 1.0f});
        AddToken(theme, "color.text", FColor{0.91f, 0.93f, 0.97f, 1.0f});
        AddToken(theme, "color.muted", FColor{0.57f, 0.62f, 0.70f, 1.0f});
        AddToken(theme, "color.accent", FColor{0.25f, 0.80f, 1.0f, 1.0f});
        AddToken(theme, "color.accentSoft", FColor{0.08f, 0.34f, 0.46f, 0.42f});
        AddToken(theme, "color.success", FColor{0.30f, 0.88f, 0.62f, 1.0f});
        AddToken(theme, "color.warning", FColor{1.0f, 0.70f, 0.30f, 1.0f});
        AddToken(theme, "color.danger", FColor{1.0f, 0.35f, 0.42f, 1.0f});
        AddToken(theme, "color.healthTrack", FColor{0.15f, 0.18f, 0.23f, 1.0f});
        AddToken(theme, "color.health", FColor{0.30f, 0.88f, 0.62f, 1.0f});
        AddToken(theme, "font.family.body", std::string("LatoLatin"));
        AddToken(theme, "font.family.mono", std::string("Roboto Mono"));

        AddVariant(theme, "Container", "Root", "Application canvas");
        AddVariant(theme, "Container", "Toolbar", "Toolbar");
        AddVariant(theme, "Container", "Panel", "Editor panel");
        AddVariant(theme, "Container", "Card", "Card");
        AddVariant(theme, "Container", "SceneRow", "Scene hierarchy row");
        AddVariant(theme, "Container", "StatusPill", "Status badge");
        AddVariant(theme, "Container", "Divider", "Divider");
        AddVariant(theme, "Container", "HudPanel", "In-game HUD panel");
        AddVariant(theme, "Container", "HudChip", "HUD status chip");
        AddVariant(theme, "Container", "HealthTrack", "Health meter track");
        AddVariant(theme, "Container", "HealthFill", "Health meter fill");
        AddVariant(theme, "Container", "ShieldFill", "Shield meter fill");

        AddVariant(theme, "Text", "Header", "Header");
        AddVariant(theme, "Text", "PanelTitle", "Panel title");
        AddVariant(theme, "Text", "Body", "Body text");
        AddVariant(theme, "Text", "Kicker", "Uppercase metadata");
        AddVariant(theme, "Text", "Accent", "Accent text");
        AddVariant(theme, "Text", "Muted", "Secondary text");
        AddVariant(theme, "Text", "Success", "Success text");
        AddVariant(theme, "Text", "Warning", "Warning text");
        AddVariant(theme, "Text", "Danger", "Danger text");
        AddVariant(theme, "Text", "Mono", "Monospaced metadata");
        AddVariant(theme, "Text", "Value", "Numeric value");
        AddVariant(theme, "Text", "HudTitle", "HUD title");
        AddVariant(theme, "Text", "HudMetric", "HUD metric");
        AddVariant(theme, "Image", "PreviewImage", "Viewport preview image");

        AddRule(theme, "Container", "Root", 0, {
            {"background-color", Token("color.canvas")}
        });

        AddRule(theme, "Container", "Toolbar", 0, {
            {"background-color", Token("color.surfaceRaised")},
            {"border-color", Token("color.border")},
            {"border-width", 1.0f},
            {"border-style", EBorderStyle::Solid},
            {"border-radius", 9.0f}
        });

        AddRule(theme, "Container", "Panel", 0, {
            {"background-color", Token("color.surface")},
            {"border-color", Token("color.border")},
            {"border-width", 1.0f},
            {"border-style", EBorderStyle::Solid},
            {"border-radius", 10.0f}
        });

        AddRule(theme, "Container", "Card", 0, {
            {"background-color", Token("color.surfaceRaised")},
            {"border-color", Token("color.border")},
            {"border-width", 1.0f},
            {"border-style", EBorderStyle::Solid},
            {"border-radius", 8.0f}
        });

        AddRule(theme, "Container", "SceneRow", 0, {
            {"background-color", Token("color.surfaceRaised")},
            {"border-color", Token("color.surfaceRaised")},
            {"border-width", 1.0f},
            {"border-style", EBorderStyle::Solid},
            {"border-radius", 6.0f}
        });

        AddRule(theme, "Container", "SceneRow", ToWidgetStateMask(EWidgetState::Selected), {
            {"background-color", Token("color.accentSoft")},
            {"border-color", Token("color.accent")}
        });

        AddRule(theme, "Container", "SceneRow", ToWidgetStateMask(EWidgetState::Hovered), {
            {"background-color", Token("color.accentSoft")},
            {"border-color", Token("color.accent")}
        });

        AddRule(theme, "Container", "SceneRow", ToWidgetStateMask(EWidgetState::Disabled), {
            {"opacity", 0.42f}
        });

        AddRule(theme, "Container", "StatusPill", 0, {
            {"background-color", Token("color.accentSoft")},
            {"border-color", Token("color.accent")},
            {"border-width", 1.0f},
            {"border-style", EBorderStyle::Solid},
            {"border-radius", 99.0f}
        });

        AddRule(theme, "Container", "Divider", 0, {
            {"background-color", Token("color.border")}
        });

        AddRule(theme, "Container", "HudPanel", 0, {
            {"background-color", Token("color.surface")},
            {"border-color", Token("color.accent")},
            {"border-width", 1.25f},
            {"border-style", EBorderStyle::Solid},
            {"border-radius", 14.0f},
            {"opacity", 0.97f}
        });

        AddRule(theme, "Container", "HudChip", 0, {
            {"background-color", Token("color.surfaceRaised")},
            {"border-color", Token("color.border")},
            {"border-width", 1.0f},
            {"border-style", EBorderStyle::Solid},
            {"border-radius", 6.0f}
        });

        AddRule(theme, "Container", "HealthTrack", 0, {
            {"background-color", Token("color.healthTrack")},
            {"border-style", EBorderStyle::None},
            {"border-radius", 5.0f}
        });

        AddRule(theme, "Container", "HealthFill", 0, {
            {"background-color", Token("color.health")},
            {"border-style", EBorderStyle::None},
            {"border-radius", 5.0f}
        });

        AddRule(theme, "Container", "ShieldFill", 0, {
            {"background-color", Token("color.accent")},
            {"border-style", EBorderStyle::None},
            {"border-radius", 5.0f}
        });

        AddRule(theme, "Text", nullptr, 0, {
            {"color", Token("color.text")},
            {"font-family", Token("font.family.body")}
        });

        AddRule(theme, "Text", "Header", 0, {
            {"font-size", 18.0f},
            {"font-weight", EFontWeight::Bold}
        });

        AddRule(theme, "Text", "PanelTitle", 0, {
            {"font-size", 13.0f},
            {"font-weight", EFontWeight::Bold}
        });

        AddRule(theme, "Text", "Body", 0, {
            {"color", Token("color.muted")},
            {"font-size", 12.0f}
        });

        AddRule(theme, "Text", "Kicker", 0, {
            {"color", Token("color.accent")},
            {"font-family", Token("font.family.mono")},
            {"font-size", 10.0f},
            {"font-weight", EFontWeight::Bold}
        });

        AddRule(theme, "Text", "Accent", 0, {
            {"color", Token("color.accent")},
            {"font-weight", EFontWeight::Bold}
        });

        AddRule(theme, "Text", "Muted", 0, {
            {"color", Token("color.muted")}
        });

        AddRule(theme, "Text", "Success", 0, {
            {"color", Token("color.success")},
            {"font-weight", EFontWeight::Bold}
        });

        AddRule(theme, "Text", "Warning", 0, {
            {"color", Token("color.warning")},
            {"font-weight", EFontWeight::Bold}
        });

        AddRule(theme, "Text", "Danger", 0, {
            {"color", Token("color.danger")},
            {"font-weight", EFontWeight::Bold}
        });

        AddRule(theme, "Text", "Mono", 0, {
            {"color", Token("color.muted")},
            {"font-family", Token("font.family.mono")},
            {"font-size", 10.0f}
        });

        AddRule(theme, "Text", "Value", 0, {
            {"font-family", Token("font.family.mono")},
            {"font-size", 15.0f},
            {"font-weight", EFontWeight::Bold}
        });

        AddRule(theme, "Text", "HudTitle", 0, {
            {"font-size", 15.0f},
            {"font-weight", EFontWeight::Bold}
        });

        AddRule(theme, "Text", "HudMetric", 0, {
            {"color", Token("color.health")},
            {"font-family", Token("font.family.mono")},
            {"font-size", 15.0f},
            {"font-weight", EFontWeight::Bold}
        });

        AddRule(theme, "Image", "PreviewImage", 0, {
            {"object-fit", EImageFit::Cover},
            {"opacity", 1.0f},
            {"border-radius", 8.0f}
        });

        return theme;
    }

    FThemeDefinition MakeAccentTheme(
        const char* themeID,
        const char* baseThemeID,
        FColor accent,
        FColor accentSoft,
        FColor health)
    {
        FThemeDefinition theme;
        theme.ID = themeID;
        theme.baseThemeID = baseThemeID;

        AddToken(theme, "color.accent", accent);
        AddToken(theme, "color.accentSoft", accentSoft);
        AddToken(theme, "color.health", health);
        return theme;
    }

    bool AddThemeToEditor(SilverwareBackend& backend, FThemeDefinition theme)
    {
        std::string error;

        if (backend.RegisterTheme(std::move(theme), &error))
            return true;

        std::cerr << "[SilverwarePlayground]: Theme registration failed: " << error << '\n';
        return false;
    }

    bool AddThemeToContext(Silverware::SilverwareUIContext& context, FThemeDefinition theme)
    {
        std::string error;

        if (context.RegisterTheme(std::move(theme), &error))
            return true;

        std::cerr << "[SilverwarePlayground]: HUD theme registration failed: " << error << '\n';
        return false;
    }

    void SubmitViewportInput(ViewportSubsystem& viewportSubsystem, float width, float height)
    {
        const bool hasValidSize = width > 0.0f && height > 0.0f;

        FViewportPanelInput input{};
        input.panelKey = kViewportPanelKey;
        input.width = hasValidSize ? width : 0.0f;
        input.height = hasValidSize ? height : 0.0f;
        input.bAppFocused = true;
        input.bHidden = !hasValidSize;
        viewportSubsystem.SubmitInput(input);
    }

    Silverware::ContainerWidget* AddContainer(
        Silverware::ContainerWidget& parent,
        const char* variant,
        Silverware::FWidgetLayout layout,
        Silverware::FContainerLayout containerLayout)
    {
        using namespace Silverware;

        auto widget = Silverware::MakeUnique<ContainerWidget>();

        if (variant && *variant)
            widget->SetStyleVariant(variant);

        widget->SetLayout(layout);
        widget->SetContainerLayout(containerLayout);

        ContainerWidget* rawWidget = widget.get();

        if (!parent.AddChild(Silverware::TUniquePtr<CoreWidget>(widget.release())))
            return nullptr;

        return rawWidget;
    }

    Silverware::TextWidget* AddText(
        Silverware::ContainerWidget& parent,
        std::string text,
        const char* variant,
        Silverware::FWidgetLayout layout = {})
    {
        using namespace Silverware;

        auto widget = Silverware::MakeUnique<TextWidget>(std::move(text));

        if (variant && *variant)
            widget->SetStyleVariant(variant);

        widget->SetLayout(layout);

        TextWidget* rawWidget = widget.get();

        if (!parent.AddChild(Silverware::TUniquePtr<CoreWidget>(widget.release())))
            return nullptr;

        return rawWidget;
    }

    void SetPadding(Silverware::FWidgetLayout& layout, float top, float right, float bottom, float left)
    {
        layout.padding.top = Silverware::FLayoutLength::Units(top);
        layout.padding.right = Silverware::FLayoutLength::Units(right);
        layout.padding.bottom = Silverware::FLayoutLength::Units(bottom);
        layout.padding.left = Silverware::FLayoutLength::Units(left);
    }

    Silverware::FContainerLayout MakeContainerLayout(
        Silverware::ELayoutDirection direction,
        float gap,
        Silverware::EAlignment alignItems = Silverware::EAlignment::Stretch,
        Silverware::EJustification justifyContent = Silverware::EJustification::Start)
    {
        Silverware::FContainerLayout layout;
        layout.direction = direction;
        layout.gap = Silverware::FLayoutLength::Units(gap);
        layout.alignItems = alignItems;
        layout.justifyContent = justifyContent;
        return layout;
    }

    Silverware::ContainerWidget* AddSceneRow(
        Silverware::ContainerWidget& parent,
        const char* label,
        const char* status,
        std::optional<Silverware::EWidgetState> previewState = std::nullopt)
    {
        using namespace Silverware;

        FWidgetLayout layout;
        layout.width = FLayoutLength::Auto();
        layout.height = FLayoutLength::Units(32.0f);
        SetPadding(layout, 0.0f, 8.0f, 0.0f, 8.0f);

        ContainerWidget* row = AddContainer(
            parent,
            "SceneRow",
            layout,
            MakeContainerLayout(ELayoutDirection::Horizontal, 6.0f, EAlignment::Center, EJustification::SpaceBetween));

        if (!row)
            return nullptr;

        if (previewState && !row->SetStyleState(*previewState, true))
            return nullptr;

        FWidgetLayout labelLayout;
        labelLayout.width = FLayoutLength::Auto();
        labelLayout.height = FLayoutLength::Auto();

        if (!AddText(*row, label, "Body", labelLayout))
            return nullptr;

        if (status && *status)
        {
            if (!AddText(*row, status, "Kicker"))
                return nullptr;
        }

        return row;
    }
}

SilverwareEditorRenderer::SilverwareEditorRenderer()
{
}

SilverwareEditorRenderer::~SilverwareEditorRenderer()
{
    Shutdown();
}

bool SilverwareEditorRenderer::RegisterEditorThemes()
{
    if (!m_Backend)
        return false;

    // The editor host outlives this playground renderer, so clean up its demo-only theme IDs.
    (void)m_Backend->UnregisterTheme(kEditorAmberThemeID);
    (void)m_Backend->UnregisterTheme(kEditorCyanThemeID);
    (void)m_Backend->UnregisterTheme("Redleaf.EditorStylingDemo.Orange");
    (void)m_Backend->UnregisterTheme("Redleaf.EditorStylingDemo.Cyan");
    (void)m_Backend->UnregisterTheme(kBaseThemeID);

    if (!AddThemeToEditor(*m_Backend, MakeBaseTheme()) ||
        !AddThemeToEditor(*m_Backend, MakeAccentTheme(
            kEditorCyanThemeID,
            kBaseThemeID,
            Silverware::FColor{0.25f, 0.80f, 1.0f, 1.0f},
            Silverware::FColor{0.08f, 0.34f, 0.46f, 0.42f},
            Silverware::FColor{0.30f, 0.88f, 0.62f, 1.0f})) ||
        !AddThemeToEditor(*m_Backend, MakeAccentTheme(
            kEditorAmberThemeID,
            kBaseThemeID,
            Silverware::FColor{1.0f, 0.56f, 0.20f, 1.0f},
            Silverware::FColor{0.48f, 0.22f, 0.06f, 0.48f},
            Silverware::FColor{0.38f, 0.88f, 0.52f, 1.0f})))
        return false;

    std::string error;

    if (!m_Backend->SetActiveTheme(kEditorCyanThemeID, &error))
    {
        std::cerr << "[SilverwarePlayground]: Failed to activate editor theme: " << error << '\n';
        return false;
    }

    m_UseAmberEditorTheme = false;

    if (m_EditorThemeLabel)
        m_EditorThemeLabel->SetText("EDITOR / CYAN");

    if (m_EditorThemeChainLabel)
        m_EditorThemeChainLabel->SetText("BASE  >  EDITOR / CYAN");

    return true;
}

bool SilverwareEditorRenderer::RegisterGameHUDThemes()
{
    if (!m_GameHUDUIContext)
        return false;

    if (!AddThemeToContext(*m_GameHUDUIContext, MakeBaseTheme()) ||
        !AddThemeToContext(*m_GameHUDUIContext, MakeAccentTheme(
            kHUDTacticalThemeID,
            kBaseThemeID,
            Silverware::FColor{0.25f, 0.98f, 0.72f, 1.0f},
            Silverware::FColor{0.04f, 0.36f, 0.25f, 0.48f},
            Silverware::FColor{0.25f, 0.98f, 0.72f, 1.0f})) ||
        !AddThemeToContext(*m_GameHUDUIContext, MakeAccentTheme(
            kHUDOverdriveThemeID,
            kHUDTacticalThemeID,
            Silverware::FColor{1.0f, 0.24f, 0.42f, 1.0f},
            Silverware::FColor{0.42f, 0.06f, 0.14f, 0.52f},
            Silverware::FColor{1.0f, 0.32f, 0.34f, 1.0f})))
        return false;

    std::string error;

    if (!m_GameHUDUIContext->SetActiveTheme(kHUDTacticalThemeID, &error))
    {
        std::cerr << "[SilverwarePlayground]: Failed to activate HUD theme: " << error << '\n';
        return false;
    }

    m_UseOverdriveHUDTheme = false;

    if (m_GameHUDThemeLabel)
        m_GameHUDThemeLabel->SetText("TACTICAL");

    return true;
}

bool SilverwareEditorRenderer::Initialize(
    EditorHost& editorHost,
    EditorRuntime& editorRuntime,
    SilverwareBackend& backend,
    IPlatformWindow& window)
{
    Shutdown();

    if (!backend.IsInitialized())
    {
        std::cerr << "[SilverwareRenderer]: Backend is not initialized.\n";
        return false;
    }

    m_EditorHost = &editorHost;
    m_EditorRuntime = &editorRuntime;
    m_Backend = &backend;

    m_RootWidget = Silverware::MakeUnique<Silverware::ContainerWidget>();

    if (!BuildEditorLayout())
    {
        std::cerr << "[SilverwareRenderer]: Failed to build editor playground layout.\n";
        Shutdown();
        return false;
    }

    if (!RegisterEditorThemes())
    {
        std::cerr << "[SilverwareRenderer]: Failed to register editor playground themes.\n";
        Shutdown();
        return false;
    }

    SubmitViewportInput(editorHost.GetSubsystem<ViewportSubsystem>(), 0.0f, 0.0f);

    int width = 0;
    int height = 0;
    window.GetFramebufferSize(width, height);

    if (width <= 0 || height <= 0)
    {
        width = 1;
        height = 1;
    }

    m_GameHUDRenderingContext = editorRuntime.CreateSilverwareRenderingContext();

    if (!m_GameHUDRenderingContext)
    {
        std::cerr << "[SilverwareRenderer]: Failed to create game HUD rendering context.\n";
        Shutdown();
        return false;
    }

    Silverware::SilverwareHost* host = backend.GetHost();

    if (!host)
    {
        std::cerr << "[SilverwareRenderer]: Silverware host is unavailable.\n";
        Shutdown();
        return false;
    }

    std::string error;
    m_GameHUDUIContext = host->CreateUIContext(
        kGameHUDContextID,
        *m_GameHUDRenderingContext,
        width,
        height,
        &error);

    if (!m_GameHUDUIContext)
    {
        std::cerr << "[SilverwareRenderer]: Failed to create game HUD context: " << error << '\n';
        Shutdown();
        return false;
    }

    if (!BuildGameHUDLayout() || !RegisterGameHUDThemes())
    {
        std::cerr << "[SilverwareRenderer]: Failed to build or theme the game HUD.\n";
        Shutdown();
        return false;
    }

    UpdateGameHUDLayout(width, height);
    m_GameHUDContextWidth = width;
    m_GameHUDContextHeight = height;

    if (!m_Backend->Synchronize(*m_RootWidget, Silverware::EBackendSyncMode::Full))
    {
        std::cerr << "[SilverwareRenderer]: Initial editor synchronization failed.\n";
        Shutdown();
        return false;
    }

    if (!m_GameHUDUIContext->Synchronize(
            *m_GameHUDRootWidget,
            Silverware::EBackendSyncMode::Full))
    {
        std::cerr << "[SilverwareRenderer]: Initial game HUD synchronization failed.\n";
        Shutdown();
        return false;
    }

    return true;
}

bool SilverwareEditorRenderer::BuildEditorLayout()
{
    using namespace Silverware;

    if (!m_RootWidget)
        return false;

    FWidgetLayout rootLayout;
    rootLayout.width = FLayoutLength::Percent(100.0f);
    rootLayout.height = FLayoutLength::Percent(100.0f);
    m_RootWidget->SetLayout(rootLayout);
    m_RootWidget->SetStyleVariant("Root");
    m_RootWidget->SetContainerLayout(MakeContainerLayout(
        ELayoutDirection::Vertical, 8.0f, EAlignment::Stretch, EJustification::Start));

    FWidgetLayout toolbarLayout;
    toolbarLayout.width = FLayoutLength::Percent(100.0f);
    toolbarLayout.height = FLayoutLength::Units(54.0f);
    SetPadding(toolbarLayout, 0.0f, 14.0f, 0.0f, 14.0f);
    ContainerWidget* toolbar = AddContainer(
        *m_RootWidget,
        "Toolbar",
        toolbarLayout,
        MakeContainerLayout(ELayoutDirection::Horizontal, 12.0f, EAlignment::Center, EJustification::SpaceBetween));

    if (!toolbar)
        return false;

    FWidgetLayout brandLayout;
    brandLayout.width = FLayoutLength::Auto();
    brandLayout.height = FLayoutLength::Percent(100.0f);
    ContainerWidget* brand = AddContainer(
        *toolbar,
        nullptr,
        brandLayout,
        MakeContainerLayout(ELayoutDirection::Horizontal, 8.0f, EAlignment::Center));

    if (!brand || !AddText(*brand, "REDLEAF", "Accent") || !AddText(*brand, "STUDIO", "Header"))
        return false;

    FWidgetLayout statusPillLayout;
    statusPillLayout.width = FLayoutLength::Auto();
    statusPillLayout.height = FLayoutLength::Units(30.0f);
    SetPadding(statusPillLayout, 0.0f, 10.0f, 0.0f, 10.0f);
    ContainerWidget* statusPill = AddContainer(
        *toolbar,
        "StatusPill",
        statusPillLayout,
        MakeContainerLayout(ELayoutDirection::Horizontal, 8.0f, EAlignment::Center));

    if (!statusPill)
        return false;

    if (!AddText(*statusPill, "STYLE PLAYGROUND", "Kicker"))
        return false;

    m_EditorThemeLabel = AddText(*statusPill, "EDITOR / CYAN", "Kicker");

    FWidgetLayout bodyLayout;
    bodyLayout.width = FLayoutLength::Percent(100.0f);
    bodyLayout.height = FLayoutLength::Auto();
    bodyLayout.flexGrow = 1.0f;
    SetPadding(bodyLayout, 0.0f, 2.0f, 0.0f, 2.0f);
    ContainerWidget* body = AddContainer(
        *m_RootWidget,
        nullptr,
        bodyLayout,
        MakeContainerLayout(ELayoutDirection::Horizontal, 8.0f, EAlignment::Stretch));

    if (!body || !m_EditorThemeLabel)
        return false;

    FWidgetLayout sidebarLayout;
    sidebarLayout.width = FLayoutLength::Percent(20.0f);
    sidebarLayout.height = FLayoutLength::Percent(100.0f);
    SetPadding(sidebarLayout, 12.0f, 10.0f, 12.0f, 10.0f);
    ContainerWidget* sidebar = AddContainer(
        *body,
        "Panel",
        sidebarLayout,
        MakeContainerLayout(ELayoutDirection::Vertical, 8.0f));

    FWidgetLayout viewportLayout;
    viewportLayout.width = FLayoutLength::Percent(52.0f);
    viewportLayout.height = FLayoutLength::Percent(100.0f);
    SetPadding(viewportLayout, 12.0f, 10.0f, 10.0f, 10.0f);
    ContainerWidget* viewportPanel = AddContainer(
        *body,
        "Panel",
        viewportLayout,
        MakeContainerLayout(ELayoutDirection::Vertical, 8.0f));

    FWidgetLayout inspectorLayout;
    inspectorLayout.width = FLayoutLength::Percent(25.0f);
    inspectorLayout.height = FLayoutLength::Percent(100.0f);
    SetPadding(inspectorLayout, 12.0f, 10.0f, 10.0f, 10.0f);
    ContainerWidget* inspector = AddContainer(
        *body,
        "Panel",
        inspectorLayout,
        MakeContainerLayout(ELayoutDirection::Vertical, 8.0f));

    if (!sidebar || !viewportPanel || !inspector)
        return false;

    if (!AddText(*sidebar, "SCENE HIERARCHY", "PanelTitle") ||
        !AddText(*sidebar, "MAIN_LEVEL  /  04 OBJECTS", "Kicker"))
        return false;

    if (!AddSceneRow(*sidebar, "Camera", "", std::nullopt) ||
        !AddSceneRow(*sidebar, "Directional Light", "ACTIVE", EWidgetState::Selected) ||
        !AddSceneRow(*sidebar, "Player Character", "HOVER", EWidgetState::Hovered) ||
        !AddSceneRow(*sidebar, "Cube", "DISABLED", EWidgetState::Disabled))
        return false;

    FWidgetLayout dividerLayout;
    dividerLayout.width = FLayoutLength::Percent(100.0f);
    dividerLayout.height = FLayoutLength::Units(1.0f);

    if (!AddContainer(*sidebar, "Divider", dividerLayout, MakeContainerLayout(ELayoutDirection::Horizontal, 0.0f)))
        return false;

    if (!AddText(*sidebar, "STATE PREVIEW", "Kicker") ||
        !AddText(*sidebar, "Selected, hovered and disabled states are being previewed without input.", "Body"))
        return false;

    if (!AddText(*viewportPanel, "VIEWPORT", "PanelTitle") ||
        !AddText(*viewportPanel, "GAME SCENE  /  PERSPECTIVE", "Kicker"))
        return false;

    FResourcePath imageSource;

    if (!FResourcePath::TryParse("silverware:/Textures/test.png", imageSource))
        return false;

    auto viewportImage = Silverware::MakeUnique<ImageWidget>();
    viewportImage->SetStyleVariant("PreviewImage");

    FWidgetLayout imageLayout;
    imageLayout.width = FLayoutLength::Percent(100.0f);
    imageLayout.height = FLayoutLength::Units(300.0f);
    imageLayout.flexGrow = 1.0f;
    viewportImage->SetLayout(imageLayout);

    if (!viewportImage->SetSource(imageSource))
        return false;

    m_ViewportImage = viewportImage.get();

    if (!viewportPanel->AddChild(Silverware::TUniquePtr<CoreWidget>(viewportImage.release())))
        return false;

    if (!AddText(*viewportPanel, "VIEWPORT OUTPUT  /  EXTERNAL TEXTURE BRIDGE", "Mono"))
        return false;

    if (!AddText(*inspector, "INSPECTOR", "PanelTitle") ||
        !AddText(*inspector, "SELECTED OBJECT", "Kicker"))
        return false;

    FWidgetLayout selectedCardLayout;
    selectedCardLayout.width = FLayoutLength::Auto();
    selectedCardLayout.height = FLayoutLength::Auto();
    SetPadding(selectedCardLayout, 10.0f, 10.0f, 10.0f, 10.0f);
    ContainerWidget* selectedCard = AddContainer(
        *inspector,
        "Card",
        selectedCardLayout,
        MakeContainerLayout(ELayoutDirection::Vertical, 6.0f));

    if (!selectedCard ||
        !AddText(*selectedCard, "Directional Light", "Header") ||
        !AddText(*selectedCard, "LIGHT COMPONENT", "Kicker") ||
        !AddText(*selectedCard, "Position   0.0, 4.0, 0.0", "Mono") ||
        !AddText(*selectedCard, "Intensity  3.4 lux", "Mono"))
        return false;

    FWidgetLayout themeCardLayout;
    themeCardLayout.width = FLayoutLength::Auto();
    themeCardLayout.height = FLayoutLength::Auto();
    SetPadding(themeCardLayout, 10.0f, 10.0f, 10.0f, 10.0f);
    ContainerWidget* themeCard = AddContainer(
        *inspector,
        "Card",
        themeCardLayout,
        MakeContainerLayout(ELayoutDirection::Vertical, 6.0f));

    if (!themeCard ||
        !AddText(*themeCard, "THEME INHERITANCE", "Kicker") ||
        !AddText(*themeCard, "Base tokens + editor overrides", "Body"))
        return false;

    m_EditorThemeChainLabel = AddText(*themeCard, "BASE  >  EDITOR / CYAN", "Mono");

    if (!m_EditorThemeChainLabel || !AddText(*inspector, "LIVE STYLE RESOLUTION", "Success") ||
        !AddText(*inspector, "Theme changes are synchronized without rebuilding widgets.", "Body"))
        return false;

    FWidgetLayout footerLayout;
    footerLayout.width = FLayoutLength::Percent(100.0f);
    footerLayout.height = FLayoutLength::Units(28.0f);
    SetPadding(footerLayout, 0.0f, 12.0f, 0.0f, 12.0f);
    ContainerWidget* footer = AddContainer(
        *m_RootWidget,
        "Toolbar",
        footerLayout,
        MakeContainerLayout(ELayoutDirection::Horizontal, 12.0f, EAlignment::Center, EJustification::SpaceBetween));

    if (!footer || !AddText(*footer, "READY", "Success") ||
        !AddText(*footer, "RETAINED TREE  /  INCREMENTAL SYNC  /  02 UI CONTEXTS", "Mono"))
        return false;

    return true;
}

bool SilverwareEditorRenderer::BuildGameHUDLayout()
{
    using namespace Silverware;

    m_GameHUDRootWidget = Silverware::MakeUnique<ContainerWidget>();

    FWidgetLayout rootLayout;
    rootLayout.width = FLayoutLength::Percent(100.0f);
    rootLayout.height = FLayoutLength::Percent(100.0f);
    m_GameHUDRootWidget->SetLayout(rootLayout);
    m_GameHUDRootWidget->SetContainerLayout(MakeContainerLayout(
        ELayoutDirection::Horizontal, 0.0f, EAlignment::Start, EJustification::Start));

    FWidgetLayout cardLayout;
    cardLayout.width = FLayoutLength::Units(300.0f);
    cardLayout.height = FLayoutLength::Auto();
    SetPadding(cardLayout, 13.0f, 13.0f, 13.0f, 13.0f);

    m_GameHUDCard = AddContainer(
        *m_GameHUDRootWidget,
        "HudPanel",
        cardLayout,
        MakeContainerLayout(ELayoutDirection::Vertical, 8.0f));

    if (!m_GameHUDCard)
        return false;

    FWidgetLayout headerLayout;
    headerLayout.width = FLayoutLength::Percent(100.0f);
    headerLayout.height = FLayoutLength::Units(26.0f);
    ContainerWidget* header = AddContainer(
        *m_GameHUDCard,
        nullptr,
        headerLayout,
        MakeContainerLayout(ELayoutDirection::Horizontal, 8.0f, EAlignment::Center, EJustification::SpaceBetween));

    if (!header || !AddText(*header, "IN-GAME HUD", "HudTitle"))
        return false;

    m_GameHUDThemeLabel = AddText(*header, "TACTICAL", "Kicker");

    FWidgetLayout dividerLayout;
    dividerLayout.width = FLayoutLength::Percent(100.0f);
    dividerLayout.height = FLayoutLength::Units(1.0f);

    if (!m_GameHUDThemeLabel ||
        !AddContainer(*m_GameHUDCard, "Divider", dividerLayout, MakeContainerLayout(ELayoutDirection::Horizontal, 0.0f)))
        return false;

    FWidgetLayout playerCardLayout;
    playerCardLayout.width = FLayoutLength::Percent(100.0f);
    playerCardLayout.height = FLayoutLength::Units(38.0f);
    SetPadding(playerCardLayout, 0.0f, 8.0f, 0.0f, 8.0f);
    ContainerWidget* playerCard = AddContainer(
        *m_GameHUDCard,
        "HudChip",
        playerCardLayout,
        MakeContainerLayout(ELayoutDirection::Horizontal, 8.0f, EAlignment::Center, EJustification::SpaceBetween));

    if (!playerCard || !AddText(*playerCard, "PLAYER ONE", "Kicker") ||
        !AddText(*playerCard, "LV 08", "Accent"))
        return false;

    ContainerWidget* healthHeader = AddContainer(
        *m_GameHUDCard,
        nullptr,
        headerLayout,
        MakeContainerLayout(ELayoutDirection::Horizontal, 8.0f, EAlignment::Center, EJustification::SpaceBetween));

    if (!healthHeader || !AddText(*healthHeader, "VITALS", "Kicker"))
        return false;

    m_GameHUDHealthLabel = AddText(*healthHeader, "HP 78 / 100", "HudMetric");

    FWidgetLayout meterLayout;
    meterLayout.width = FLayoutLength::Percent(100.0f);
    meterLayout.height = FLayoutLength::Units(8.0f);
    ContainerWidget* healthTrack = AddContainer(
        *m_GameHUDCard,
        "HealthTrack",
        meterLayout,
        MakeContainerLayout(ELayoutDirection::Horizontal, 0.0f));

    if (!healthTrack || !m_GameHUDHealthLabel)
        return false;

    FWidgetLayout healthFillLayout;
    healthFillLayout.width = FLayoutLength::Percent(78.0f);
    healthFillLayout.height = FLayoutLength::Percent(100.0f);
    m_GameHUDHealthFill = AddContainer(
        *healthTrack,
        "HealthFill",
        healthFillLayout,
        MakeContainerLayout(ELayoutDirection::Horizontal, 0.0f));

    ContainerWidget* shieldHeader = AddContainer(
        *m_GameHUDCard,
        nullptr,
        headerLayout,
        MakeContainerLayout(ELayoutDirection::Horizontal, 8.0f, EAlignment::Center, EJustification::SpaceBetween));

    if (!m_GameHUDHealthFill || !shieldHeader ||
        !AddText(*shieldHeader, "SHIELD", "Kicker") ||
        !AddText(*shieldHeader, "64%", "Mono"))
        return false;

    ContainerWidget* shieldTrack = AddContainer(
        *m_GameHUDCard,
        "HealthTrack",
        meterLayout,
        MakeContainerLayout(ELayoutDirection::Horizontal, 0.0f));

    if (!shieldTrack)
        return false;

    FWidgetLayout shieldFillLayout;
    shieldFillLayout.width = FLayoutLength::Percent(64.0f);
    shieldFillLayout.height = FLayoutLength::Percent(100.0f);
    m_GameHUDShieldFill = AddContainer(
        *shieldTrack,
        "ShieldFill",
        shieldFillLayout,
        MakeContainerLayout(ELayoutDirection::Horizontal, 0.0f));

    FWidgetLayout footerLayout;
    footerLayout.width = FLayoutLength::Percent(100.0f);
    footerLayout.height = FLayoutLength::Units(20.0f);
    ContainerWidget* footer = AddContainer(
        *m_GameHUDCard,
        nullptr,
        footerLayout,
        MakeContainerLayout(ELayoutDirection::Horizontal, 8.0f, EAlignment::Center, EJustification::SpaceBetween));

    if (!m_GameHUDShieldFill || !footer || !AddText(*footer, "ZONE / IRON HARBOR", "Mono"))
        return false;

    m_GameHUDClockLabel = AddText(*footer, "MATCH 00:00", "Success");
    return m_GameHUDClockLabel != nullptr;
}

void SilverwareEditorRenderer::UpdateThemeDemo(float deltaTime)
{
    if (!std::isfinite(deltaTime) || deltaTime <= 0.0f)
        return;

    m_EditorThemeSwitchElapsed += deltaTime;

    while (m_EditorThemeSwitchElapsed >= kEditorThemeSwitchIntervalSeconds)
    {
        m_EditorThemeSwitchElapsed -= kEditorThemeSwitchIntervalSeconds;
        const bool useAmber = !m_UseAmberEditorTheme;
        const char* themeID = useAmber ? kEditorAmberThemeID : kEditorCyanThemeID;
        std::string error;

        if (!m_Backend || !m_Backend->SetActiveTheme(themeID, &error))
        {
            std::cerr << "[SilverwarePlayground]: Editor theme switch failed: " << error << '\n';
            continue;
        }

        m_UseAmberEditorTheme = useAmber;

        if (m_EditorThemeLabel)
            m_EditorThemeLabel->SetText(useAmber ? "EDITOR / AMBER" : "EDITOR / CYAN");

        if (m_EditorThemeChainLabel)
            m_EditorThemeChainLabel->SetText(useAmber ? "BASE  >  EDITOR / AMBER" : "BASE  >  EDITOR / CYAN");

        std::cout << "[SilverwarePlayground]: Editor theme switched to " << themeID << ".\n";
    }

    m_GameHUDThemeSwitchElapsed += deltaTime;

    while (m_GameHUDThemeSwitchElapsed >= kHUDThemeSwitchIntervalSeconds)
    {
        m_GameHUDThemeSwitchElapsed -= kHUDThemeSwitchIntervalSeconds;
        const bool useOverdrive = !m_UseOverdriveHUDTheme;
        const char* themeID = useOverdrive ? kHUDOverdriveThemeID : kHUDTacticalThemeID;
        std::string error;

        if (!m_GameHUDUIContext || !m_GameHUDUIContext->SetActiveTheme(themeID, &error))
        {
            std::cerr << "[SilverwarePlayground]: HUD theme switch failed: " << error << '\n';
            continue;
        }

        m_UseOverdriveHUDTheme = useOverdrive;

        if (m_GameHUDThemeLabel)
            m_GameHUDThemeLabel->SetText(useOverdrive ? "OVERDRIVE" : "TACTICAL");

        std::cout << "[SilverwarePlayground]: HUD theme switched to " << themeID << ".\n";
    }
}

void SilverwareEditorRenderer::UpdateHUDTelemetry(float deltaTime)
{
    if (!std::isfinite(deltaTime) || deltaTime <= 0.0f)
        return;

    m_HUDClockSeconds += deltaTime;
    m_HUDTelemetryElapsed += deltaTime;

    if (m_HUDTelemetryElapsed < kHUDTelemetryIntervalSeconds)
        return;

    m_HUDTelemetryElapsed = std::fmod(m_HUDTelemetryElapsed, kHUDTelemetryIntervalSeconds);
    const int health = 78 + static_cast<int>(12.0f * std::sin(m_HUDClockSeconds * 0.85f));

    if (m_GameHUDHealthLabel)
        m_GameHUDHealthLabel->SetText("HP " + std::to_string(health) + " / 100");

    if (m_GameHUDHealthFill)
    {
        FWidgetLayout layout = m_GameHUDHealthFill->GetLayout();
        layout.width = Silverware::FLayoutLength::Percent(static_cast<float>(health));
        m_GameHUDHealthFill->SetLayout(layout);
    }

    if (m_GameHUDClockLabel)
    {
        const int totalSeconds = static_cast<int>(m_HUDClockSeconds) % 3600;
        char label[32];
        std::snprintf(label, sizeof(label), "MATCH %02d:%02d", totalSeconds / 60, totalSeconds % 60);
        m_GameHUDClockLabel->SetText(label);
    }
}

void SilverwareEditorRenderer::UpdateGameHUDLayout(int width, int height)
{
    if (!m_GameHUDCard || width <= 0 || height <= 0)
        return;

    // Keep the HUD visually inside the editor's central viewport panel for this playground.
    auto layout = m_GameHUDCard->GetLayout();
    layout.margin.left = Silverware::FLayoutLength::Units(std::max(12.0f, width * 0.72f - 330.0f));
    layout.margin.top = Silverware::FLayoutLength::Units(72.0f);
    m_GameHUDCard->SetLayout(layout);
}

void SilverwareEditorRenderer::RenderGameHUDContext(IPlatformWindow& window)
{
    if (!m_GameHUDUIContext || !m_GameHUDRootWidget)
        return;

    int width = 0;
    int height = 0;
    window.GetFramebufferSize(width, height);

    if (width <= 0 || height <= 0)
        return;

    if (width != m_GameHUDContextWidth || height != m_GameHUDContextHeight)
    {
        m_GameHUDUIContext->Resize(width, height);
        UpdateGameHUDLayout(width, height);
        m_GameHUDContextWidth = width;
        m_GameHUDContextHeight = height;
    }

    // Synchronization is cheap when the tree is clean, and is required after theme changes.
    if (!m_GameHUDUIContext->Synchronize(*m_GameHUDRootWidget, Silverware::EBackendSyncMode::Incremental))
        std::cerr << "[SilverwarePlayground]: Game HUD synchronization failed.\n";

    m_GameHUDUIContext->Update();
    m_GameHUDUIContext->Render();
}

void SilverwareEditorRenderer::Tick(float deltaTime)
{
    if (!m_Backend || !m_RootWidget || !m_EditorHost || !m_ViewportImage || !m_GameHUDUIContext)
        return;

    UpdateThemeDemo(deltaTime);
    UpdateHUDTelemetry(deltaTime);

    auto& viewportSubsystem = m_EditorHost->GetSubsystem<ViewportSubsystem>();
    const FViewportOutput* output = viewportSubsystem.GetOutput(kViewportPanelKey);

    if (output && output->textureHandle.IsValid() && output->width > 0 && output->height > 0)
    {
        // This bridge intentionally uses the engine handle ID, not output->nativeTexture.
        const Silverware::FExternalTextureReference texture{
            static_cast<std::uint64_t>(output->textureHandle.id)
        };

        if (!m_ViewportImage->SetExternalTexture(texture, output->width, output->height))
        {
            std::cerr << "[SilverwarePlayground]: Failed to assign viewport texture.\n";
            m_ViewportImage->ClearSource();
        }
    }
    else
    {
        m_ViewportImage->ClearSource();
    }

    float viewportWidth = 0.0f;
    float viewportHeight = 0.0f;

    if (!m_Backend->TryGetWidgetSize(*m_ViewportImage, viewportWidth, viewportHeight))
    {
        viewportWidth = 0.0f;
        viewportHeight = 0.0f;
    }

    SubmitViewportInput(viewportSubsystem, viewportWidth, viewportHeight);

    if (!m_Backend->Synchronize(*m_RootWidget, Silverware::EBackendSyncMode::Incremental))
        std::cerr << "[SilverwarePlayground]: Editor synchronization failed.\n";
}

void SilverwareEditorRenderer::Shutdown()
{
    if (m_GameHUDUIContext)
    {
        Silverware::SilverwareHost* host = m_Backend ? m_Backend->GetHost() : nullptr;

        if (host)
        {
            std::string error;

            if (!host->DestroyUIContext(kGameHUDContextID, &error))
            {
                std::cerr << "[SilverwarePlayground]: Failed to destroy HUD context: " << error << '\n';
                host->Shutdown();
            }
        }

        m_GameHUDUIContext = nullptr;
    }

    m_GameHUDCard = nullptr;
    m_GameHUDHealthFill = nullptr;
    m_GameHUDShieldFill = nullptr;
    m_GameHUDThemeLabel = nullptr;
    m_GameHUDHealthLabel = nullptr;
    m_GameHUDClockLabel = nullptr;
    m_GameHUDRootWidget.reset();

    // The rendering context is borrowed by the UI context, so release it after that context.
    m_GameHUDRenderingContext.reset();

    if (m_EditorHost)
        m_EditorHost->GetSubsystem<ViewportSubsystem>().Destroy(kViewportPanelKey);

    m_ViewportImage = nullptr;
    m_EditorThemeLabel = nullptr;
    m_EditorThemeChainLabel = nullptr;
    m_RootWidget.reset();

    m_Backend = nullptr;
    m_EditorRuntime = nullptr;
    m_EditorHost = nullptr;

    m_GameHUDContextWidth = 0;
    m_GameHUDContextHeight = 0;
    m_EditorThemeSwitchElapsed = 0.0f;
    m_GameHUDThemeSwitchElapsed = 0.0f;
    m_HUDTelemetryElapsed = 0.0f;
    m_HUDClockSeconds = 0.0f;
    m_UseAmberEditorTheme = false;
    m_UseOverdriveHUDTheme = false;
}
