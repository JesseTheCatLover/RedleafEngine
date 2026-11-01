//  Copyright 2025 JesseTheCatLover. All Rights Reserved.

#pragma once
#include <vector>

#include "InputSystem/FRawInputEvent.h"

class IPlatformWindow;

class IInputBackend
{
public:
    virtual ~IInputBackend() = default;

    virtual void SetTargetWindow(IPlatformWindow* platformWindow) = 0;

    // Collects all input events since last frame
    virtual void FetchEvents(std::vector<FRawInputEvent>& outEvents) = 0;

    virtual void GetMousePosition(float& outX, float& outY) = 0;
};
