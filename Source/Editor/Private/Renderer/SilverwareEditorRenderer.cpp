// Copyright 2026 JesseTheCatLover. All Rights Reserved.

#include "SilverwareEditorRenderer.h"

#include "Backends/SilverwareBackend.h"

#include "Silverware/Core/Styling/StyleModel.h"
#include "Silverware/Core/Widgets/ContainerWidget.h"
#include "Silverware/Core/Widgets/CoreWidget.h"
#include "Silverware/Core/Widgets/WidgetLayout.h"
#include "Silverware/UI/TextWidget.h"

#include <iostream>
#include <string>
#include <utility>

#include "Silverware/UI/ButtonWidget.h"

namespace
{
    using namespace Silverware;

    constexpr char kBaseThemeID[] = "Redleaf.InkBlue.Base";
    constexpr char kDarkThemeID[] = "Redleaf.InkBlue.Dark";
    constexpr char kLightThemeID[] = "Redleaf.InkBlue.Light";

    constexpr char kBaseStylesheetPath[] = "engine:/Editor/Stylesheets/InkBlue.Base.swstyle";
    constexpr char kDarkStylesheetPath[] = "engine:/Editor/Stylesheets/InkBlue.Dark.swstyle";
    constexpr char kLightStylesheetPath[] = "engine:/Editor/Stylesheets/InkBlue.Light.swstyle";

    bool LoadPlaygroundStylesheet(
        Silverware::SilverwareUIContext& context,
        const char* virtualPath)
    {
        Silverware::FResourcePath path;

        if (!Silverware::FResourcePath::TryParse(virtualPath, path))
        {
            std::cerr << "[SilverwareRenderer]: Invalid stylesheet path: "
                      << virtualPath << '\n';
            return false;
        }

        const Silverware::FStylesheetLoadResult result = context.LoadStylesheet(path);

        if (!result.Succeeded())
        {
            std::cerr << "[SilverwareRenderer]: Failed to load stylesheet "
                      << virtualPath << ": " << result.message;

            if (result.status == Silverware::EStylesheetLoadStatus::ParseError)
                std::cerr << " (line " << result.parseError.line
                          << ", column " << result.parseError.column << ')';

            std::cerr << '\n';
            return false;
        }

        return true;
    }

    constexpr char kPlaygroundThemeID[] = "Redleaf.SilverwarePlayground";

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

    FThemeDefinition MakePlaygroundTheme()
    {
        FThemeDefinition theme;
        theme.ID = kPlaygroundThemeID;

        AddToken(theme, "color.canvas", FColor{0.035f, 0.043f, 0.063f, 1.0f});
        AddToken(theme, "color.surface", FColor{0.065f, 0.078f, 0.106f, 1.0f});
        AddToken(theme, "color.surfaceRaised", FColor{0.090f, 0.108f, 0.145f, 1.0f});
        AddToken(theme, "color.border", FColor{0.18f, 0.22f, 0.29f, 1.0f});
        AddToken(theme, "color.text", FColor{0.91f, 0.93f, 0.97f, 1.0f});
        AddToken(theme, "color.muted", FColor{0.57f, 0.62f, 0.70f, 1.0f});
        AddToken(theme, "color.accent", FColor{0.25f, 0.80f, 1.0f, 1.0f});
        AddToken(theme, "color.accentSoft", FColor{0.08f, 0.34f, 0.46f, 0.42f});
        AddToken(theme, "color.success", FColor{0.30f, 0.88f, 0.62f, 1.0f});

        AddToken(theme, "font.family.body", std::string("LatoLatin"));
        AddToken(theme, "font.family.mono", std::string("Roboto Mono"));

        AddVariant(theme, "Container", "Root", "Playground root");
        AddVariant(theme, "Container", "Toolbar", "Toolbar");
        AddVariant(theme, "Container", "Panel", "Playground panel");
        AddVariant(theme, "Container", "PlaygroundArea", "Widget test area");
        AddVariant(theme, "Container", "StatusPill", "Status badge");

        AddVariant(theme, "Text", "Header", "Header");
        AddVariant(theme, "Text", "PanelTitle", "Panel title");
        AddVariant(theme, "Text", "Body", "Body text");
        AddVariant(theme, "Text", "Kicker", "Metadata");
        AddVariant(theme, "Text", "Accent", "Accent text");
        AddVariant(theme, "Text", "Mono", "Monospaced metadata");
        AddVariant(theme, "Text", "Success", "Success text");

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

        AddRule(theme, "Container", "PlaygroundArea", 0, {
            {"background-color", Token("color.surfaceRaised")},
            {"border-color", Token("color.border")},
            {"border-width", 1.0f},
            {"border-style", EBorderStyle::Solid},
            {"border-radius", 8.0f}
        });

        AddRule(theme, "Container", "StatusPill", 0, {
            {"background-color", Token("color.accentSoft")},
            {"border-color", Token("color.accent")},
            {"border-width", 1.0f},
            {"border-style", EBorderStyle::Solid},
            {"border-radius", 99.0f}
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
            {"font-size", 14.0f},
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

        AddRule(theme, "Text", "Mono", 0, {
            {"color", Token("color.muted")},
            {"font-family", Token("font.family.mono")},
            {"font-size", 10.0f}
        });

        AddRule(theme, "Text", "Success", 0, {
            {"color", Token("color.success")},
            {"font-weight", EFontWeight::Bold}
        });

        return theme;
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
        const char* variant)
    {
        using namespace Silverware;

        auto widget = Silverware::MakeUnique<TextWidget>(std::move(text));

        if (variant && *variant)
            widget->SetStyleVariant(variant);

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

    Silverware::ButtonWidget* AddButton(
    Silverware::ContainerWidget& parent,
    const char* variant,
    Silverware::FWidgetLayout layout,
    std::string text)
    {
        using namespace Silverware;

        auto button = Silverware::MakeUnique<ButtonWidget>();

        if (variant && *variant)
            button->SetStyleVariant(variant);

        button->SetLayout(layout);
        button->SetContainerLayout(MakeContainerLayout(
            ELayoutDirection::Horizontal, 0.0f,
            EAlignment::Center, EJustification::Center));

        auto label = Silverware::MakeUnique<TextWidget>(Silverware::TakeUniqueOwnership(text));

        if (!button->AddChild(Silverware::TUniquePtr<CoreWidget>(label.release())))
            return nullptr;

        ButtonWidget* rawButton = button.get();

        if (!parent.AddChild(Silverware::TUniquePtr<CoreWidget>(button.release())))
            return nullptr;

        return rawButton;
    }
}

SilverwareEditorRenderer::SilverwareEditorRenderer()
{
}

SilverwareEditorRenderer::~SilverwareEditorRenderer()
{
    Shutdown();
}

bool SilverwareEditorRenderer::Initialize(SilverwareBackend& backend)
{
    Shutdown();

    if (!backend.IsInitialized())
    {
        std::cerr << "[SilverwareRenderer]: Backend is not initialized.\n";
        return false;
    }

    m_Backend = &backend;
    m_RootWidget = Silverware::MakeUnique<Silverware::ContainerWidget>();

    if (!BuildPlaygroundLayout() || !RegisterPlaygroundTheme())
    {
        std::cerr << "[SilverwareRenderer]: Failed to build the interaction playground.\n";
        Shutdown();
        return false;
    }

    if (!m_Backend->Synchronize(*m_RootWidget, Silverware::EBackendSyncMode::Full))
    {
        std::cerr << "[SilverwareRenderer]: Initial synchronization failed.\n";
        Shutdown();
        return false;
    }

    return true;
}

bool SilverwareEditorRenderer::RegisterPlaygroundTheme()
{
    if (!m_Backend)
        return false;

    Silverware::SilverwareUIContext* context = m_Backend->GetEditorUIContext();

    if (!context)
        return false;

    // Register the base before its appearance themes.
    if (!LoadPlaygroundStylesheet(*context, kBaseStylesheetPath) ||
        !LoadPlaygroundStylesheet(*context, kDarkStylesheetPath) ||
        !LoadPlaygroundStylesheet(*context, kLightStylesheetPath))
        return false;

    std::string error;

    if (!m_Backend->RegisterTheme(MakePlaygroundTheme(), &error))
    {
        std::cerr << "[SilverwareRenderer]: Theme registration failed: " << error << '\n';
        return false;
    }

    if (!m_Backend->SetActiveTheme(kDarkThemeID, &error))
    {
        std::cerr << "[SilverwareRenderer]: Failed to activate InkBlue Dark: "
                  << error << '\n';
        return false;
    }

    return true;
}

bool SilverwareEditorRenderer::BuildPlaygroundLayout()
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
        ELayoutDirection::Vertical, 8.0f));

    // Header
    FWidgetLayout toolbarLayout;
    toolbarLayout.height = FLayoutLength::Units(54.0f);
    SetPadding(toolbarLayout, 0.0f, 14.0f, 0.0f, 14.0f);

    ContainerWidget* toolbar = AddContainer(
        *m_RootWidget,
        "Toolbar",
        toolbarLayout,
        MakeContainerLayout(
            ELayoutDirection::Horizontal, 12.0f,
            EAlignment::Center, EJustification::SpaceBetween));

    if (!toolbar ||
        !AddText(*toolbar, "SILVERWARE", "Accent") ||
        !AddText(*toolbar, "INTERACTION PLAYGROUND", "Header"))
        return false;

    FWidgetLayout statusLayout;
    statusLayout.height = FLayoutLength::Units(30.0f);
    SetPadding(statusLayout, 0.0f, 10.0f, 0.0f, 10.0f);

    ContainerWidget* statusPill = AddContainer(
        *toolbar,
        "StatusPill",
        statusLayout,
        MakeContainerLayout(ELayoutDirection::Horizontal, 0.0f, EAlignment::Center));

    if (!statusPill || !AddText(*statusPill, "INPUT ROUTER", "Kicker"))
        return false;

    // Main content
    FWidgetLayout bodyLayout;
    bodyLayout.height = FLayoutLength::Auto();
    bodyLayout.flexGrow = 1.0f;

    ContainerWidget* body = AddContainer(
        *m_RootWidget,
        nullptr,
        bodyLayout,
        MakeContainerLayout(ELayoutDirection::Horizontal, 8.0f));

    if (!body)
        return false;

    FWidgetLayout panelLayout;
    panelLayout.width = FLayoutLength::Units(0.0f);
    panelLayout.height = FLayoutLength::Percent(100.0f);
    panelLayout.flexGrow = 1.0f;
    SetPadding(panelLayout, 14.0f, 14.0f, 14.0f, 14.0f);

    ContainerWidget* buttonPanel = AddContainer(
        *body,
        "Panel",
        panelLayout,
        MakeContainerLayout(ELayoutDirection::Vertical, 10.0f));

    ContainerWidget* blockPanel = AddContainer(
        *body,
        "Panel",
        panelLayout,
        MakeContainerLayout(ELayoutDirection::Vertical, 10.0f));

    if (!buttonPanel || !blockPanel)
        return false;

    if (!AddText(*buttonPanel, "BUTTONS", "PanelTitle") ||
        !AddText(*buttonPanel, "Button interaction, focus and visual states.", "Body") ||
        !AddText(*blockPanel, "INTERACTIVE BLOCKS", "PanelTitle") ||
        !AddText(*blockPanel, "Pointer interaction, event propagation and capture.", "Body"))
        return false;

    FWidgetLayout areaLayout;
    areaLayout.height = FLayoutLength::Auto();
    areaLayout.flexGrow = 1.0f;
    SetPadding(areaLayout, 12.0f, 12.0f, 12.0f, 12.0f);

    m_ButtonPlayground = AddContainer(
        *buttonPanel,
        "PlaygroundArea",
        areaLayout,
        MakeContainerLayout(ELayoutDirection::Vertical, 8.0f));

    m_InteractiveBlockPlayground = AddContainer(
        *blockPanel,
        "PlaygroundArea",
        areaLayout,
        MakeContainerLayout(ELayoutDirection::Vertical, 8.0f));

    if (!m_ButtonPlayground || !m_InteractiveBlockPlayground)
        return false;

    // Button examples
    FWidgetLayout buttonLayout;
    buttonLayout.width = FLayoutLength::Units(150.0f);
    buttonLayout.height = FLayoutLength::Units(38.0f);
    SetPadding(buttonLayout, 8.0f, 14.0f, 8.0f, 14.0f);

    ButtonWidget* primaryButton = AddButton(
        *m_ButtonPlayground,
        "Primary",
        buttonLayout,
        "Click me!");

    if (!primaryButton)
        return false;

    m_PrimaryButtonClickConnection = primaryButton->OnClick().Connect(
        [](FClickEvent& event)
        {
            event.MarkHandled();
            std::cout << "[SilverwarePlayground]: Primary button clicked.\n";
        });

    // Footer
    FWidgetLayout footerLayout;
    footerLayout.height = FLayoutLength::Units(28.0f);
    SetPadding(footerLayout, 0.0f, 12.0f, 0.0f, 12.0f);

    ContainerWidget* footer = AddContainer(
        *m_RootWidget,
        "Toolbar",
        footerLayout,
        MakeContainerLayout(
            ELayoutDirection::Horizontal, 12.0f,
            EAlignment::Center, EJustification::SpaceBetween));

    if (!footer ||
        !AddText(*footer, "READY", "Success") ||
        !AddText(*footer, "RETAINED WIDGET TREE  /  INPUT ROUTER  /  INCREMENTAL SYNC", "Mono"))
        return false;

    return true;
}

void SilverwareEditorRenderer::Tick(float deltaTime)
{
    if (!m_Backend || !m_RootWidget)
        return;

    if (!m_Backend->Synchronize(*m_RootWidget, Silverware::EBackendSyncMode::Incremental))
        std::cerr << "[SilverwareRenderer]: Incremental synchronization failed.\n";
}

void SilverwareEditorRenderer::Shutdown()
{
    if (m_Backend && m_Backend->IsInitialized())
    {
        m_Backend->ClearActiveTheme();
        (void)m_Backend->UnregisterTheme(kPlaygroundThemeID);
        // Unregister children before their shared parent.
        (void)m_Backend->UnregisterTheme(kDarkThemeID);
        (void)m_Backend->UnregisterTheme(kLightThemeID);
        (void)m_Backend->UnregisterTheme(kBaseThemeID);
    }

    m_PrimaryButtonClickConnection.Disconnect();

    m_ButtonPlayground = nullptr;
    m_InteractiveBlockPlayground = nullptr;
    m_RootWidget.reset();
    m_Backend = nullptr;
}