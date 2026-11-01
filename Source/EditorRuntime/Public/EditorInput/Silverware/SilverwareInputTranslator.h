// Copyright 2026 JesseTheCatLover. All Rights Reserved.

#pragma once

#include "InputSystem/FRawInputEvent.h"
#include "Silverware/Core/Input/UIInputEvent.h"

class SilverwareInputTranslator final
{
public:
    [[nodiscard]] static bool TryTranslate(
        const FRawInputEvent& rawEvent,
        Silverware::FUIInputEvent& outEvent,
        float nativeToLogicalScale = 1.0f);
};