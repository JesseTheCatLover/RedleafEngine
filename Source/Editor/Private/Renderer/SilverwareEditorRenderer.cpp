// Copyright 2026 JesseTheCatLover. All Rights Reserved.

#include "SilverwareEditorRenderer.h"

#include "Backends/SilverwareBackend.h"

#include "Silverware/Core/Styling/StyleModel.h"
#include "Silverware/Core/Widgets/ContainerWidget.h"
#include "Silverware/Core/Widgets/CoreWidget.h"
#include "Silverware/Core/Widgets/Styling/WidgetLayout.h"
#include "Silverware/Core/Widgets/Styling/TextStyle.h"
#include "Silverware/Core/Widgets/Styling/WidgetEffectStyle.h"
#include "Silverware/Core/Widgets/Styling/WidgetVisualStyle.h"
#include "Silverware/Core/Widgets/Styling/BackgroundStyle.h"
#include "Silverware/UI/Widgets/TextWidget.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "Silverware/UI/Widgets/ButtonWidget.h"
#include "Silverware/UI/Widgets/CheckboxWidget.h"
#include "Silverware/UI/Widgets/ImageWidget.h"
#include "Silverware/UI/Widgets/ProgressBarWidget.h"
#include "Silverware/UI/Widgets/RadioButtonWidget.h"
#include "Silverware/UI/Widgets/ToggleWidget.h"
#include "Silverware/UI/Widgets/SliderWidget.h"
#include "Silverware/UI/Widgets/RangedSliderWidget.h"

namespace
{
    using namespace Silverware;

    constexpr char kBaseThemeID[] = "Redleaf.InkBlue.Base";
    constexpr char kDarkThemeID[] = "Redleaf.InkBlue.Dark";
    constexpr char kLightThemeID[] = "Redleaf.InkBlue.Light";

    struct FTypographySample
    {
        Silverware::TextWidget* widget = nullptr;
        Silverware::FTextStyle baseStyle;
    };

    FStyleValue Token(const char* tokenID)
    {
        return FStyleTokenReference{std::string(tokenID)};
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

    bool AddWidgetEffectSample(
        Silverware::ContainerWidget& parent,
        const char* label,
        const char* description,
        Silverware::FWidgetEffectStyle effectStyle,
        Silverware::FColor backgroundColor)
    {
        using namespace Silverware;

        if (!AddText(parent, label, "Kicker"))
            return false;

        FWidgetLayout cardLayout;
        cardLayout.width = FLayoutLength::Percent(100.0f);
        cardLayout.height = FLayoutLength::Units(54.0f);
        SetPadding(cardLayout, 7.0f, 9.0f, 7.0f, 9.0f);

        ContainerWidget* card = AddContainer(
            parent,
            nullptr,
            cardLayout,
            MakeContainerLayout(
                ELayoutDirection::Vertical,
                0.0f,
                EAlignment::Start,
                EJustification::Center));

        if (!card)
            return false;

        FWidgetVisualStyle visualStyle;
        visualStyle.backgroundColor = backgroundColor;
        visualStyle.borderColor = FColor{0.27f, 0.34f, 0.45f, 0.9f};
        visualStyle.borderWidth = 1.0f;
        visualStyle.cornerRadius = 7.0f;

        card->SetVisualStyle(std::move(visualStyle));
        card->SetEffectStyle(std::move(effectStyle));

        return AddText(*card, description, "Body") != nullptr;
    }

    bool AddFontEffectSample(
        Silverware::ContainerWidget& parent,
        const char* label,
        const char* sampleText,
        Silverware::FFontEffectList effects,
        std::vector<FTypographySample>& outSamples)
    {
        using namespace Silverware;

        if (!AddText(parent, label, "Kicker"))
            return false;

        FTextStyle style;
        style.fontSize = 16.0f;
        style.fontWeight = EFontWeight::Bold;
        style.color = FColor{0.91f, 0.95f, 1.0f, 1.0f};
        style.fontEffects = std::move(effects);

        FTextStyle baseStyle = style;
        TextWidget* widget = AddStyledText(parent, sampleText, std::move(style));

        if (!widget)
            return false;

        outSamples.push_back({widget, std::move(baseStyle)});
        return true;
    }

    bool AddFontEffectSamples(
        Silverware::ContainerWidget& parent,
        std::vector<FTypographySample>& outSamples)
    {
        using namespace Silverware;

        if (!AddText(parent, "FONT EFFECTS", "PanelTitle") ||
            !AddText(parent, "Glyph-specific blur, glow, outline and shadow.", "Body"))
            return false;

        if (!AddFontEffectSample(
            parent,
            "FONT BLUR",
            "Soft blurred glyphs",
            FFontEffectList{
                FFontBlurEffect{
                    1.6f,
                    FColor{0.28f, 0.72f, 1.0f, 0.95f}
                }
            },
            outSamples))
            return false;


        if (!AddFontEffectSample(
            parent,
            "FONT GLOW",
            "Cyan glow around the text",
            FFontEffectList{
                FFontGlowEffect{
                    1.2f,
                    4.0f,
                    0.0f,
                    0.0f,
                    FColor{0.12f, 0.62f, 1.0f, 1.0f}
                }
            },
            outSamples))
            return false;

        if (!AddFontEffectSample(
            parent,
            "FONT OUTLINE",
            "Outlined typography",
            FFontEffectList{
                FFontOutlineEffect{
                    1.4f,
                    FColor{0.68f, 0.40f, 1.0f, 1.0f}
                }
            },
            outSamples))
            return false;

        if (!AddFontEffectSample(
            parent,
            "FONT SHADOW",
            "Offset hard glyph shadow",
            FFontEffectList{
                FFontHardShadowEffect{
                    2.0f,
                    3.0f,
                    FColor{0.0f, 0.0f, 0.0f, 0.90f}
                }
            },
            outSamples))
            return false;

        return AddFontEffectSample(
            parent,
            "COMBINED FONT EFFECTS",
            "Shadow + outline + glow",
            FFontEffectList{
                FFontHardShadowEffect{
                    1.5f,
                    2.0f,
                    FColor{0.0f, 0.0f, 0.0f, 0.85f}
                },
                FFontOutlineEffect{
                    0.2f,
                    FColor{0.18f, 0.58f, 0.95f, 1.0f}
                },
                FFontGlowEffect{
                    0.5f,
                    2.0f,
                    0.0f,
                    0.0f,
                    FColor{0.12f, 0.48f, 0.95f, 0.72f}
                }
            },
            outSamples);
        return true;
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

    bool AddTextOverflowSamples(
    Silverware::ContainerWidget& parent,
    std::vector<FTypographySample>& outSamples)
{
    using namespace Silverware;

    constexpr char kSampleText[] =
        "Silverware is a retained UI framework designed to make complex interfaces easier to build and maintain.";

    const auto addSample = [&parent, &outSamples, &kSampleText](
        const char* label,
        FTextOverflow textOverflow) -> bool
    {
        if (!AddText(parent, label, "Kicker"))
            return false;

        FWidgetLayout frameLayout;
        frameLayout.width = FLayoutLength::Units(600.0f);
        frameLayout.height = FLayoutLength::Units(38.0f);
        SetPadding(frameLayout, 4.0f, 6.0f, 4.0f, 6.0f);

        ContainerWidget* frame = AddContainer(
            parent,
            nullptr,
            frameLayout,
            MakeContainerLayout(
                ELayoutDirection::Vertical,
                0.0f,
                EAlignment::Stretch,
                EJustification::Start));

        if (!frame)
            return false;

        FWidgetVisualStyle frameStyle;
        frameStyle.backgroundColor = FColor{0.055f, 0.070f, 0.095f, 1.0f};
        frameStyle.borderColor = FColor{0.25f, 0.32f, 0.42f, 1.0f};
        frameStyle.borderWidth = 1.0f;
        frame->SetVisualStyle(std::move(frameStyle));

        FTextStyle textStyle;
        textStyle.fontSize = 13.0f;
        textStyle.whiteSpace = EWhiteSpace::NoWrap;
        textStyle.textOverflow = std::move(textOverflow);

        FTextStyle baseStyle = textStyle;

        TextWidget* textWidget = AddStyledText(
            *frame,
            kSampleText,
            std::move(textStyle));

        if (!textWidget)
            return false;

        FWidgetLayout textLayout = textWidget->GetLayout();
        textLayout.width = FLayoutLength::Percent(100.0f);
        textLayout.height = FLayoutLength::Units(28.0f);
        textWidget->SetLayout(textLayout);

        FWidgetOverflowStyle overflowStyle;
        overflowStyle.overflowX = EOverflowMode::Hidden;
        overflowStyle.overflowY = EOverflowMode::Hidden;
        textWidget->SetOverflowStyle(std::move(overflowStyle));

        outSamples.push_back({textWidget, std::move(baseStyle)});

        return true;
    };

    if (!addSample("TEXT OVERFLOW: CLIP", FTextOverflow::Clip()) ||
        !addSample("TEXT OVERFLOW: ELLIPSIS", FTextOverflow::Ellipsis()) ||
        !addSample("TEXT OVERFLOW: CUSTOM MARKER", FTextOverflow::Custom(" [more]")))
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

    CheckboxWidget* AddCheckbox(
    ContainerWidget& parent,
    const char* label,
    const char* variant,
    FWidgetLayout rowLayout,
    FContainerLayout contentLayout)
    {
        using namespace Silverware;

        auto row = Silverware::MakeUnique<ContainerWidget>();
        row->SetLayout(rowLayout);
        row->SetContainerLayout(MakeContainerLayout(
            ELayoutDirection::Horizontal,
            6.0f,
            EAlignment::Center,
            EJustification::Start));

        auto content = Silverware::MakeUnique<ContainerWidget>();

        FWidgetLayout contentWidgetLayout;
        contentWidgetLayout.width = FLayoutLength::Units(0.0f);
        contentWidgetLayout.height = FLayoutLength::Auto();
        contentWidgetLayout.flexGrow = 1.0f;

        content->SetStyleVariant("CheckboxLabelContent");
        content->SetLayout(contentWidgetLayout);
        content->SetContainerLayout(contentLayout);

        auto checkbox = Silverware::MakeUnique<CheckboxWidget>();

        if (variant && *variant)
            checkbox->SetStyleVariant(variant);

        FWidgetLayout checkboxLayout;
        checkboxLayout.width = FLayoutLength::Units(20.0f);
        checkboxLayout.height = FLayoutLength::Units(20.0f);

        checkbox->SetLayout(checkboxLayout);

        CheckboxWidget* rawCheckbox = checkbox.get();
        ContainerWidget* rawContent = content.get();

        // Add the content before the checkbox to preserve left-to-right order.
        if (!row->AddChild(Silverware::TUniquePtr<CoreWidget>(content.release())))
            return nullptr;

        if (!row->AddChild(Silverware::TUniquePtr<CoreWidget>(checkbox.release())))
            return nullptr;

        if (!rawCheckbox->SetContentWidget(rawContent))
            return nullptr;

        TextWidget* labelWidget = AddText(*rawContent, label, "CheckboxLabel");

        if (!labelWidget)
            return nullptr;

        FWidgetTransformStyle labelTransform;
        labelTransform.operations = FTransformOperationList{
            FTranslateTransform{
                FLayoutLength::Units(0.0f),
                FLayoutLength::Units(4.0f)
            }
        };

        labelWidget->SetTransformStyle(std::move(labelTransform));

        if (!parent.AddChild(Silverware::TUniquePtr<CoreWidget>(row.release())))
            return nullptr;

        return rawCheckbox;
    }

    RadioButtonWidget* AddRadioButton(
        ContainerWidget& parent,
        const char* label,
        const char* groupName,
        FWidgetLayout rowLayout,
        FContainerLayout contentLayout)
    {
        using namespace Silverware;

        auto row = Silverware::MakeUnique<ContainerWidget>();
        row->SetLayout(rowLayout);
        row->SetContainerLayout(MakeContainerLayout(
            ELayoutDirection::Horizontal,
            6.0f,
            EAlignment::Center,
            EJustification::Start));

        auto radioButton = Silverware::MakeUnique<RadioButtonWidget>();

        FWidgetLayout radioButtonLayout;
        radioButtonLayout.width = FLayoutLength::Units(20.0f);
        radioButtonLayout.height = FLayoutLength::Units(20.0f);
        radioButton->SetLayout(radioButtonLayout);
        radioButton->SetGroupName(groupName ? groupName : "");

        auto content = Silverware::MakeUnique<ContainerWidget>();

        FWidgetLayout contentWidgetLayout;
        contentWidgetLayout.width = FLayoutLength::Units(0.0f);
        contentWidgetLayout.height = FLayoutLength::Auto();
        contentWidgetLayout.flexGrow = 1.0f;

        content->SetStyleVariant("RadioButtonLabelContent");
        content->SetLayout(contentWidgetLayout);
        content->SetContainerLayout(contentLayout);

        RadioButtonWidget* rawRadioButton = radioButton.get();
        ContainerWidget* rawContent = content.get();

        // Keep the indicator before its label in the horizontal row.
        if (!row->AddChild(Silverware::TUniquePtr<CoreWidget>(radioButton.release())))
            return nullptr;

        if (!row->AddChild(Silverware::TUniquePtr<CoreWidget>(content.release())))
            return nullptr;

        if (!rawRadioButton->SetContentWidget(rawContent))
            return nullptr;

        if (!AddText(*rawContent, label, "RadioButtonLabel"))
            return nullptr;

        if (!parent.AddChild(Silverware::TUniquePtr<CoreWidget>(row.release())))
            return nullptr;

        return rawRadioButton;
    }

    ToggleWidget* AddToggle(
        ContainerWidget& parent,
        const char* label,
        const char* variant,
        FWidgetLayout rowLayout)
    {
        using namespace Silverware;

        auto row = Silverware::MakeUnique<ContainerWidget>();
        row->SetLayout(rowLayout);
        row->SetContainerLayout(MakeContainerLayout(
            ELayoutDirection::Horizontal,
            8.0f,
            EAlignment::Center,
            EJustification::Start));

        auto content = Silverware::MakeUnique<ContainerWidget>();

        FWidgetLayout contentWidgetLayout;
        contentWidgetLayout.width = FLayoutLength::Units(0.0f);
        contentWidgetLayout.height = FLayoutLength::Auto();
        contentWidgetLayout.flexGrow = 1.0f;

        content->SetStyleVariant("ToggleLabelContent");
        content->SetLayout(contentWidgetLayout);
        content->SetContainerLayout(MakeContainerLayout(
            ELayoutDirection::Horizontal,
            0.0f,
            EAlignment::Center,
            EJustification::Start));

        auto toggle = Silverware::MakeUnique<ToggleWidget>();

        if (variant && *variant)
            toggle->SetStyleVariant(variant);

        FWidgetLayout toggleLayout;
        toggleLayout.width = FLayoutLength::Units(44.0f);
        toggleLayout.height = FLayoutLength::Units(24.0f);
        toggle->SetLayout(toggleLayout);

        ToggleWidget* rawToggle = toggle.get();
        ContainerWidget* rawContent = content.get();

        // Keep the label content before the switch in the horizontal row.
        if (!row->AddChild(Silverware::TUniquePtr<CoreWidget>(content.release())))
            return nullptr;

        if (!row->AddChild(Silverware::TUniquePtr<CoreWidget>(toggle.release())))
            return nullptr;

        if (!rawToggle->SetContentWidget(rawContent))
            return nullptr;

        if (!AddText(*rawContent, label, "ToggleLabel"))
            return nullptr;

        if (!parent.AddChild(Silverware::TUniquePtr<CoreWidget>(row.release())))
            return nullptr;

        return rawToggle;
    }

    std::string FormatSliderValue(double value, int precision, const char* suffix)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(precision) << value;

        if (suffix && *suffix)
            stream << suffix;

        return stream.str();
    }

    std::string FormatRangedSliderValues(
        const FRangedSliderValues& values,
        int precision,
        const char* suffix)
    {
        return FormatSliderValue(values.low, precision, suffix) + "  –  " +
               FormatSliderValue(values.high, precision, suffix);
    }

    RangedSliderWidget* AddRangedSlider(
        ContainerWidget& parent,
        const char* variant,
        FWidgetLayout layout,
        double minimum,
        double maximum,
        double step,
        double lowValue,
        double highValue,
        ESliderOrientation orientation = ESliderOrientation::Horizontal)
    {
        using namespace Silverware;

        auto widget = Silverware::MakeUnique<RangedSliderWidget>();

        if (variant && *variant)
            widget->SetStyleVariant(variant);

        widget->SetLayout(layout);
        widget->SetOrientation(orientation);

        if (!widget->SetRange(minimum, maximum) || !widget->SetStep(step))
            return nullptr;

        widget->SetValues(lowValue, highValue);

        RangedSliderWidget* rawWidget = widget.get();

        if (!parent.AddChild(Silverware::TUniquePtr<CoreWidget>(widget.release())))
            return nullptr;

        return rawWidget;
    }

    SliderWidget* AddSlider(
        ContainerWidget& parent,
        const char* variant,
        FWidgetLayout layout,
        double minimum,
        double maximum,
        double step,
        double value,
        ESliderOrientation orientation = ESliderOrientation::Horizontal)
    {
        using namespace Silverware;

        auto widget = Silverware::MakeUnique<SliderWidget>();

        if (variant && *variant)
            widget->SetStyleVariant(variant);

        widget->SetLayout(layout);
        widget->SetOrientation(orientation);

        if (!widget->SetRange(minimum, maximum) || !widget->SetStep(step))
            return nullptr;

        widget->SetValue(value);

        SliderWidget* rawWidget = widget.get();

        if (!parent.AddChild(Silverware::TUniquePtr<CoreWidget>(widget.release())))
            return nullptr;

        return rawWidget;
    }

    ProgressBarWidget* AddProgressBar(
    ContainerWidget& parent,
    const char* variant,
    FWidgetLayout layout,
    double minimum,
    double maximum,
    double value,
    EProgressBarOrientation orientation = EProgressBarOrientation::Horizontal)
    {
        using namespace Silverware;

        auto widget = Silverware::MakeUnique<ProgressBarWidget>();

        if (variant && *variant)
            widget->SetStyleVariant(variant);

        widget->SetLayout(layout);
        widget->SetOrientation(orientation);

        if (!widget->SetRange(minimum, maximum))
            return nullptr;

        widget->SetValue(value);

        ProgressBarWidget* rawWidget = widget.get();

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

    if (!BuildPlaygroundLayout())
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
            EAlignment::Center, EJustification::Start));

    if (!toolbar ||
        !AddText(*toolbar, "SILVERWARE", "Accent") ||
        !AddText(*toolbar, "INTERACTION PLAYGROUND", "Header"))
        return false;

    FWidgetLayout statusLayout;
    statusLayout.height = FLayoutLength::Units(30.0f);
    SetPadding(statusLayout, 0.0f, 10.0f, 0.0f, 10.0f);

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

    // Use the second column for widget and font effect samples for now.
    ContainerWidget* typographyPanel = AddContainer(
        *body,
        "Panel",
        panelLayout,
        MakeContainerLayout(ELayoutDirection::Vertical, 10.0f));

    if (!buttonPanel || !typographyPanel)
        return false;

    if (!AddText(*buttonPanel, "BUTTONS", "PanelTitle") ||
        !AddText(*buttonPanel, "Button interaction, focus and visual states.", "Body") ||
        !AddText(*typographyPanel, "WIDGET + FONT EFFECTS", "PanelTitle") ||
        !AddText(*typographyPanel, "Rendering and typography effect samples.", "Body"))
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
        !AddFontEffectSamples(*typographyArea, typographySamples))
        return false;

    // The interaction column now contains several control examples.
    FWidgetOverflowStyle playgroundOverflow;
    playgroundOverflow.overflowY = EOverflowMode::Auto;
    m_ButtonPlayground->SetOverflowStyle(std::move(playgroundOverflow));

    // Rich content button
    FWidgetLayout buttonLayout;
    buttonLayout.width = FLayoutLength::Units(240.0f);
    buttonLayout.height = FLayoutLength::Units(58.0f);
    SetPadding(buttonLayout, 8.0f, 8.0f, 8.0f, 8.0f);
    //
    // ButtonWidget* richButton = AddButton(
    //     *m_ButtonPlayground,
    //     "Primary",
    //     buttonLayout,
    //     MakeContainerLayout(
    //         ELayoutDirection::Horizontal,
    //         10.0f,
    //         EAlignment::Center,
    //         EJustification::Center));
    //
    // if (!richButton)
    //     return false;
    //
    // // Icon
    // FResourcePath iconSource;
    //
    // if (!FResourcePath::TryParse("silverware:/Textures/save02.png", iconSource))
    //     return false;
    //
    // FWidgetLayout iconLayout;
    // iconLayout.width = FLayoutLength::Units(24.0f);
    // iconLayout.height = FLayoutLength::Units(24.0f);
    //
    // FImageStyle iconStyle;
    // iconStyle.fit = EImageFit::Contain;
    //
    // if (!AddImage(*richButton, std::move(iconSource), iconLayout, iconStyle))
    //     return false;
    //
    // // Text content
    // ContainerWidget* textContent = AddContainer(
    //     *richButton,
    //     nullptr,
    //     FWidgetLayout{},
    //     MakeContainerLayout(
    //         ELayoutDirection::Vertical,
    //         2.0f,
    //         EAlignment::Start,
    //         EJustification::Center));
    //
    // if (!textContent)
    //     return false;
    //
    // if (!AddText(*textContent, "Save Project", "PanelTitle") ||
    //     !AddText(*textContent, "Write changes to disk", "Body"))
    //     return false;
    //
    // // Click event
    // m_PrimaryButtonClickConnection = richButton->OnClick().Connect(
    //     [](FClickEvent& event)
    //     {
    //         event.MarkHandled();
    //         std::cout << "[SilverwarePlayground]: Rich button clicked.\n";
    //     });

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

    FWidgetVisualStyle fontButtonVisualStyle;

    FLinearGradient fontButtonGradient;
    fontButtonGradient.angleDegrees = 180.0f;
    fontButtonGradient.stops = {
        { FColor{0.30f, 0.40f, 0.54f, 1.0f}, 0.0f },
        { FColor{0.20f, 0.30f, 0.44f, 1.0f}, 0.5f },
        { FColor{0.13f, 0.21f, 0.33f, 1.0f}, 1.0f }
    };

    fontButtonVisualStyle.backgroundLayers = FBackgroundLayerList{
        FGradient{fontButtonGradient}
    };

    fontButtonVisualStyle.borderColor = FColor{0.48f, 0.59f, 0.72f, 0.65f};
    fontButtonVisualStyle.borderWidth = 2.0f;
    fontButtonVisualStyle.borderStyle = EBorderStyle::Dashed;
    fontButtonVisualStyle.cornerRadius = 7.0f;
    fontButtonVisualStyle.bottomRightRadius = 30.f;
    fontButtonVisualStyle.topLeftRadius = 20.f;

    FWidgetVisualStyle inkBlueButtonStyle;

    inkBlueButtonStyle.backgroundColor = FColor{
        0.341f, 0.420f, 0.502f, 1.0f // #576B80
    };

    FLinearGradient inkBlueGradient;
    inkBlueGradient.angleDegrees = 180.0f;
    inkBlueGradient.stops = {
        { FColor{0.369f, 0.478f, 0.580f, 1.0f}, 0.0f }, // #5E7A94
        { FColor{0.341f, 0.420f, 0.502f, 1.0f}, 0.45f }, // #576B80
        { FColor{0.271f, 0.345f, 0.420f, 1.0f}, 1.0f } // #45586B
    };

    inkBlueButtonStyle.backgroundLayers = FBackgroundLayerList{
        FGradient{inkBlueGradient}
    };

    inkBlueButtonStyle.borderColor = FColor{
        0.341f, 0.420f, 0.502f, 1.0f
    };

    inkBlueButtonStyle.borderWidth = 1.0f;
    inkBlueButtonStyle.borderStyle = EBorderStyle::Solid;
    inkBlueButtonStyle.cornerRadius = 3.0f;

    const auto addFontButton = [
                this,
                &typographySamples,
                transformStyleFontButton1,
                transformStyleFontButton2,
                transformStyleFontButton3, buttonLayout, fontButtonVisualStyle, inkBlueButtonStyle
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
                button->SetVisualStyle(inkBlueButtonStyle);
                button->SetEnterKeyActivationEnabled(true);
                break;

            case 1:
                button->SetTransformStyle(transformStyleFontButton2);
                button->SetVisualStyle(fontButtonVisualStyle);
                button->SetEnabled(false);
                break;

            case 2:
                button->SetTransformStyle(transformStyleFontButton3);
                button->SetVisualStyle(inkBlueButtonStyle);
                button->SetSpaceKeyActivationEnabled(true);
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

    // if (!addFontButton(0, "FunnelSans", "Funnel Sans") ||
    //     !addFontButton(1, "RobotoMono", "Roboto Mono")
    //     // || !addFontButton(2, "LatoLatin", "LatoLatin")
    //     )
    //     return false;

    // // Checkbox list
    // FWidgetLayout checkboxListLayout;
    // checkboxListLayout.width = FLayoutLength::Units(180.0f);
    // checkboxListLayout.height = FLayoutLength::Auto();
    //
    // ContainerWidget *checkboxList = AddContainer(
    //     *m_ButtonPlayground,
    //     nullptr,
    //     checkboxListLayout,
    //     MakeContainerLayout(
    //         ELayoutDirection::Vertical,
    //         8.0f,
    //         EAlignment::Start,
    //         EJustification::Start));
    //
    // if (!checkboxList)
    //     return false;
    //
    // if (!AddText(*checkboxList, "CHECKBOXES", "PanelTitle") ||
    //     !AddText(*checkboxList, "Pointer, keyboard, checked and disabled states.", "Body"))
    //     return false;
    //
    // // Checkbox rows
    // FWidgetLayout checkboxRowLayout;
    // checkboxRowLayout.width = FLayoutLength::Percent(100.0f);
    // checkboxRowLayout.height = FLayoutLength::Units(26.0f);
    //
    // const FContainerLayout checkboxContentLayout = MakeContainerLayout(
    //     ELayoutDirection::Horizontal,
    //     0.0f,
    //     EAlignment::Center,
    //     EJustification::Start);
    //
    // CheckboxWidget *vsyncCheckbox = AddCheckbox(
    //     *checkboxList,
    //     "Enable VSync",
    //     nullptr,
    //     checkboxRowLayout,
    //     checkboxContentLayout);
    //
    // if (!vsyncCheckbox)
    //     return false;
    //
    // m_CheckboxChangedConnections[0] = vsyncCheckbox->OnCheckedChanged().Connect(
    //     [](TValueChangedEvent<bool> &event) {
    //         std::cout << "[SilverwarePlayground]: VSync checkbox "
    //                 << (event.newValue ? "checked" : "unchecked") << ".\n";
    //     });
    //
    // CheckboxWidget *hdrCheckbox = AddCheckbox(
    //     *checkboxList,
    //     "Enable HDR",
    //     nullptr,
    //     checkboxRowLayout,
    //     checkboxContentLayout);
    //
    // if (!hdrCheckbox)
    //     return false;
    //
    // hdrCheckbox->SetChecked(true);
    //
    // m_CheckboxChangedConnections[1] = hdrCheckbox->OnCheckedChanged().Connect(
    //     [](TValueChangedEvent<bool> &event) {
    //         std::cout << "[SilverwarePlayground]: HDR checkbox "
    //                 << (event.newValue ? "checked" : "unchecked") << ".\n";
    //     });
    //
    // CheckboxWidget *disabledCheckbox = AddCheckbox(
    //     *checkboxList,
    //     "Read-only setting",
    //     nullptr,
    //     checkboxRowLayout,
    //     checkboxContentLayout);
    //
    // if (!disabledCheckbox)
    //     return false;
    //
    // disabledCheckbox->SetChecked(true);
    // disabledCheckbox->SetEnabled(false);

    // // Radio button group
    // FWidgetLayout radioButtonListLayout;
    // radioButtonListLayout.width = FLayoutLength::Units(220.0f);
    // radioButtonListLayout.height = FLayoutLength::Auto();
    //
    // ContainerWidget* radioButtonList = AddContainer(
    //     *m_ButtonPlayground,
    //     nullptr,
    //     radioButtonListLayout,
    //     MakeContainerLayout(
    //         ELayoutDirection::Vertical,
    //         8.0f,
    //         EAlignment::Start,
    //         EJustification::Start));
    //
    // if (!radioButtonList ||
    //     !AddText(*radioButtonList, "RADIO BUTTONS", "PanelTitle") ||
    //     !AddText(*radioButtonList, "Mutually exclusive rendering backend selection.", "Body"))
    // {
    //     return false;
    // }
    //
    // FWidgetLayout radioButtonRowLayout;
    // radioButtonRowLayout.width = FLayoutLength::Percent(100.0f);
    // radioButtonRowLayout.height = FLayoutLength::Units(26.0f);
    //
    // const FContainerLayout radioButtonContentLayout = MakeContainerLayout(
    //     ELayoutDirection::Horizontal,
    //     0.0f,
    //     EAlignment::Center,
    //     EJustification::Start);
    //
    // constexpr char kRenderingBackendGroup[] = "RenderingBackend";
    //
    // RadioButtonWidget* openGLRadioButton = AddRadioButton(
    //     *radioButtonList,
    //     "OpenGL",
    //     kRenderingBackendGroup,
    //     radioButtonRowLayout,
    //     radioButtonContentLayout);
    //
    // RadioButtonWidget* vulkanRadioButton = AddRadioButton(
    //     *radioButtonList,
    //     "Vulkan",
    //     kRenderingBackendGroup,
    //     radioButtonRowLayout,
    //     radioButtonContentLayout);
    //
    // RadioButtonWidget* metalRadioButton = AddRadioButton(
    //     *radioButtonList,
    //     "Metal",
    //     kRenderingBackendGroup,
    //     radioButtonRowLayout,
    //     radioButtonContentLayout);
    //
    // RadioButtonWidget* directXRadioButton = AddRadioButton(
    //     *radioButtonList,
    //     "DirectX 12 (unavailable)",
    //     kRenderingBackendGroup,
    //     radioButtonRowLayout,
    //     radioButtonContentLayout);
    //
    // if (!openGLRadioButton || !vulkanRadioButton ||
    //     !metalRadioButton || !directXRadioButton)
    // {
    //     return false;
    // }
    //
    // // The initial selection is established before the tree is attached to the context.
    // vulkanRadioButton->SetSelected(true);
    // directXRadioButton->SetEnabled(false);
    //
    // m_RadioButtonChangedConnections[0] = openGLRadioButton->OnSelectedChanged().Connect(
    //     [](TValueChangedEvent<bool>& event)
    //     {
    //         std::cout << "[SilverwarePlayground]: OpenGL radio button "
    //                   << (event.newValue ? "selected" : "deselected") << ".\n";
    //     });
    //
    // m_RadioButtonChangedConnections[1] = vulkanRadioButton->OnSelectedChanged().Connect(
    //     [](TValueChangedEvent<bool>& event)
    //     {
    //         std::cout << "[SilverwarePlayground]: Vulkan radio button "
    //                   << (event.newValue ? "selected" : "deselected") << ".\n";
    //     });
    //
    // m_RadioButtonChangedConnections[2] = metalRadioButton->OnSelectedChanged().Connect(
    //     [](TValueChangedEvent<bool>& event)
    //     {
    //         std::cout << "[SilverwarePlayground]: Metal radio button "
    //                   << (event.newValue ? "selected" : "deselected") << ".\n";
    //     });
    //
    // m_RadioButtonChangedConnections[3] = directXRadioButton->OnSelectedChanged().Connect(
    //     [](TValueChangedEvent<bool>& event)
    //     {
    //         std::cout << "[SilverwarePlayground]: DirectX 12 radio button "
    //                   << (event.newValue ? "selected" : "deselected") << ".\n";
    //     });

    // // Toggle examples
    // FWidgetLayout toggleListLayout;
    // toggleListLayout.width = FLayoutLength::Units(260.0f);
    // toggleListLayout.height = FLayoutLength::Auto();
    //
    // ContainerWidget* toggleList = AddContainer(
    //     *m_ButtonPlayground,
    //     nullptr,
    //     toggleListLayout,
    //     MakeContainerLayout(
    //         ELayoutDirection::Vertical,
    //         8.0f,
    //         EAlignment::Start,
    //         EJustification::Start));
    //
    // if (!toggleList ||
    //     !AddText(*toggleList, "TOGGLES", "PanelTitle") ||
    //     !AddText(*toggleList, "Independent on/off controls with animated thumbs.", "Body"))
    // {
    //     return false;
    // }
    //
    // FWidgetLayout toggleRowLayout;
    // toggleRowLayout.width = FLayoutLength::Percent(100.0f);
    // toggleRowLayout.height = FLayoutLength::Units(28.0f);
    //
    // ToggleWidget* temporalUpscalingToggle = AddToggle(
    //     *toggleList,
    //     "Temporal Upscaling",
    //     "Default",
    //     toggleRowLayout);
    //
    // ToggleWidget* dynamicResolutionToggle = AddToggle(
    //     *toggleList,
    //     "Dynamic Resolution",
    //     "Default",
    //     toggleRowLayout);
    //
    // ToggleWidget* rayTracingToggle = AddToggle(
    //     *toggleList,
    //     "Ray Tracing",
    //     "Default",
    //     toggleRowLayout);
    //
    // ToggleWidget* disabledToggle = AddToggle(
    //     *toggleList,
    //     "Read-only feature",
    //     "Default",
    //     toggleRowLayout);
    //
    // if (!temporalUpscalingToggle || !dynamicResolutionToggle ||
    //     !rayTracingToggle || !disabledToggle)
    // {
    //     return false;
    // }
    //
    // // Establish initial values before the root tree is attached to the UI context.
    // temporalUpscalingToggle->SetOn(true);
    // rayTracingToggle->SetOn(true);
    // disabledToggle->SetOn(true);
    // disabledToggle->SetEnabled(false);
    //
    // m_ToggleChangedConnections[0] = temporalUpscalingToggle->OnToggledChanged().Connect(
    //     [](TValueChangedEvent<bool>& event)
    //     {
    //         std::cout << "[SilverwarePlayground]: Temporal Upscaling "
    //                   << (event.newValue ? "enabled" : "disabled") << ".\n";
    //     });
    //
    // m_ToggleChangedConnections[1] = dynamicResolutionToggle->OnToggledChanged().Connect(
    //     [](TValueChangedEvent<bool>& event)
    //     {
    //         std::cout << "[SilverwarePlayground]: Dynamic Resolution "
    //                   << (event.newValue ? "enabled" : "disabled") << ".\n";
    //     });
    //
    // m_ToggleChangedConnections[2] = rayTracingToggle->OnToggledChanged().Connect(
    //     [](TValueChangedEvent<bool>& event)
    //     {
    //         std::cout << "[SilverwarePlayground]: Ray Tracing "
    //                   << (event.newValue ? "enabled" : "disabled") << ".\n";
    //     });
    //
    // m_ToggleChangedConnections[3] = disabledToggle->OnToggledChanged().Connect(
    //     [](TValueChangedEvent<bool>& event)
    //     {
    //         std::cout << "[SilverwarePlayground]: Read-only feature "
    //                     << (event.newValue ? "enabled" : "disabled") << ".\n";
    //     });

    // Slider examples
    FWidgetLayout sliderListLayout;
    sliderListLayout.width = FLayoutLength::Units(340.0f);
    sliderListLayout.height = FLayoutLength::Auto();

    ContainerWidget* sliderList = AddContainer(
        *m_ButtonPlayground,
        nullptr,
        sliderListLayout,
        MakeContainerLayout(
            ELayoutDirection::Vertical,
            10.0f,
            EAlignment::Start,
            EJustification::Start));
    //
    // if (!sliderList ||
    //     !AddText(*sliderList, "SLIDERS", "PanelTitle") ||
    //     !AddText(*sliderList, "Pointer drag, keyboard adjustment, snapping and orientation.", "Body"))
    // {
    //     return false;
    // }
    //
    // const auto addHorizontalSliderSample = [this, sliderList](
    //     const char* label,
    //     double minimum,
    //     double maximum,
    //     double step,
    //     double initialValue,
    //     int precision,
    //     const char* suffix,
    //     std::size_t connectionIndex) -> bool
    // {
    //     FWidgetLayout sampleLayout;
    //     sampleLayout.width = FLayoutLength::Percent(100.0f);
    //     sampleLayout.height = FLayoutLength::Auto();
    //
    //     ContainerWidget* sample = AddContainer(
    //         *sliderList,
    //         nullptr,
    //         sampleLayout,
    //         MakeContainerLayout(ELayoutDirection::Vertical, 5.0f));
    //
    //     if (!sample)
    //         return false;
    //
    //     FWidgetLayout headerLayout;
    //     headerLayout.width = FLayoutLength::Percent(100.0f);
    //     headerLayout.height = FLayoutLength::Auto();
    //
    //     ContainerWidget* header = AddContainer(
    //         *sample,
    //         nullptr,
    //         headerLayout,
    //         MakeContainerLayout(
    //             ELayoutDirection::Horizontal,
    //             8.0f,
    //             EAlignment::Center,
    //             EJustification::Start));
    //
    //     if (!header || !AddText(*header, label, "Body"))
    //         return false;
    //
    //     TextWidget* valueLabel = AddText(
    //         *header,
    //         FormatSliderValue(initialValue, precision, suffix),
    //         "Mono");
    //
    //     if (!valueLabel)
    //         return false;
    //
    //     FWidgetLayout sliderLayout;
    //     sliderLayout.width = FLayoutLength::Units(280.0f);
    //     sliderLayout.height = FLayoutLength::Units(24.0f);
    //
    //     SliderWidget* slider = AddSlider(
    //         *sample,
    //         "Default",
    //         sliderLayout,
    //         minimum,
    //         maximum,
    //         step,
    //         initialValue);
    //
    //     if (!slider)
    //         return false;
    //
    //     const std::string sampleName(label);
    //     const std::string valueSuffix(suffix ? suffix : "");
    //
    //     m_SliderValueChangedConnections[connectionIndex] = slider->OnValueChanged().Connect(
    //         [valueLabel, precision, valueSuffix](TValueChangedEvent<double>& event)
    //         {
    //             valueLabel->SetText(FormatSliderValue(
    //                 event.newValue,
    //                 precision,
    //                 valueSuffix.c_str()));
    //         });
    //
    //     m_SliderValueCommittedConnections[connectionIndex] = slider->OnValueChangeCommitted().Connect(
    //         [sampleName, precision, valueSuffix](TValueChangeCommittedEvent<double>& event)
    //         {
    //             std::cout << "[SilverwarePlayground]: " << sampleName
    //                       << " committed "
    //                       << FormatSliderValue(event.oldValue, precision, valueSuffix.c_str())
    //                       << " -> "
    //                       << FormatSliderValue(event.newValue, precision, valueSuffix.c_str())
    //                       << ".\n";
    //         });
    //
    //     return true;
    // };
    //
    // if (!addHorizontalSliderSample(
    //         "Master Volume", 0.0, 100.0, 1.0, 68.0, 0, "%", 0) ||
    //     !addHorizontalSliderSample(
    //         "Render Scale", 50.0, 150.0, 5.0, 100.0, 0, "%", 1))
    // {
    //     return false;
    // }
    //
    // // Vertical slider: the value should increase as the thumb moves upward.
    // FWidgetLayout verticalSampleLayout;
    // verticalSampleLayout.width = FLayoutLength::Percent(100.0f);
    // verticalSampleLayout.height = FLayoutLength::Auto();
    //
    // ContainerWidget* verticalSample = AddContainer(
    //     *sliderList,
    //     nullptr,
    //     verticalSampleLayout,
    //     MakeContainerLayout(
    //         ELayoutDirection::Horizontal,
    //         12.0f,
    //         EAlignment::Center,
    //         EJustification::Start));
    //
    // if (!verticalSample)
    //     return false;
    //
    // FWidgetLayout exposureInfoLayout;
    // exposureInfoLayout.width = FLayoutLength::Units(0.0f);
    // exposureInfoLayout.height = FLayoutLength::Auto();
    // exposureInfoLayout.flexGrow = 1.0f;
    //
    // ContainerWidget* exposureInfo = AddContainer(
    //     *verticalSample,
    //     nullptr,
    //     exposureInfoLayout,
    //     MakeContainerLayout(ELayoutDirection::Vertical, 5.0f));
    //
    // if (!exposureInfo ||
    //     !AddText(*exposureInfo, "VERTICAL SLIDER", "Kicker"))
    // {
    //     return false;
    // }
    //
    // TextWidget* exposureValueLabel = AddText(*exposureInfo, "0.0 EV", "Mono");
    //
    // if (!exposureValueLabel ||
    //     !AddText(*exposureInfo, "Move upward to increase exposure.", "Body"))
    // {
    //     return false;
    // }
    //
    // FWidgetLayout verticalSliderLayout;
    // verticalSliderLayout.width = FLayoutLength::Units(24.0f);
    // verticalSliderLayout.height = FLayoutLength::Units(160.0f);
    //
    // SliderWidget* exposureSlider = AddSlider(
    //     *verticalSample,
    //     "Vertical",
    //     verticalSliderLayout,
    //     -2.0,
    //     2.0,
    //     0.1,
    //     0.0,
    //     ESliderOrientation::Vertical);
    //
    // if (!exposureSlider)
    //     return false;
    //
    // m_SliderValueChangedConnections[2] = exposureSlider->OnValueChanged().Connect(
    //     [exposureValueLabel](TValueChangedEvent<double>& event)
    //     {
    //         exposureValueLabel->SetText(FormatSliderValue(event.newValue, 1, " EV"));
    //     });
    //
    // m_SliderValueCommittedConnections[2] = exposureSlider->OnValueChangeCommitted().Connect(
    //     [](TValueChangeCommittedEvent<double>& event)
    //     {
    //         std::cout << "[SilverwarePlayground]: Exposure committed "
    //                   << FormatSliderValue(event.oldValue, 1, " EV")
    //                   << " -> "
    //                   << FormatSliderValue(event.newValue, 1, " EV")
    //                   << ".\n";
    //     });
    //
    // // Disabled-state sample. It should remain visible but ignore pointer and keyboard input.
    // FWidgetLayout disabledSliderSampleLayout;
    // disabledSliderSampleLayout.width = FLayoutLength::Percent(100.0f);
    // disabledSliderSampleLayout.height = FLayoutLength::Auto();
    //
    // ContainerWidget* disabledSliderSample = AddContainer(
    //     *sliderList,
    //     nullptr,
    //     disabledSliderSampleLayout,
    //     MakeContainerLayout(ELayoutDirection::Vertical, 5.0f));
    //
    // if (!disabledSliderSample)
    //     return false;
    //
    // FWidgetLayout disabledHeaderLayout;
    // disabledHeaderLayout.width = FLayoutLength::Percent(100.0f);
    // disabledHeaderLayout.height = FLayoutLength::Auto();
    //
    // ContainerWidget* disabledHeader = AddContainer(
    //     *disabledSliderSample,
    //     nullptr,
    //     disabledHeaderLayout,
    //     MakeContainerLayout(
    //         ELayoutDirection::Horizontal,
    //         8.0f,
    //         EAlignment::Center,
    //         EJustification::Start));
    //
    // if (!disabledHeader || !AddText(*disabledHeader, "Read-only preview", "Body") ||
    //     !AddText(*disabledHeader, "75%", "Mono"))
    // {
    //     return false;
    // }
    //
    // FWidgetLayout disabledSliderLayout;
    // disabledSliderLayout.width = FLayoutLength::Units(280.0f);
    // disabledSliderLayout.height = FLayoutLength::Units(24.0f);
    //
    // SliderWidget* disabledSlider = AddSlider(
    //     *disabledSliderSample,
    //     "Default",
    //     disabledSliderLayout,
    //     0.0,
    //     100.0,
    //     5.0,
    //     75.0);
    //
    // if (!disabledSlider)
    //     return false;
    //
    // disabledSlider->SetEnabled(false);

    // // Ranged slider examples: endpoint thumbs move independently, while the
    // // selected handle translates the complete interval without changing width.
    // if (!AddText(*sliderList, "RANGED SLIDERS", "PanelTitle") ||
    //     !AddText(*sliderList,
    //         "Drag either thumb to edit one endpoint. Drag the filled handle to move the whole range.",
    //         "Body"))
    // {
    //     return false;
    // }
    //
    // const auto addHorizontalRangedSliderSample = [this, sliderList](
    //     const char* label,
    //     double minimum,
    //     double maximum,
    //     double step,
    //     double initialLow,
    //     double initialHigh,
    //     int precision,
    //     const char* suffix,
    //     std::size_t connectionIndex) -> bool
    // {
    //     FWidgetLayout sampleLayout;
    //     sampleLayout.width = FLayoutLength::Percent(100.0f);
    //     sampleLayout.height = FLayoutLength::Auto();
    //
    //     ContainerWidget* sample = AddContainer(
    //         *sliderList,
    //         nullptr,
    //         sampleLayout,
    //         MakeContainerLayout(ELayoutDirection::Vertical, 5.0f));
    //
    //     if (!sample)
    //         return false;
    //
    //     FWidgetLayout headerLayout;
    //     headerLayout.width = FLayoutLength::Percent(100.0f);
    //     headerLayout.height = FLayoutLength::Auto();
    //
    //     ContainerWidget* header = AddContainer(
    //         *sample,
    //         nullptr,
    //         headerLayout,
    //         MakeContainerLayout(
    //             ELayoutDirection::Horizontal,
    //             8.0f,
    //             EAlignment::Center,
    //             EJustification::SpaceBetween));
    //
    //     if (!header || !AddText(*header, label, "Body"))
    //         return false;
    //
    //     TextWidget* valueLabel = AddText(
    //         *header,
    //         FormatRangedSliderValues({initialLow, initialHigh}, precision, suffix),
    //         "Mono");
    //
    //     if (!valueLabel)
    //         return false;
    //
    //     FWidgetLayout sliderLayout;
    //     sliderLayout.width = FLayoutLength::Units(280.0f);
    //     sliderLayout.height = FLayoutLength::Units(24.0f);
    //
    //     RangedSliderWidget* slider = AddRangedSlider(
    //         *sample,
    //         "Default",
    //         sliderLayout,
    //         minimum,
    //         maximum,
    //         step,
    //         initialLow,
    //         initialHigh);
    //
    //     if (!slider)
    //         return false;
    //
    //     const std::string sampleName(label);
    //     const std::string valueSuffix(suffix ? suffix : "");
    //
    //     m_RangedSliderValuesChangedConnections[connectionIndex] =
    //         slider->OnRangeChanged().Connect(
    //             [valueLabel, precision, valueSuffix](TValueChangedEvent<FRangedSliderValues>& event)
    //             {
    //                 valueLabel->SetText(FormatRangedSliderValues(
    //                     event.newValue,
    //                     precision,
    //                     valueSuffix.c_str()));
    //             });
    //
    //     m_RangedSliderValuesCommittedConnections[connectionIndex] =
    //         slider->OnRangeChangeCommitted().Connect(
    //             [sampleName, precision, valueSuffix](
    //                 TValueChangeCommittedEvent<FRangedSliderValues>& event)
    //             {
    //                 std::cout << "[SilverwarePlayground]: " << sampleName
    //                           << " committed ["
    //                           << FormatRangedSliderValues(event.oldValue, precision, valueSuffix.c_str())
    //                           << "] -> ["
    //                           << FormatRangedSliderValues(event.newValue, precision, valueSuffix.c_str())
    //                           << "].\n";
    //             });
    //
    //     return true;
    // };
    //
    // if (!addHorizontalRangedSliderSample(
    //         "Playback Loop", 0.0, 100.0, 1.0, 20.0, 80.0, 0, "%", 0) ||
    //     !addHorizontalRangedSliderSample(
    //         "Temperature Window", -20.0, 50.0, 0.5, 4.0, 28.0, 1, " °C", 1))
    // {
    //     return false;
    // }
    //
    // // Vertical ranged slider: moving the selected handle translates both
    // // endpoints while preserving their distance, and upward increases values.
    // FWidgetLayout verticalRangeSampleLayout;
    // verticalRangeSampleLayout.width = FLayoutLength::Percent(100.0f);
    // verticalRangeSampleLayout.height = FLayoutLength::Auto();
    //
    // ContainerWidget* verticalRangeSample = AddContainer(
    //     *sliderList,
    //     nullptr,
    //     verticalRangeSampleLayout,
    //     MakeContainerLayout(
    //         ELayoutDirection::Horizontal,
    //         12.0f,
    //         EAlignment::Center,
    //         EJustification::Start));
    //
    // if (!verticalRangeSample)
    //     return false;
    //
    // FWidgetLayout verticalRangeInfoLayout;
    // verticalRangeInfoLayout.width = FLayoutLength::Units(0.0f);
    // verticalRangeInfoLayout.height = FLayoutLength::Auto();
    // verticalRangeInfoLayout.flexGrow = 1.0f;
    //
    // ContainerWidget* verticalRangeInfo = AddContainer(
    //     *verticalRangeSample,
    //     nullptr,
    //     verticalRangeInfoLayout,
    //     MakeContainerLayout(ELayoutDirection::Vertical, 5.0f));
    //
    // if (!verticalRangeInfo ||
    //     !AddText(*verticalRangeInfo, "VERTICAL RANGE", "Kicker"))
    // {
    //     return false;
    // }
    //
    // TextWidget* verticalRangeValueLabel = AddText(
    //     *verticalRangeInfo,
    //     FormatRangedSliderValues({-4.0, 6.0}, 1, " dB"),
    //     "Mono");
    //
    // if (!verticalRangeValueLabel ||
    //     !AddText(*verticalRangeInfo,
    //         "Drag either endpoint or move the selected range upward.",
    //         "Body"))
    // {
    //     return false;
    // }
    //
    // FWidgetLayout verticalRangeSliderLayout;
    // verticalRangeSliderLayout.width = FLayoutLength::Units(24.0f);
    // verticalRangeSliderLayout.height = FLayoutLength::Units(160.0f);
    //
    // RangedSliderWidget* verticalRangeSlider = AddRangedSlider(
    //     *verticalRangeSample,
    //     "Vertical",
    //     verticalRangeSliderLayout,
    //     -10.0,
    //     10.0,
    //     0.5,
    //     -4.0,
    //     6.0,
    //     ESliderOrientation::Vertical);
    //
    // if (!verticalRangeSlider)
    //     return false;
    //
    // m_RangedSliderValuesChangedConnections[2] =
    //     verticalRangeSlider->OnRangeChanged().Connect(
    //         [verticalRangeValueLabel](TValueChangedEvent<FRangedSliderValues>& event)
    //         {
    //             verticalRangeValueLabel->SetText(FormatRangedSliderValues(event.newValue, 1, " dB"));
    //         });
    //
    // m_RangedSliderValuesCommittedConnections[2] =
    //     verticalRangeSlider->OnRangeChangeCommitted().Connect(
    //         [](TValueChangeCommittedEvent<FRangedSliderValues>& event)
    //         {
    //             std::cout << "[SilverwarePlayground]: Vertical range committed ["
    //                       << FormatRangedSliderValues(event.oldValue, 1, " dB")
    //                       << "] -> ["
    //                       << FormatRangedSliderValues(event.newValue, 1, " dB")
    //                       << "].\n";
    //         });

        // Progress bar examples.
    FWidgetLayout progressBarPlaygroundLayout;
    progressBarPlaygroundLayout.width = FLayoutLength::Percent(100.0f);
    progressBarPlaygroundLayout.height = FLayoutLength::Auto();

    m_ProgressBarPlayground = AddContainer(
        *sliderList,
        nullptr,
        progressBarPlaygroundLayout,
        MakeContainerLayout(
            ELayoutDirection::Vertical,
            10.0f,
            EAlignment::Start,
            EJustification::Start));

    if (!m_ProgressBarPlayground ||
        !AddText(*m_ProgressBarPlayground, "PROGRESS BARS", "PanelTitle") ||
        !AddText(*m_ProgressBarPlayground,
            "Determinate progress, value ranges and orientation.",
            "Body"))
    {
        return false;
    }

    // Static progress states.
    const auto addProgressStateSample = [this](
        const char* label,
        const char* valueText,
        double minimum,
        double maximum,
        double value,
        bool bEnabled) -> bool
    {
        FWidgetLayout sampleLayout;
        sampleLayout.width = FLayoutLength::Percent(100.0f);
        sampleLayout.height = FLayoutLength::Auto();

        ContainerWidget* sample = AddContainer(
            *m_ProgressBarPlayground,
            nullptr,
            sampleLayout,
            MakeContainerLayout(ELayoutDirection::Vertical, 5.0f));

        if (!sample)
            return false;

        ContainerWidget* header = AddContainer(
            *sample,
            nullptr,
            FWidgetLayout{},
            MakeContainerLayout(
                ELayoutDirection::Horizontal,
                8.0f,
                EAlignment::Center,
                EJustification::SpaceBetween));

        if (!header ||
            !AddText(*header, label, "Body") ||
            !AddText(*header, valueText, "Mono"))
        {
            return false;
        }

        FWidgetLayout progressBarLayout;
        progressBarLayout.width = FLayoutLength::Units(180.0f);
        progressBarLayout.height = FLayoutLength::Units(12.0f);

        ProgressBarWidget* progressBar = AddProgressBar(
            *sample,
            "Default",
            progressBarLayout,
            minimum,
            maximum,
            value);

        if (!progressBar)
            return false;

        progressBar->SetEnabled(bEnabled);

        return true;
    };

    if (!addProgressStateSample("Empty", "0%", 0.0, 100.0, 0.0, true) ||
        !addProgressStateSample("Almost complete", "75%", 0.0, 100.0, 75.0, true) ||
        !addProgressStateSample("Custom range", "325 / 500", 0.0, 500.0, 325.0, true) ||
        !addProgressStateSample("Disabled preview", "75%", 0.0, 100.0, 75.0, false))
    {
        return false;
    }

    // Live progress controlled by a slider.
    FWidgetLayout liveProgressSampleLayout;
    liveProgressSampleLayout.width = FLayoutLength::Percent(100.0f);
    liveProgressSampleLayout.height = FLayoutLength::Auto();

    ContainerWidget* liveProgressSample = AddContainer(
        *m_ProgressBarPlayground,
        nullptr,
        liveProgressSampleLayout,
        MakeContainerLayout(ELayoutDirection::Vertical, 6.0f));

    if (!liveProgressSample ||
        !AddText(*liveProgressSample, "LIVE PROGRESS", "Kicker"))
    {
        return false;
    }

    ContainerWidget* liveProgressHeader = AddContainer(
        *liveProgressSample,
        nullptr,
        FWidgetLayout{},
        MakeContainerLayout(
            ELayoutDirection::Horizontal,
            8.0f,
            EAlignment::Center,
            EJustification::SpaceBetween));

    TextWidget* liveProgressValueLabel = liveProgressHeader
        ? AddText(*liveProgressHeader, "42%", "Mono")
        : nullptr;

    if (!liveProgressHeader ||
        !AddText(*liveProgressHeader, "Importing assets", "Body") ||
        !liveProgressValueLabel)
    {
        return false;
    }

    FWidgetLayout liveProgressBarLayout;
    liveProgressBarLayout.width = FLayoutLength::Units(180.0f);
    liveProgressBarLayout.height = FLayoutLength::Units(12.0f);

    ProgressBarWidget* liveProgressBar = AddProgressBar(
        *liveProgressSample,
        "Default",
        liveProgressBarLayout,
        0.0,
        100.0,
        42.0);

    if (!liveProgressBar)
        return false;

    FWidgetLayout liveProgressSliderLayout;
    liveProgressSliderLayout.width = FLayoutLength::Units(280.0f);
    liveProgressSliderLayout.height = FLayoutLength::Units(24.0f);

    SliderWidget* liveProgressSlider = AddSlider(
        *liveProgressSample,
        "Default",
        liveProgressSliderLayout,
        0.0,
        100.0,
        1.0,
        42.0);

    if (!liveProgressSlider)
        return false;

    m_ProgressBarSliderValueChangedConnection =
        liveProgressSlider->OnValueChanged().Connect(
            [liveProgressBar, liveProgressValueLabel](TValueChangedEvent<double>& event)
            {
                liveProgressBar->SetValue(event.newValue);

                liveProgressValueLabel->SetText(
                    FormatSliderValue(event.newValue, 0, "%"));
            });

    m_ProgressBarValueChangedConnection =
        liveProgressBar->OnValueChanged().Connect(
            [](TValueChangedEvent<double>& event)
            {
                std::cout << "[SilverwarePlayground]: Progress changed "
                          << FormatSliderValue(event.oldValue, 0, "%")
                          << " -> "
                          << FormatSliderValue(event.newValue, 0, "%")
                          << ".\n";
            });

    // Vertical progress bar.
    FWidgetLayout verticalProgressSampleLayout;
    verticalProgressSampleLayout.width = FLayoutLength::Percent(100.0f);
    verticalProgressSampleLayout.height = FLayoutLength::Auto();

    ContainerWidget* verticalProgressSample = AddContainer(
        *m_ProgressBarPlayground,
        nullptr,
        verticalProgressSampleLayout,
        MakeContainerLayout(
            ELayoutDirection::Horizontal,
            12.0f,
            EAlignment::Center,
            EJustification::Start));

    if (!verticalProgressSample)
        return false;

    FWidgetLayout verticalProgressInfoLayout;
    verticalProgressInfoLayout.width = FLayoutLength::Units(0.0f);
    verticalProgressInfoLayout.height = FLayoutLength::Auto();
    verticalProgressInfoLayout.flexGrow = 1.0f;

    ContainerWidget* verticalProgressInfo = AddContainer(
        *verticalProgressSample,
        nullptr,
        verticalProgressInfoLayout,
        MakeContainerLayout(ELayoutDirection::Vertical, 5.0f));

    if (!verticalProgressInfo ||
        !AddText(*verticalProgressInfo, "VERTICAL PROGRESS", "Kicker") ||
        !AddText(*verticalProgressInfo, "68%", "Mono") ||
        !AddText(*verticalProgressInfo,
            "Vertical progress fill and track orientation.",
            "Body"))
    {
        return false;
    }

    FWidgetLayout verticalProgressBarLayout;
    verticalProgressBarLayout.width = FLayoutLength::Units(12.0f);
    verticalProgressBarLayout.height = FLayoutLength::Units(160.0f);

    ProgressBarWidget* verticalProgressBar = AddProgressBar(
        *verticalProgressSample,
        "Vertical",
        verticalProgressBarLayout,
        0.0,
        100.0,
        68.0,
        EProgressBarOrientation::Vertical);

    if (!verticalProgressBar)
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
    }

    m_PrimaryButtonClickConnection.Disconnect();

    for (auto& connection : m_FontButtonClickConnections)
        connection.Disconnect();

    for (auto& connection : m_CheckboxChangedConnections)
        connection.Disconnect();

    for (auto& connection : m_RadioButtonChangedConnections)
        connection.Disconnect();

    for (auto& connection : m_ToggleChangedConnections)
        connection.Disconnect();

    for (auto& connection : m_SliderValueChangedConnections)
        connection.Disconnect();

    for (auto& connection : m_SliderValueCommittedConnections)
        connection.Disconnect();

    for (auto& connection : m_RangedSliderValuesChangedConnections)
        connection.Disconnect();

    for (auto& connection : m_RangedSliderValuesCommittedConnections)
        connection.Disconnect();

    m_ButtonPlayground = nullptr;
    m_InteractiveBlockPlayground = nullptr;
    m_RootWidget.reset();
    m_Backend = nullptr;
}


