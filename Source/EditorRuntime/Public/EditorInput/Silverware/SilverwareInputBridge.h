// Copyright 2026 JesseTheCatLover. All Rights Reserved.

#pragma once

#include "Framework/InputManager.h"

namespace Silverware
{
    class SilverwareUIContext;
}

class SilverwareInputBridge final
{
private:
    InputManager* m_InputManager = nullptr;
    Silverware::SilverwareUIContext* m_UIContext = nullptr;

    InputCallbackHandle m_RawInputCallback = INVALID_INPUT_CALLBACK;
    float m_NativeToLogicalScale = 1.0f;

public:
    SilverwareInputBridge() = default;
    ~SilverwareInputBridge();

    SilverwareInputBridge(const SilverwareInputBridge&) = delete;
    SilverwareInputBridge& operator=(const SilverwareInputBridge&) = delete;
    SilverwareInputBridge(SilverwareInputBridge&&) = delete;
    SilverwareInputBridge& operator=(SilverwareInputBridge&&) = delete;

    [[nodiscard]] bool Initialize(
        InputManager& inputManager,
        Silverware::SilverwareUIContext& uiContext,
        float nativeToLogicalScale = 1.0f);

    void Shutdown();

    [[nodiscard]] bool IsInitialized() const noexcept;

private:
    void OnRawInputEvent(const FRawInputEvent& event);
};