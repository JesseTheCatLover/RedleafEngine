//  Copyright 2025-2026 JesseTheCatLover. All Rights Reserved.

#pragma once

#include <string>
#include <unordered_map>

#include "Core/Math/FVector2.h"
#include "InputSystem/InputCallbacks.h"
#include "InputSystem/InputChannels.h"

class IHotkeyBindingEditable;
using InputCallbackHandle = ::InputCallbackHandle;
using EInputEventPhase = ::EInputEventPhase;
using FBoolActionCallback = ::FBoolActionCallback;
using FAxis1DActionCallback = ::FAxis1DActionCallback;
using FAxis2DActionCallback = ::FAxis2DActionCallback;

class InputSubsystem;

/**
 * @class InputManager
 * @brief Gameplay/editor-facing interface for InputSystem.
 */
class InputManager
{
    friend class JEngine;

public:
    ~InputManager() = default;

    // Non-copyable / non-movable
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;
    InputManager(InputManager&&) = delete;
    InputManager& operator=(InputManager&&) = delete;

private:
    explicit InputManager() = default;

    bool Initialize(InputSubsystem* system);

    /// Non-owning pointer to the engine input subsystem.
    InputSubsystem* m_InputSystem = nullptr;

    /// Cache: name -> channel handle.
    std::unordered_map<std::string, InputChannelHandle> m_Cache;
    uint32_t m_LastChannelVersion = 0;

    /**
     * @brief Returns (and caches) the channel handle for the given name.
     * @note Returns InvalidInputChannel if the name cannot be resolved.
     */
    [[nodiscard]] InputChannelHandle GetChannelHandle(const std::string& name);

public:
    InputCallbackHandle BindRawInputEvent(FRawInputEventCallback callback);
    
    InputCallbackHandle BindAction(const std::string& name, EInputEventPhase phase, FBoolActionCallback cb);

    InputCallbackHandle BindAxis1D(const std::string& name, EInputEventPhase phase, FAxis1DActionCallback cb);

    InputCallbackHandle BindAxis2D(const std::string& name, EInputEventPhase phase, FAxis2DActionCallback cb);

    void Unbind(InputCallbackHandle handle);

    [[nodiscard]] bool GetActionDown (const std::string& name);
    [[nodiscard]] bool GetActionUp (const std::string& name);
    [[nodiscard]] bool GetActionHeld (const std::string& name);

    [[nodiscard]] float GetAxis1D (const std::string& name);
    [[nodiscard]] FVector2 GetAxis2D(const std::string& name);

    [[nodiscard]] IHotkeyBindingEditable* GetHotkeyBindings();
    [[nodiscard]] const IHotkeyBindingEditable* GetHotkeyBindings() const;
};
