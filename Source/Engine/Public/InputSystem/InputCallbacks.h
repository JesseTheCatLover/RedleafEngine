//  Copyright 2025 JesseTheCatLover. All Rights Reserved.

#pragma once
#include <cstdint>
#include <functional>

#include "InputChannels.h"
#include "InputSystem/MappingStyles/ActionAxis/ActionAxisStates.h"

enum class EInputEventPhase : uint8_t
{
    Pressed,
    Released,
    Held,
    AxisChanged, // (use delta)
};

using InputCallbackHandle = uint32_t;
static constexpr InputCallbackHandle INVALID_INPUT_CALLBACK = 0;

struct FRawInputEvent;

using FRawInputEventCallback = std::function<void(const FRawInputEvent&)>;

using FBoolActionCallback   = std::function<void(InputChannelHandle, const FActionStateBool&)>;
using FAxis1DActionCallback = std::function<void(InputChannelHandle, const FActionStateAxis1D&)>;
using FAxis2DActionCallback = std::function<void(InputChannelHandle, const FActionStateAxis2D&)>;

