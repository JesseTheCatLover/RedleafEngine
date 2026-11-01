//  Copyright 2025 JesseTheCatLover. All Rights Reserved.

#pragma once
#include <cstdint>

enum class ERawInputType : uint8_t
{
    KeyDown, KeyUp,
    MouseMove,
    MouseButtonDown, MouseButtonUp,
    MouseWheel,
    GamepadButtonDown, GamepadButtonUp,
    GamepadAxis,
    TextInput,
};

struct FRawInputModifiers
{
    bool shift = false;
    bool control = false;
    bool alt = false;
    bool super = false;
    bool capsLock = false;
    bool numLock = false;
};

struct FRawInputEvent
{
    ERawInputType type{};

    int deviceID = 0; ///< Which keyboard/mouse/gamepad
    uint32_t code = 0; ///< underlying EPhysicalInput (casted)
    float value = 0.0f; ///< axis value, wheel delta, etc.
    double timestamp = 0.0; ///< in seconds

    // Mouse position, in window coordinates.
    float positionX = 0.0f;
    float positionY = 0.0f;

    // Mouse movement delta.
    float deltaX = 0.0f;
    float deltaY = 0.0f;

    // Mouse wheel offset.
    float scrollX = 0.0f;
    float scrollY = 0.0f;

    FRawInputModifiers modifiers{};
    bool isRepeat = false;
};
