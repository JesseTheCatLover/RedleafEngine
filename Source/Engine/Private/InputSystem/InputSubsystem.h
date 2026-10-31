//  Copyright 2025 JesseTheCatLover. All Rights Reserved.

#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include <cstdint>

#include "IInputBackend.h"
#include "Core/Memory/SmartPointers.h"
#include "InputSystem/MappingStyles/ActionAxis/ActionAxisStates.h"
#include "InputSystem/InputCallbacks.h"
#include "InputSystem/MappingStyles/IInputMappingStyle.h"
#include "InputSystem/FInputDeviceState.h"

/**
 * @class InputSubsystem
 * @brief Engine-internal subsystem that converts raw OS events -> device state -> logical channels.
 */
class InputSubsystem
{
    friend class JEngine;
    friend class InputManager;

public:
    ~InputSubsystem() = default;

    // Non-copyable / non-movable
    InputSubsystem(const InputSubsystem&) = delete;
    InputSubsystem& operator=(const InputSubsystem&) = delete;
    InputSubsystem(InputSubsystem&&) = delete;
    InputSubsystem& operator=(InputSubsystem&&) = delete;

    // Set the mapping style
    void SetMappingStyle(TUniquePtr<IInputMappingStyle> style);

    [[nodiscard]] const std::vector<FInputDeviceState>& GetCurrentDevicesState() const { return m_DevicesState; }
    [[nodiscard]] const std::vector<FInputDeviceState>& GetPreviousDevicesState() const { return m_PrevDevicesState; }

    [[nodiscard]] IInputMappingStyle* GetMappingStyleRaw() { return m_MappingStyle.get(); }
    [[nodiscard]] const IInputMappingStyle* GetMappingStyleRaw() const { return m_MappingStyle.get(); }

private:
    InputSubsystem();

    bool Initialize(IInputBackend* backend);
    void Shutdown();

    // Called once per engine frame
    void Tick(float deltaTime);

    // Turn channel states into callback invocations
    void DispatchCallbacks();

    // Backend providing raw OS events
    IInputBackend* m_Backend = nullptr;

    // Mapping style (ActionAxisStyle or future ones)
    TUniquePtr<IInputMappingStyle> m_MappingStyle;

    // Per-frame raw events
    std::vector<FRawInputEvent> m_Events;

    // Current + previous device state (per keyboard/mouse/gamepad)
    std::vector<FInputDeviceState> m_DevicesState;
    std::vector<FInputDeviceState> m_PrevDevicesState;

    // Active logical channels
    std::vector<FInputChannelDesc> m_Channels;

    // Optional packed channel data (unused currently but useful for future patterns)
    std::vector<float> m_ChannelData;
    uint32_t m_ChannelVersion = 0;

    // Fast lookup: "Jump" -> channel handle
    std::unordered_map<std::string, InputChannelHandle> m_NameToHandle;

    // ---- Callback storage ----
    struct FRawInputCallbackEntry
    {
        InputCallbackHandle handle{};
        FRawInputEventCallback callback{};
    };

    std::vector<FRawInputCallbackEntry> m_RawInputCallbacks;

    struct FBoolCallbackEntry
    {
        InputCallbackHandle handle{};
        std::string channelName;
        InputChannelHandle channelHandle{ INVALID_CHANNEL_HANDLE };
        EInputEventPhase phase{};
        FBoolActionCallback callback{};
    };

    struct FAxis1DCallbackEntry
    {
        InputCallbackHandle handle{};
        std::string channelName;
        InputChannelHandle channelHandle{ INVALID_CHANNEL_HANDLE };
        EInputEventPhase phase{};
        FAxis1DActionCallback callback{};
    };

    struct FAxis2DCallbackEntry
    {
        InputCallbackHandle handle{};
        std::string channelName;
        InputChannelHandle channelHandle{ INVALID_CHANNEL_HANDLE };
        EInputEventPhase phase{};
        FAxis2DActionCallback callback{};
    };

    std::vector<FBoolCallbackEntry>  m_BoolCallbacks;
    std::vector<FAxis1DCallbackEntry> m_Axis1DCallbacks;
    std::vector<FAxis2DCallbackEntry> m_Axis2DCallbacks;

    InputCallbackHandle m_NextCallbackHandle = 1;

    // ---- Callback Registration API ----
    InputCallbackHandle RegisterRawInputEventCallback(FRawInputEventCallback callback);
    void DispatchRawInputEvents();

    InputCallbackHandle RegisterBoolCallback(
        const std::string& channelName,
        EInputEventPhase phase,
        FBoolActionCallback cb);

    InputCallbackHandle RegisterAxis1DCallback(
        const std::string& channelName,
        EInputEventPhase phase,
        FAxis1DActionCallback cb);

    InputCallbackHandle RegisterAxis2DCallback(
        const std::string& channelName,
        EInputEventPhase phase,
        FAxis2DActionCallback cb);

    void UnregisterCallback(InputCallbackHandle handle);

    // ---- Channel Queries ----
    [[nodiscard]] FActionStateBool  GetBoolChannel (InputChannelHandle handle) const;
    [[nodiscard]] FActionStateAxis1D GetAxis1DChannel(InputChannelHandle handle) const;
    [[nodiscard]] FActionStateAxis2D GetAxis2DChannel(InputChannelHandle handle) const;

    [[nodiscard]] uint32_t GetChannelVersion() const { return m_ChannelVersion; }
    [[nodiscard]] InputChannelHandle FindChannelIdByName(const std::string& name) const;

    // ---- Low-level pipeline ----
    void ProcessEvents();
    void RebuildChannels();
};
