//  Copyright 2025 JesseTheCatLover. All Rights Reserved.

#include "Framework/InputManager.h"

#include <iostream>

#include "InputSystem/InputSubsystem.h"
#include "InputSystem/MappingStyles/HotkeyChord/IHotkeyBindingEditable.h"

bool InputManager::Initialize(InputSubsystem *system)
{
    if (!system)
        return false;

    m_InputSystem = system;
    m_Cache.clear();
    m_LastChannelVersion = m_InputSystem->GetChannelVersion();

    return true;
}

InputChannelHandle InputManager::GetChannelHandle(const std::string &name)
{
    if (!m_InputSystem)
    {
        std::cerr << "[InputManager]: Cannot return channel handle, input system pointer is invalid." << std::endl;
        return INVALID_CHANNEL_HANDLE;
    }

    // 1) If channels changed, clear cache
    uint32_t currentVersion = m_InputSystem->GetChannelVersion();
    if (currentVersion != m_LastChannelVersion)
    {
        m_Cache.clear();
        m_LastChannelVersion = currentVersion;
    }

    // 2) Normal cached lookup
    auto it = m_Cache.find(name);
    if (it != m_Cache.end())
        return it->second;

    InputChannelHandle handle = m_InputSystem->FindChannelIdByName(name);
    m_Cache[name] = handle;
    return handle;
}

InputCallbackHandle InputManager::BindRawInputEvent(FRawInputEventCallback callback)
{
    if (!m_InputSystem)
        return INVALID_INPUT_CALLBACK;

    return m_InputSystem->RegisterRawInputEventCallback(std::move(callback));
}

InputCallbackHandle InputManager::BindAction(
    const std::string& name,
    EInputEventPhase phase,
    FBoolActionCallback cb)
{
    if (!m_InputSystem)
        return INVALID_INPUT_CALLBACK;

    return m_InputSystem->RegisterBoolCallback(name, phase, std::move(cb));
}

InputCallbackHandle InputManager::BindAxis1D(
    const std::string& name,
    EInputEventPhase phase,
    FAxis1DActionCallback cb)
{
    if (!m_InputSystem)
        return INVALID_INPUT_CALLBACK;

    return m_InputSystem->RegisterAxis1DCallback(name, phase, std::move(cb));
}

InputCallbackHandle InputManager::BindAxis2D(
    const std::string& name,
    EInputEventPhase phase,
    FAxis2DActionCallback cb)
{
    if (!m_InputSystem)
        return INVALID_INPUT_CALLBACK;

    return m_InputSystem->RegisterAxis2DCallback(name, phase, std::move(cb));
}

void InputManager::Unbind(InputCallbackHandle handle)
{
    if (!m_InputSystem || handle == INVALID_INPUT_CALLBACK)
        return;

    m_InputSystem->UnregisterCallback(handle);
}

bool InputManager::GetActionDown(const std::string& name)
{
    InputChannelHandle handle = GetChannelHandle(name);
    if (handle == INVALID_CHANNEL_HANDLE)
        return false;

    FActionStateBool st = m_InputSystem->GetBoolChannel(handle);
    return st.pressed;
}

bool InputManager::GetActionUp(const std::string& name)
{
    InputChannelHandle handle = GetChannelHandle(name);
    if (handle == INVALID_CHANNEL_HANDLE)
        return false;

    FActionStateBool st = m_InputSystem->GetBoolChannel(handle);
    return st.released;
}

bool InputManager::GetActionHeld(const std::string& name)
{
    InputChannelHandle handle = GetChannelHandle(name);
    if (handle == INVALID_CHANNEL_HANDLE)
        return false;

    FActionStateBool st = m_InputSystem->GetBoolChannel(handle);
    return st.held;
}

float InputManager::GetAxis1D(const std::string& name)
{
    InputChannelHandle handle = GetChannelHandle(name);
    if (handle == INVALID_CHANNEL_HANDLE)
        return 0.0f;

    FActionStateAxis1D st = m_InputSystem->GetAxis1DChannel(handle);
    return st.value;
}

FVector2 InputManager::GetAxis2D(const std::string& name)
{
    InputChannelHandle handle = GetChannelHandle(name);
    if (handle == INVALID_CHANNEL_HANDLE)
        return FVector2(0.f);

    FActionStateAxis2D st = m_InputSystem->GetAxis2DChannel(handle);
    return {st.x, st.y};
}

IHotkeyBindingEditable* InputManager::GetHotkeyBindings()
{
    if (!m_InputSystem)
        return nullptr;

    IInputMappingStyle* style = m_InputSystem->GetMappingStyleRaw();
    if (!style)
        return nullptr;

    return dynamic_cast<IHotkeyBindingEditable*>(style);
}

const IHotkeyBindingEditable* InputManager::GetHotkeyBindings() const
{
    if (!m_InputSystem)
        return nullptr;

    const IInputMappingStyle* style = m_InputSystem->GetMappingStyleRaw();
    if (!style)
        return nullptr;

    return dynamic_cast<const IHotkeyBindingEditable*>(style);
}
