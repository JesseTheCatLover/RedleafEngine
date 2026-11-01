// Copyright 2026 JesseTheCatLover. All Rights Reserved.

#include "EditorInput/Silverware/SilverwareInputTranslator.h"

#include <cmath>
#include <utility>

#include "InputSystem/EPhysicalInput.h"

namespace
{
    Silverware::FUIInputModifiers MakeModifiers(const FRawInputModifiers& modifiers)
    {
        Silverware::FUIInputModifiers result{};
        result.control = modifiers.control;
        result.shift = modifiers.shift;
        result.alt = modifiers.alt;
        result.super = modifiers.super;
        result.capsLock = modifiers.capsLock;
        result.numLock = modifiers.numLock;
        return result;
    }

    bool TryMapKey(EPhysicalInput input, Silverware::EUIKey& outKey)
    {
        const auto inputValue = static_cast<uint16_t>(input);

        // Contiguous key ranges.
        const auto mapRange = [&](EPhysicalInput first, EPhysicalInput last,
                                  Silverware::EUIKey outputFirst)
        {
            const auto firstValue = static_cast<uint16_t>(first);
            const auto lastValue = static_cast<uint16_t>(last);

            if (inputValue < firstValue || inputValue > lastValue)
                return false;

            const auto offset = inputValue - firstValue;
            outKey = static_cast<Silverware::EUIKey>(
                static_cast<uint16_t>(outputFirst) + offset);

            return true;
        };

        if (mapRange(EPhysicalInput::Key_0, EPhysicalInput::Key_9, Silverware::EUIKey::Digit0))
            return true;

        if (mapRange(EPhysicalInput::Key_A, EPhysicalInput::Key_Z, Silverware::EUIKey::A))
            return true;

        if (mapRange(EPhysicalInput::Key_F1, EPhysicalInput::Key_F24, Silverware::EUIKey::F1))
            return true;

        if (mapRange(EPhysicalInput::Key_KP_0, EPhysicalInput::Key_KP_9, Silverware::EUIKey::Keypad0))
            return true;

        switch (input)
        {
        case EPhysicalInput::Key_Space:          outKey = Silverware::EUIKey::Space; break;
        case EPhysicalInput::Key_Apostrophe:     outKey = Silverware::EUIKey::Apostrophe; break;
        case EPhysicalInput::Key_Comma:          outKey = Silverware::EUIKey::Comma; break;
        case EPhysicalInput::Key_Minus:          outKey = Silverware::EUIKey::Minus; break;
        case EPhysicalInput::Key_Period:         outKey = Silverware::EUIKey::Period; break;
        case EPhysicalInput::Key_Slash:          outKey = Silverware::EUIKey::Slash; break;
        case EPhysicalInput::Key_Semicolon:      outKey = Silverware::EUIKey::Semicolon; break;
        case EPhysicalInput::Key_Equal:          outKey = Silverware::EUIKey::Equal; break;

        case EPhysicalInput::Key_LeftBracket:    outKey = Silverware::EUIKey::LeftBracket; break;
        case EPhysicalInput::Key_Backslash:      outKey = Silverware::EUIKey::Backslash; break;
        case EPhysicalInput::Key_RightBracket:   outKey = Silverware::EUIKey::RightBracket; break;
        case EPhysicalInput::Key_GraveAccent:    outKey = Silverware::EUIKey::GraveAccent; break;

        case EPhysicalInput::Key_Escape:         outKey = Silverware::EUIKey::Escape; break;
        case EPhysicalInput::Key_Enter:          outKey = Silverware::EUIKey::Enter; break;
        case EPhysicalInput::Key_Tab:            outKey = Silverware::EUIKey::Tab; break;
        case EPhysicalInput::Key_Backspace:      outKey = Silverware::EUIKey::Backspace; break;
        case EPhysicalInput::Key_Insert:          outKey = Silverware::EUIKey::Insert; break;
        case EPhysicalInput::Key_Delete:          outKey = Silverware::EUIKey::Delete; break;
        case EPhysicalInput::Key_Home:            outKey = Silverware::EUIKey::Home; break;
        case EPhysicalInput::Key_End:             outKey = Silverware::EUIKey::End; break;
        case EPhysicalInput::Key_PageUp:          outKey = Silverware::EUIKey::PageUp; break;
        case EPhysicalInput::Key_PageDown:        outKey = Silverware::EUIKey::PageDown; break;

        case EPhysicalInput::Key_Up:              outKey = Silverware::EUIKey::Up; break;
        case EPhysicalInput::Key_Down:            outKey = Silverware::EUIKey::Down; break;
        case EPhysicalInput::Key_Left:            outKey = Silverware::EUIKey::Left; break;
        case EPhysicalInput::Key_Right:           outKey = Silverware::EUIKey::Right; break;

        case EPhysicalInput::Key_LeftShift:       outKey = Silverware::EUIKey::LeftShift; break;
        case EPhysicalInput::Key_RightShift:      outKey = Silverware::EUIKey::RightShift; break;
        case EPhysicalInput::Key_LeftControl:     outKey = Silverware::EUIKey::LeftControl; break;
        case EPhysicalInput::Key_RightControl:    outKey = Silverware::EUIKey::RightControl; break;
        case EPhysicalInput::Key_LeftAlt:         outKey = Silverware::EUIKey::LeftAlt; break;
        case EPhysicalInput::Key_RightAlt:        outKey = Silverware::EUIKey::RightAlt; break;
        case EPhysicalInput::Key_LeftSuper:       outKey = Silverware::EUIKey::LeftSuper; break;
        case EPhysicalInput::Key_RightSuper:      outKey = Silverware::EUIKey::RightSuper; break;

        case EPhysicalInput::Key_KP_Decimal:      outKey = Silverware::EUIKey::KeypadDecimal; break;
        case EPhysicalInput::Key_KP_Divide:       outKey = Silverware::EUIKey::KeypadDivide; break;
        case EPhysicalInput::Key_KP_Multiply:     outKey = Silverware::EUIKey::KeypadMultiply; break;
        case EPhysicalInput::Key_KP_Subtract:     outKey = Silverware::EUIKey::KeypadSubtract; break;
        case EPhysicalInput::Key_KP_Add:          outKey = Silverware::EUIKey::KeypadAdd; break;
        case EPhysicalInput::Key_KP_Enter:        outKey = Silverware::EUIKey::KeypadEnter; break;
        case EPhysicalInput::Key_KP_Equal:        outKey = Silverware::EUIKey::KeypadEqual; break;

        default:
            return false;
        }

        return true;
    }

    bool TryMapPointerButton(EPhysicalInput input, Silverware::EUIPointerButton& outButton)
    {
        switch (input)
        {
        case EPhysicalInput::Mouse_ButtonLeft:
            outButton = Silverware::EUIPointerButton::Primary;
            return true;

        case EPhysicalInput::Mouse_ButtonRight:
            outButton = Silverware::EUIPointerButton::Secondary;
            return true;

        case EPhysicalInput::Mouse_ButtonMiddle:
            outButton = Silverware::EUIPointerButton::Middle;
            return true;

        case EPhysicalInput::Mouse_Button4:
            outButton = Silverware::EUIPointerButton::Auxiliary1;
            return true;

        case EPhysicalInput::Mouse_Button5:
            outButton = Silverware::EUIPointerButton::Auxiliary2;
            return true;

        default:
            return false;
        }
    }

    bool TryMakePosition(
        const FRawInputEvent& rawEvent,
        float scale,
        Silverware::FUIInputVector2& outPosition)
    {
        if (!std::isfinite(scale) || scale <= 0.0f)
            return false;

        outPosition.x = rawEvent.positionX * scale;
        outPosition.y = rawEvent.positionY * scale;
        return true;
    }
}

bool SilverwareInputTranslator::TryTranslate(
    const FRawInputEvent& rawEvent,
    Silverware::FUIInputEvent& outEvent,
    float nativeToLogicalScale)
{
    const EPhysicalInput input = static_cast<EPhysicalInput>(rawEvent.code);

    switch (rawEvent.type)
    {
    case ERawInputType::KeyDown:
    {
        Silverware::EUIKey key{};
        if (!TryMapKey(input, key))
            return false;

        outEvent = Silverware::FKeyDownInputEvent{
            key,
            MakeModifiers(rawEvent.modifiers),
            rawEvent.isRepeat
        };
        return true;
    }

    case ERawInputType::KeyUp:
    {
        Silverware::EUIKey key{};
        if (!TryMapKey(input, key))
            return false;

        outEvent = Silverware::FKeyUpInputEvent{
            key,
            MakeModifiers(rawEvent.modifiers)
        };
        return true;
    }

    case ERawInputType::MouseMove:
    {
        Silverware::FPointerMoveInputEvent event{};

        if (!TryMakePosition(rawEvent, nativeToLogicalScale, event.position))
            return false;

        event.delta = {
            rawEvent.deltaX * nativeToLogicalScale,
            rawEvent.deltaY * nativeToLogicalScale
        };

        event.modifiers = MakeModifiers(rawEvent.modifiers);
        event.pointerID = 0;
        event.pointerType = Silverware::EUIPointerType::Mouse;

        outEvent = event;
        return true;
    }

    case ERawInputType::MouseButtonDown:
    case ERawInputType::MouseButtonUp:
    {
        Silverware::EUIPointerButton button{};
        if (!TryMapPointerButton(input, button))
            return false;

        Silverware::FUIInputVector2 position{};
        if (!TryMakePosition(rawEvent, nativeToLogicalScale, position))
            return false;

        if (rawEvent.type == ERawInputType::MouseButtonDown)
        {
            Silverware::FPointerButtonDownInputEvent event{};
            event.position = position;
            event.modifiers = MakeModifiers(rawEvent.modifiers);
            event.button = button;
            outEvent = event;
        }
        else
        {
            Silverware::FPointerButtonUpInputEvent event{};
            event.position = position;
            event.modifiers = MakeModifiers(rawEvent.modifiers);
            event.button = button;
            outEvent = event;
        }

        return true;
    }

    case ERawInputType::MouseWheel:
    {
        Silverware::FPointerScrollInputEvent event{};

        if (!TryMakePosition(rawEvent, nativeToLogicalScale, event.position))
            return false;

        event.offset = {
            rawEvent.scrollX,
            rawEvent.scrollY
        };

        event.modifiers = MakeModifiers(rawEvent.modifiers);
        event.unit = Silverware::EScrollDeltaUnit::Lines;
        event.pointerID = 0;

        outEvent = event;
        return true;
    }

    case ERawInputType::TextInput:
    {
        const uint32_t codepoint = rawEvent.code;

        // Reject invalid Unicode scalar values.
        if (codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF))
            return false;

        Silverware::FTextInputEvent event{};
        event.text.push_back(static_cast<char32_t>(codepoint));

        outEvent = std::move(event);
        return true;
    }

    default:
        // Gamepad events are not currently represented in FUIInputEvent.
        return false;
    }
}