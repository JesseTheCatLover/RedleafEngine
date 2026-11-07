
// Copyright 2026 JesseTheCatLover. All Rights Reserved.

#include "SilverwareEditorRenderer.h"

#include "Backends/SilverwareBackend.h"

#include "Silverware/Core/Styling/StyleModel.h"
#include "Silverware/Core/Widgets/ContainerWidget.h"
#include "Silverware/Core/Widgets/CoreWidget.h"
#include "Silverware/Core/Widgets/WidgetLayout.h"
#include "Silverware/Core/Widgets/Styling/TextStyle.h"
#include "Silverware/UI/TextWidget.h"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "Silverware/UI/ButtonWidget.h"
#include "Silverware/UI/ImageWidget.h"

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

    struct FTypographySample
    {
        Silverware::TextWidget* widget = nullptr;
        Silverware::FTextStyle baseStyle;
    };

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

    Silverware::TextWidget* AddStyledText(
        Silverware::ContainerWidget& parent,
        std::string text,
        Silverware::FTextStyle style)
    {
        using namespace Silverware;

        auto widget = Silverware::MakeUnique<TextWidget>(std::move(text));

        FWidgetLayout layout;
        layout.width = FLayoutLength::Percent(100.0f);
        layout.height = FLayoutLength::Auto();
        widget->SetLayout(layout);
        widget->SetTextStyle(std::move(style));

        TextWidget* rawWidget = widget.get();

        if (!parent.AddChild(Silverware::TUniquePtr<CoreWidget>(widget.release())))
            return nullptr;

        return rawWidget;
    }

    bool AddTypographySamples(
        Silverware::ContainerWidget& parent,
        std::vector<FTypographySample>& outSamples)
    {
        using namespace Silverware;

        const auto addHeading = [&parent, &outSamples](const char* text)
        {
            TextWidget* widget = AddText(parent, text, "Kicker");

            if (!widget)
                return false;

            outSamples.push_back({widget, FTextStyle{}});
            return true;
        };

        const auto addSample = [&parent, &outSamples](const char* text, FTextStyle style)
        {
            FTextStyle baseStyle = style;
            TextWidget* widget = AddStyledText(parent, text, std::move(style));

            if (!widget)
                return false;

            outSamples.push_back({widget, std::move(baseStyle)});
            return true;
        };

        if (!addHeading("FONT WEIGHT:"))
            return false;

        const auto addWeight = [&addSample](const char* label, EFontWeight weight)
        {
            FTextStyle style;
            style.fontSize = 12.0f;
            style.fontWeight = weight;
            return addSample(label, std::move(style));
        };

        if (!addWeight("Weight 100 - Thin", EFontWeight::Thin) ||
            !addWeight("Weight 300 - Light", EFontWeight::Light) ||
            !addWeight("Weight 400 - Normal", EFontWeight::Normal) ||
            !addWeight("Weight 600 - SemiBold", EFontWeight::SemiBold) ||
            !addWeight("Weight 700 - Bold", EFontWeight::Bold) ||
            !addWeight("Weight 900 - Black", EFontWeight::Black))
            return false;

        if (!addHeading("FONT STYLE:"))
            return false;

        FTextStyle normalStyle;
        normalStyle.fontSize = 14.0f;
        normalStyle.fontWeight = EFontWeight::SemiBold;
        normalStyle.fontStyle = EFontStyle::Normal;

        FTextStyle italicStyle;
        italicStyle.fontSize = 14.0f;
        italicStyle.fontWeight = EFontWeight::SemiBold;
        italicStyle.fontStyle = EFontStyle::Italic;

        if (!addSample("Normal font face", std::move(normalStyle)) ||
            !addSample("Italic font face", std::move(italicStyle)))
            return false;

        if (!addHeading("FONT KERNING:"))
            return false;

        FTextStyle kerningAuto;
        kerningAuto.fontSize = 17.0f;
        kerningAuto.fontKerning = EFontKerning::Auto;

        FTextStyle kerningNormal;
        kerningNormal.fontSize = 17.0f;
        kerningNormal.fontKerning = EFontKerning::Normal;

        FTextStyle kerningNone;
        kerningNone.fontSize = 17.0f;
        kerningNone.fontKerning = EFontKerning::None;

        if (!addSample("AVATAR To Wa - kerning auto", std::move(kerningAuto)) ||
            !addSample("AVATAR To Wa - kerning normal", std::move(kerningNormal)) ||
            !addSample("AVATAR To Wa - kerning none", std::move(kerningNone)))
            return false;

        if (!addHeading("LETTER SPACING:"))
            return false;

        FTextStyle normalSpacing;
        normalSpacing.letterSpacing = FLetterSpacing::Normal();

        FTextStyle wideSpacing;
        wideSpacing.letterSpacing = FLetterSpacing::Units(1.5f);

        FTextStyle tightSpacing;
        tightSpacing.letterSpacing = FLetterSpacing::Units(-0.5f);

        if (!addSample("SILVERWARE - normal spacing", std::move(normalSpacing)) ||
            !addSample("SILVERWARE - wide spacing", std::move(wideSpacing)) ||
            !addSample("SILVERWARE - tight spacing", std::move(tightSpacing)))
            return false;

        if (!addHeading("LINE HEIGHT:"))
            return false;

        FTextStyle compactLineHeight;
        compactLineHeight.lineHeight = FLineHeight::Multiplier(1.0f);

        FTextStyle relaxedLineHeight;
        relaxedLineHeight.lineHeight = FLineHeight::Multiplier(1.55f);

        if (!addSample("1.0 - Line height controls the rhythm between wrapped lines in a text block. This is an example.",
                       std::move(compactLineHeight)) ||
            !addSample("1.55 - Line height controls the rhythm between wrapped lines in a text block. This is an exmaple.",
                       std::move(relaxedLineHeight)))
            return false;

        if (!addHeading("TEXT DECORATION:"))
            return false;

        FTextStyle underline;
        underline.textDecoration = ETextDecoration::Underline;

        FTextStyle overline;
        overline.textDecoration = ETextDecoration::Overline;

        FTextStyle lineThrough;
        lineThrough.textDecoration = ETextDecoration::LineThrough;

        if (!addSample("Underline decoration", std::move(underline)) ||
            !addSample("Overline decoration", std::move(overline)) ||
            !addSample("Line-through decoration", std::move(lineThrough)))
            return false;

        if (!addHeading("TEXT TRANSFORM:"))
            return false;

        FTextStyle uppercase;
        uppercase.textTransform = ETextTransform::Uppercase;

        FTextStyle lowercase;
        lowercase.textTransform = ETextTransform::Lowercase;

        FTextStyle capitalize;
        capitalize.textTransform = ETextTransform::Capitalize;

        constexpr char kTransformSample[] = "Silverware typography sample";

        if (!addSample(("Uppercase: " + std::string(kTransformSample)).c_str(), std::move(uppercase)) ||
            !addSample(("Lowercase: " + std::string(kTransformSample)).c_str(), std::move(lowercase)) ||
            !addSample(("Capitalize: " + std::string(kTransformSample)).c_str(), std::move(capitalize)))
            return false;

        return true;
    }

    void SetPadding(Silverware::FWidgetLayout &layout, float top, float right, float bottom, float left)
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

     bool AddOverflowTextCase(
        ContainerWidget &parent,
        const char *label,
        EOverflowMode overflowX,
        EOverflowMode overflowY,
        EClipMode clipMode,
        EWhiteSpace whiteSpace,
        std::string text,
        bool translateText = false,
        bool ignoreAncestorClip = false)
    {
        if (!AddText(parent, label, "Kicker"))
            return false;

        FWidgetLayout frameLayout;
        frameLayout.width = FLayoutLength::Percent(65.0f);
        frameLayout.height = FLayoutLength::Units(52.0f);
        SetPadding(frameLayout, 5.0f, 6.0f, 5.0f, 6.0f);

        ContainerWidget *frame = AddContainer(
            parent,
            nullptr,
            frameLayout,
            MakeContainerLayout(
                ELayoutDirection::Vertical,
                0.0f,
                EAlignment::Start,
                EJustification::Start));

        if (!frame)
            return false;

        FWidgetVisualStyle frameVisualStyle;
        frameVisualStyle.backgroundColor = FColor{0.055f, 0.070f, 0.095f, 1.0f};
        frameVisualStyle.borderColor = FColor{0.25f, 0.32f, 0.42f, 1.0f};
        frameVisualStyle.borderWidth = 1.0f;
        frameVisualStyle.cornerRadius = 6.0f;

        frame->SetVisualStyle(std::move(frameVisualStyle));

        FWidgetOverflowStyle overflowStyle;
        overflowStyle.overflowX = overflowX;
        overflowStyle.overflowY = overflowY;
        overflowStyle.clip = clipMode;

        frame->SetOverflowStyle(std::move(overflowStyle));

        FTextStyle textStyle;
        textStyle.fontSize = 11.0f;
        textStyle.whiteSpace = whiteSpace;

        TextWidget *content = AddStyledText(
            *frame,
            std::move(text),
            std::move(textStyle));

        if (!content)
            return false;

        if (translateText)
        {
            FWidgetTransformStyle transformStyle;
            transformStyle.operations = FTransformOperationList{
                FTranslateTransform{
                    FLayoutLength::Units(180.0f),
                    FLayoutLength::Units(0.0f)
                }
            };

            content->SetTransformStyle(std::move(transformStyle));
        }

        if (ignoreAncestorClip)
        {
            FWidgetOverflowStyle childOverflowStyle;
            childOverflowStyle.clip = EClipMode::None;
            content->SetOverflowStyle(std::move(childOverflowStyle));
        }

        return true;
    }

    bool AddOverflowSamples(ContainerWidget &parent)
    {
        using namespace Silverware;

        constexpr char kLongLine[] =
                "This is deliberately long content that should extend beyond "
                "the right boundary of its containing widget. It keeps going "
                "to make overflow behavior easy to observe.";

        if (!AddOverflowTextCase(
            parent,
            "VISIBLE / VISIBLE",
            EOverflowMode::Visible,
            EOverflowMode::Visible,
            EClipMode::Auto,
            EWhiteSpace::NoWrap,
            kLongLine))
            return false;

        if (!AddOverflowTextCase(
            parent,
            "HIDDEN / HIDDEN",
            EOverflowMode::Hidden,
            EOverflowMode::Hidden,
            EClipMode::Auto,
            EWhiteSpace::NoWrap,
            kLongLine))
            return false;

        if (!AddOverflowTextCase(
            parent,
            "HIDDEN X / VISIBLE Y",
            EOverflowMode::Hidden,
            EOverflowMode::Visible,
            EClipMode::Auto,
            EWhiteSpace::Pre,
            "FIRST LINE - LONG CONTENT THAT EXTENDS OUTSIDE THE FRAME\n"
            "SECOND LINE - THIS SHOULD ALSO CONTINUE BEYOND THE FRAME YOHOOOOOOOOOOOOOOOOOOOOOOOOO\n"
            "THIRD LINE - MORE CONTENT TO TEST VERTICAL OVERFLOW\n"
            "FOURTH LINE - STILL MORE CONTENT\n"
            "FIFTH LINE - END OF TEST\n"
            "SIXTH LINE - END OF TEST"))
            return false;

        if (!AddOverflowTextCase(
            parent,
            "AUTO / HIDDEN",
            EOverflowMode::Auto,
            EOverflowMode::Hidden,
            EClipMode::Auto,
            EWhiteSpace::NoWrap,
            kLongLine))
            return false;

        if (!AddOverflowTextCase(
            parent,
            "SCROLL / HIDDEN",
            EOverflowMode::Scroll,
            EOverflowMode::Hidden,
            EClipMode::Auto,
            EWhiteSpace::NoWrap,
            kLongLine))
            return false;

        if (!AddOverflowTextCase(
            parent,
            "TRANSFORMED CHILD - FORCED CLIPPING",
            EOverflowMode::Hidden,
            EOverflowMode::Hidden,
            EClipMode::Always,
            EWhiteSpace::NoWrap,
            kLongLine,
            true))
            return false;

        if (!AddOverflowTextCase(
            parent,
            "TRANSFORMED CHILD - IGNORE ANCESTOR CLIP",
            EOverflowMode::Hidden,
            EOverflowMode::Hidden,
            EClipMode::Always,
            EWhiteSpace::NoWrap,
            kLongLine,
            true,
            true))
            return false;

        return true;
    }

    Silverware::ButtonWidget *AddButton(
        Silverware::ContainerWidget& parent,
    const char* variant,
    Silverware::FWidgetLayout layout,
    Silverware::FContainerLayout contentLayout)
    {
        using namespace Silverware;

        auto widget = Silverware::MakeUnique<ButtonWidget>();

        if (variant && *variant)
            widget->SetStyleVariant(variant);

        widget->SetLayout(layout);
        widget->SetContainerLayout(contentLayout);

        ButtonWidget* rawWidget = widget.get();

        if (!parent.AddChild(Silverware::TUniquePtr<CoreWidget>(widget.release())))
            return nullptr;

        return rawWidget;
    }

    Silverware::ImageWidget* AddImage(
    Silverware::ContainerWidget& parent,
    Silverware::FResourcePath source,
    Silverware::FWidgetLayout layout,
    Silverware::FImageStyle style)
    {
        using namespace Silverware;

        auto widget = Silverware::MakeUnique<ImageWidget>();

        if (!widget->SetSource(Silverware::TakeUniqueOwnership(source)))
            return nullptr;

        widget->SetLayout(layout);
        widget->SetImageStyle(Silverware::TakeUniqueOwnership(style));

        ImageWidget* rawWidget = widget.get();

        if (!parent.AddChild(Silverware::TUniquePtr<CoreWidget>(widget.release())))
            return nullptr;

        return rawWidget;
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

    // Use the second column for typography tests for now. The interactive-block
    // playground can return once its dedicated test cases are ready.
    ContainerWidget* typographyPanel = AddContainer(
        *body,
        "Panel",
        panelLayout,
        MakeContainerLayout(ELayoutDirection::Vertical, 10.0f));

    if (!buttonPanel || !typographyPanel)
        return false;

    if (!AddText(*buttonPanel, "BUTTONS", "PanelTitle") ||
    !AddText(*buttonPanel, "Button interaction, focus and visual states.", "Body") ||
    !AddText(*typographyPanel, "OVERFLOW & CLIPPING", "PanelTitle") ||
    !AddText(*typographyPanel, "Content boundaries and clipping behavior.", "Body"))
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

    ContainerWidget* typographyArea = AddContainer(
        *typographyPanel,
        "PlaygroundArea",
        areaLayout,
        MakeContainerLayout(ELayoutDirection::Vertical, 5.0f));

    std::vector<FTypographySample> typographySamples;

    if (!m_ButtonPlayground || !typographyArea ||
    !AddOverflowSamples(*typographyArea))
        return false;

    // Rich content button
    FWidgetLayout buttonLayout;
    buttonLayout.width = FLayoutLength::Units(240.0f);
    buttonLayout.height = FLayoutLength::Units(58.0f);
    SetPadding(buttonLayout, 8.0f, 8.0f, 8.0f, 8.0f);

    ButtonWidget* richButton = AddButton(
        *m_ButtonPlayground,
        "Primary",
        buttonLayout,
        MakeContainerLayout(
            ELayoutDirection::Horizontal,
            10.0f,
            EAlignment::Center,
            EJustification::Center));

    if (!richButton)
        return false;

    FWidgetTransformStyle transformStyle;

    transformStyle.operations = FTransformOperationList
    {
        FTranslateTransform{
            FLayoutLength::Units(20.0f),
            FLayoutLength::Units(0.0f)
        },
        FRotateTransform{15.0f},
        FScaleTransform{1.2f, 1.2f}
    };

    transformStyle.origin = FTransformOrigin{
        FLayoutLength::Percent(50.0f),
        FLayoutLength::Percent(50.0f)
    };

    richButton->SetTransformStyle(std::move(transformStyle));

    // Icon
    FResourcePath iconSource;

    if (!FResourcePath::TryParse("silverware:/Textures/save02.png", iconSource))
        return false;

    FWidgetLayout iconLayout;
    iconLayout.width = FLayoutLength::Units(24.0f);
    iconLayout.height = FLayoutLength::Units(24.0f);

    FImageStyle iconStyle;
    iconStyle.fit = EImageFit::Contain;

    if (!AddImage(*richButton, std::move(iconSource), iconLayout, iconStyle))
        return false;

    // Text content
    ContainerWidget* textContent = AddContainer(
        *richButton,
        nullptr,
        FWidgetLayout{},
        MakeContainerLayout(
            ELayoutDirection::Vertical,
            2.0f,
            EAlignment::Start,
            EJustification::Center));

    if (!textContent)
        return false;

    if (!AddText(*textContent, "Save Project", "PanelTitle") ||
        !AddText(*textContent, "Write changes to disk", "Body"))
        return false;

    // Click event
    m_PrimaryButtonClickConnection = richButton->OnClick().Connect(
        [](FClickEvent& event)
        {
            event.MarkHandled();
            std::cout << "[SilverwarePlayground]: Rich button clicked.\n";
        });

    FWidgetTransformStyle transformStyleFontButton1;

    transformStyleFontButton1.operations = FTransformOperationList
    {
        FTranslateTransform{
            FLayoutLength::Units(32.0f),
            FLayoutLength::Units(52.0f)
        },
        FRotateTransform{-9.0f},
    };

    transformStyleFontButton1.origin = FTransformOrigin{
        FLayoutLength::Percent(50.0f),
        FLayoutLength::Percent(50.0f)
    };

    FWidgetTransformStyle transformStyleFontButton2;

    transformStyleFontButton2.operations = FTransformOperationList
    {
        FTranslateTransform{
            FLayoutLength::Units(350.0f),
            FLayoutLength::Units(60.0f)
        },
        FRotateTransform{12.0f},
    };

    transformStyleFontButton2.origin = FTransformOrigin{
        FLayoutLength::Percent(50.0f),
        FLayoutLength::Percent(50.0f)
    };

    FWidgetTransformStyle transformStyleFontButton3;

    transformStyleFontButton3.operations = FTransformOperationList
    {
        FTranslateTransform{
            FLayoutLength::Units(78.0f),
            FLayoutLength::Units(110.0f)
        },
        FRotateTransform{15.0f},
    };

    transformStyleFontButton3.origin = FTransformOrigin{
        FLayoutLength::Percent(50.0f),
        FLayoutLength::Percent(50.0f)
    };

    const auto addFontButton = [
                this,
                &typographySamples,
                transformStyleFontButton1,
                transformStyleFontButton2,
                transformStyleFontButton3, buttonLayout
            ](
        std::size_t connectionIndex,
        const char *label,
        std::string fontFamily) -> bool {
        if (connectionIndex >= 3)
            return false;

        ButtonWidget *button = AddButton(
            *m_ButtonPlayground,
            "Primary",
            buttonLayout,
            MakeContainerLayout(
                ELayoutDirection::Horizontal,
                10.0f,
                EAlignment::Center,
                EJustification::Center));

        if (!button)
            return false;

        switch (connectionIndex)
        {
            case 0:
                button->SetTransformStyle(transformStyleFontButton1);
                break;

            case 1:
                button->SetTransformStyle(transformStyleFontButton2);
                break;

            case 2:
                button->SetTransformStyle(transformStyleFontButton3);
                break;
        }

        if (!AddText(*button, label, "ButtonLabel"))
            return false;

        const std::vector<FTypographySample> samples = typographySamples;

        m_FontButtonClickConnections[connectionIndex] =
                button->OnClick().Connect(
                    [samples, fontFamily = std::move(fontFamily)](FClickEvent &event) {
                        event.MarkHandled();

                        for (const FTypographySample &sample: samples)
                        {
                            if (!sample.widget)
                                continue;

                            FTextStyle style = sample.baseStyle;
                            style.fontFamily = fontFamily;
                            sample.widget->SetTextStyle(std::move(style));
                        }

                        std::cout << "[SilverwarePlayground]: Typography font changed to "
                                << fontFamily << ".\n";
                    });

        return true;
    };

    if (!addFontButton(0, "FunnelSans", "Funnel Sans") ||
        !addFontButton(1, "RobotoMono", "Roboto Mono") ||
        !addFontButton(2, "LatoLatin", "LatoLatin"))
        return false;

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

    for (auto& connection : m_FontButtonClickConnections)
        connection.Disconnect();

    m_ButtonPlayground = nullptr;
    m_InteractiveBlockPlayground = nullptr;
    m_RootWidget.reset();
    m_Backend = nullptr;
}
