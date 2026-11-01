// Copyright 2026 JesseTheCatLover. All Rights Reserved.

#include "EditorInput/Silverware/SilverwareInputBridge.h"

#include <cmath>

#include "EditorInput/Silverware/SilverwareInputTranslator.h"
#include "Silverware/Backend/SilverwareUIContext.h"

SilverwareInputBridge::~SilverwareInputBridge()
{
    Shutdown();
}

bool SilverwareInputBridge::Initialize(
    InputManager& inputManager,
    Silverware::SilverwareUIContext& uiContext,
    float nativeToLogicalScale)
{
    Shutdown();

    if (!std::isfinite(nativeToLogicalScale) || nativeToLogicalScale <= 0.0f ||
        !uiContext.IsInitialized())
        return false;

    m_InputManager = &inputManager;
    m_UIContext = &uiContext;
    m_NativeToLogicalScale = nativeToLogicalScale;

    m_RawInputCallback = m_InputManager->BindRawInputEvent(
        [this](const FRawInputEvent& event)
        {
            OnRawInputEvent(event);
        });

    if (m_RawInputCallback == INVALID_INPUT_CALLBACK)
    {
        m_InputManager = nullptr;
        m_UIContext = nullptr;
        return false;
    }

    return true;
}

void SilverwareInputBridge::Shutdown()
{
    if (m_InputManager && m_RawInputCallback != INVALID_INPUT_CALLBACK)
        m_InputManager->Unbind(m_RawInputCallback);

    m_RawInputCallback = INVALID_INPUT_CALLBACK;
    m_InputManager = nullptr;
    m_UIContext = nullptr;
    m_NativeToLogicalScale = 1.0f;
}

bool SilverwareInputBridge::IsInitialized() const noexcept
{
    return m_InputManager &&
           m_UIContext &&
           m_RawInputCallback != INVALID_INPUT_CALLBACK;
}

void SilverwareInputBridge::OnRawInputEvent(const FRawInputEvent& event)
{
    if (!m_UIContext)
        return;

    Silverware::FUIInputEvent translatedEvent{};

    if (!SilverwareInputTranslator::TryTranslate(
            event, translatedEvent, m_NativeToLogicalScale))
        return;

    const Silverware::EUIInputResult result =
        m_UIContext->ProcessInputEvent(translatedEvent);

    // Input consumption will be integrated with Redleaf's host input
    // arbitration in a later step.
    (void)result;
}