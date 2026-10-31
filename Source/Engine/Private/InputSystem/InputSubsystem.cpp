//  Copyright 2025 JesseTheCatLover. All Rights Reserved.

#include "InputSubsystem.h"
#include <algorithm>
#include <iostream>

namespace
{
    // Helper to find/create a device state entry
    FInputDeviceState& GetOrCreateDevice(
        std::vector<FInputDeviceState>& devices,
        EInputDeviceType type,
        int index)
    {
        for (auto& d : devices)
        {
            if (d.type == type && d.index == index)
                return d;
        }

        FInputDeviceState dev{};
        dev.type  = type;
        dev.index = index;

        devices.push_back(std::move(dev));
        return devices.back();
    }
}

InputSubsystem::InputSubsystem() = default;

bool InputSubsystem::Initialize(IInputBackend* backend)
{
    if (!backend)
    {
        std::cerr << "[InputSubsystem]: failed to initialize input backend.\n";
        return false;
    }

    m_Backend = backend;
    m_Events.clear();
    m_DevicesState.clear();
    m_PrevDevicesState.clear();
    m_Channels.clear();
    m_NameToHandle.clear();
    m_RawInputCallbacks.clear();
    m_BoolCallbacks.clear();
    m_Axis1DCallbacks.clear();
    m_Axis2DCallbacks.clear();
    m_NextCallbackHandle = 1;

    return true;
}

void InputSubsystem::Shutdown()
{
    m_Backend = nullptr;
    m_MappingStyle.reset();
    m_Events.clear();
    m_DevicesState.clear();
    m_PrevDevicesState.clear();
    m_Channels.clear();
    m_NameToHandle.clear();
    m_RawInputCallbacks.clear();
    m_BoolCallbacks.clear();
    m_Axis1DCallbacks.clear();
    m_Axis2DCallbacks.clear();
    m_NextCallbackHandle = 1;
}

// ---------- Callback registration ----------

InputCallbackHandle InputSubsystem::RegisterRawInputEventCallback(FRawInputEventCallback callback)
{
    if (!callback)
        return INVALID_INPUT_CALLBACK;

    FRawInputCallbackEntry entry;
    entry.handle = m_NextCallbackHandle++;
    entry.callback = std::move(callback);

    InputCallbackHandle handle = entry.handle;
    m_RawInputCallbacks.push_back(std::move(entry));

    return handle;
}

void InputSubsystem::DispatchRawInputEvents()
{
    for (const FRawInputEvent& event : m_Events)
    {
        const auto callbacks = m_RawInputCallbacks;

        for (const FRawInputCallbackEntry& entry : callbacks)
        {
            const auto it = std::find_if(
                m_RawInputCallbacks.begin(),
                m_RawInputCallbacks.end(),
                [&entry](const FRawInputCallbackEntry& current)
                {
                    return current.handle == entry.handle;
                });

            if (it == m_RawInputCallbacks.end())
                continue;

            entry.callback(event);
        }
    }
}

InputCallbackHandle InputSubsystem::RegisterBoolCallback(
    const std::string& channelName,
    EInputEventPhase phase,
    FBoolActionCallback cb)
{
    if (!m_MappingStyle || !cb)
        return INVALID_INPUT_CALLBACK;

    InputChannelHandle handle = FindChannelIdByName(channelName);
    if (handle == INVALID_CHANNEL_HANDLE)
        return INVALID_INPUT_CALLBACK;

    FBoolCallbackEntry entry;
    entry.handle        = m_NextCallbackHandle++;
    entry.channelName   = channelName;
    entry.channelHandle = handle;
    entry.phase         = phase;
    entry.callback      = std::move(cb);

    m_BoolCallbacks.push_back(std::move(entry));
    return entry.handle;
}

InputCallbackHandle InputSubsystem::RegisterAxis1DCallback(
    const std::string& channelName,
    EInputEventPhase phase,
    FAxis1DActionCallback cb)
{
    if (!m_MappingStyle || !cb)
        return INVALID_INPUT_CALLBACK;

    InputChannelHandle handle = FindChannelIdByName(channelName);
    if (handle == INVALID_CHANNEL_HANDLE)
        return INVALID_INPUT_CALLBACK;

    FAxis1DCallbackEntry entry;
    entry.handle        = m_NextCallbackHandle++;
    entry.channelName   = channelName;
    entry.channelHandle = handle;
    entry.phase         = phase;
    entry.callback      = std::move(cb);

    m_Axis1DCallbacks.push_back(std::move(entry));
    return entry.handle;
}

InputCallbackHandle InputSubsystem::RegisterAxis2DCallback(
    const std::string& channelName,
    EInputEventPhase phase,
    FAxis2DActionCallback cb)
{
    if (!m_MappingStyle || !cb)
        return INVALID_INPUT_CALLBACK;

    InputChannelHandle handle = FindChannelIdByName(channelName);
    if (handle == INVALID_CHANNEL_HANDLE)
        return INVALID_INPUT_CALLBACK;

    FAxis2DCallbackEntry entry;
    entry.handle        = m_NextCallbackHandle++;
    entry.channelName   = channelName;
    entry.channelHandle = handle;
    entry.phase         = phase;
    entry.callback      = std::move(cb);

    m_Axis2DCallbacks.push_back(std::move(entry));
    return entry.handle;
}

void InputSubsystem::UnregisterCallback(InputCallbackHandle handle)
{
    if (handle == INVALID_INPUT_CALLBACK)
        return;

    auto removeByHandle = [handle](auto& vec)
    {
        vec.erase(
            std::remove_if(vec.begin(), vec.end(),
                [handle](const auto& e) { return e.handle == handle; }),
            vec.end());
    };

    removeByHandle(m_RawInputCallbacks);

    removeByHandle(m_BoolCallbacks);
    removeByHandle(m_Axis1DCallbacks);
    removeByHandle(m_Axis2DCallbacks);
}

// ---------- Core processing ----------

void InputSubsystem::ProcessEvents()
{
    // 1) Clear per-frame mouse deltas (delta-style inputs)
    for (auto& mouse : m_DevicesState)
    {
        if (mouse.type == EInputDeviceType::Mouse)
        {
            mouse.values[EPhysicalInput::Mouse_DeltaX]   = 0.0f;
            mouse.values[EPhysicalInput::Mouse_DeltaY]   = 0.0f;
            mouse.values[EPhysicalInput::Mouse_WheelX]  = 0.0f;
            mouse.values[EPhysicalInput::Mouse_WheelY]  = 0.0f;
        }
    }

    // 2) Apply raw events to device state
    for (const FRawInputEvent& e : m_Events)
    {
        const EPhysicalInput phys = static_cast<EPhysicalInput>(e.code);

        switch (e.type)
        {
        // ---------- KEYBOARD ----------
        case ERawInputType::KeyDown:
        case ERawInputType::KeyUp:
        {
            FInputDeviceState& kb =
                GetOrCreateDevice(m_DevicesState, EInputDeviceType::Keyboard, /*index*/ 0);

            // 1.0 = pressed, 0.0 = released
            kb.values[phys] = (e.type == ERawInputType::KeyDown) ? 1.0f : 0.0f;
        }
        break;

        // ---------- MOUSE (buttons + move + wheel) ----------
            case ERawInputType::MouseButtonDown:
            case ERawInputType::MouseButtonUp:
            {
                FInputDeviceState& mouse =
                    GetOrCreateDevice(m_DevicesState, EInputDeviceType::Mouse, 0);

                mouse.values[phys] = e.value;
            }
            break;

            case ERawInputType::MouseMove:
            {
                FInputDeviceState& mouse =
                    GetOrCreateDevice(m_DevicesState, EInputDeviceType::Mouse, 0);

                mouse.values[EPhysicalInput::Mouse_DeltaX] += e.deltaX;
                mouse.values[EPhysicalInput::Mouse_DeltaY] += e.deltaY;
            }
            break;

            case ERawInputType::MouseWheel:
            {
                FInputDeviceState& mouse =
                    GetOrCreateDevice(m_DevicesState, EInputDeviceType::Mouse, 0);

                mouse.values[EPhysicalInput::Mouse_WheelX] += e.scrollX;
                mouse.values[EPhysicalInput::Mouse_WheelY] += e.scrollY;
            }
            break;

        // ---------- GAMEPAD ----------
        case ERawInputType::GamepadButtonDown:
        case ERawInputType::GamepadButtonUp:
        case ERawInputType::GamepadAxis:
        {
            FInputDeviceState& pad =
                GetOrCreateDevice(m_DevicesState, EInputDeviceType::Gamepad, e.deviceID);

            // 0/1 for buttons, -1..1 for axes
            pad.values[phys] = e.value;
        }
        break;

        case ERawInputType::TextInput:
            // UI / text system can read from m_Events directly if needed.
            break;
        }
    }
}

void InputSubsystem::SetMappingStyle(TUniquePtr<IInputMappingStyle> style)
{
    m_MappingStyle = std::move(style);
    RebuildChannels();
}

void InputSubsystem::Tick(float deltaTime)
{
    if (!m_Backend)
    {
        std::cerr << "[InputSubsystem]: no backend provided, cannot tick.\n";
        return;
    }

    // 1) Get raw events from backend
    m_Events.clear();
    m_Backend->FetchEvents(m_Events);

    // 2) Notify raw input observers
    DispatchRawInputEvents();

    // 3) Save previous device states
    m_PrevDevicesState = m_DevicesState;

    // 4) Apply events -> current device state
    ProcessEvents();

    // 5) Update logical channels
    if (m_MappingStyle)
        m_MappingStyle->UpdateChannels(deltaTime, m_DevicesState, m_PrevDevicesState, m_ChannelData);

    // 6) Dispatch logical callbacks
    if (m_MappingStyle)
        DispatchCallbacks();
}

// ---------- Callback dispatch ----------

void InputSubsystem::DispatchCallbacks()
{
    if (!m_MappingStyle)
        return;

    const size_t channelCount = m_Channels.size();

    for (size_t i = 0; i < channelCount; ++i)
    {
        const FInputChannelDesc& desc = m_Channels[i];
        InputChannelHandle handle     = desc.handle;

        switch (desc.type)
        {
        case EInputChannelType::Bool:
        {
            FActionStateBool st = m_MappingStyle->GetBoolState(handle);

            for (const auto& entry : m_BoolCallbacks)
            {
                if (entry.channelHandle != handle ||
                    entry.channelHandle == INVALID_CHANNEL_HANDLE)
                    continue;

                switch (entry.phase)
                {
                case EInputEventPhase::Pressed:
                    if (st.pressed)
                        entry.callback(handle, st);
                    break;
                case EInputEventPhase::Released:
                    if (st.released)
                        entry.callback(handle, st);
                    break;
                case EInputEventPhase::Held:
                    if (st.held)
                        entry.callback(handle, st);
                    break;
                default:
                    break;
                }
            }
        }
        break;

        case EInputChannelType::Axis1D:
        {
            FActionStateAxis1D st = m_MappingStyle->GetAxis1DState(handle);

            for (const auto& entry : m_Axis1DCallbacks)
            {
                if (entry.channelHandle != handle ||
                    entry.channelHandle == INVALID_CHANNEL_HANDLE)
                    continue;

                switch (entry.phase)
                {
                case EInputEventPhase::AxisChanged:
                    if (st.delta != 0.0f)
                        entry.callback(handle, st);
                    break;
                case EInputEventPhase::Held:
                    if (st.value != 0.0f)
                        entry.callback(handle, st);
                    break;
                default:
                    break;
                }
            }
        }
        break;

        case EInputChannelType::Axis2D:
        {
            FActionStateAxis2D st = m_MappingStyle->GetAxis2DState(handle);

            for (const auto& entry : m_Axis2DCallbacks)
            {
                if (entry.channelHandle != handle ||
                    entry.channelHandle == INVALID_CHANNEL_HANDLE)
                    continue;

                switch (entry.phase)
                {
                case EInputEventPhase::AxisChanged:
                    if (st.dx != 0.0f || st.dy != 0.0f)
                        entry.callback(handle, st);
                    break;
                case EInputEventPhase::Held:
                    if (st.x != 0.0f || st.y != 0.0f)
                        entry.callback(handle, st);
                    break;
                default:
                    break;
                }
            }
        }
        break;
        }
    }
}

// ---------- Channel queries ----------

FActionStateBool InputSubsystem::GetBoolChannel(InputChannelHandle handle) const
{
    if (!m_MappingStyle)
        return FActionStateBool{};
    return m_MappingStyle->GetBoolState(handle);
}

FActionStateAxis1D InputSubsystem::GetAxis1DChannel(InputChannelHandle handle) const
{
    if (!m_MappingStyle)
        return FActionStateAxis1D{};
    return m_MappingStyle->GetAxis1DState(handle);
}

FActionStateAxis2D InputSubsystem::GetAxis2DChannel(InputChannelHandle handle) const
{
    if (!m_MappingStyle)
        return FActionStateAxis2D{};
    return m_MappingStyle->GetAxis2DState(handle);
}

// ---------- Channels / mapping ----------

InputChannelHandle InputSubsystem::FindChannelIdByName(const std::string& name) const
{
    auto it = m_NameToHandle.find(name);
    if (it == m_NameToHandle.end())
        return INVALID_CHANNEL_HANDLE;
    return it->second;
}

void InputSubsystem::RebuildChannels()
{
    m_Channels.clear();
    m_NameToHandle.clear();
    m_ChannelData.clear();

    if (!m_MappingStyle)
        return;

    m_MappingStyle->BuildChannels(m_Channels);

    for (const FInputChannelDesc& desc : m_Channels)
    {
        m_NameToHandle[desc.name] = desc.handle;
    }

    ++m_ChannelVersion;

    for (auto& entry : m_BoolCallbacks)
        entry.channelHandle = FindChannelIdByName(entry.channelName);
    for (auto& entry : m_Axis1DCallbacks)
        entry.channelHandle = FindChannelIdByName(entry.channelName);
    for (auto& entry : m_Axis2DCallbacks)
        entry.channelHandle = FindChannelIdByName(entry.channelName);
}