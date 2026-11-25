//  Copyright 2025-2026 JesseTheCatLover. All Rights Reserved.

#pragma once
#include <string>
#include <functional>

#include "WindowDescTypes.h"
#include "Core/Memory/SmartPointers.h"

class IPlatformWindow;

class IPlatformSurface
{
public:
    using GetProcAddressFunc = void* (*)(const char*);
    using FOnWindowFocusChanged = std::function<void(TSharedPtr<IPlatformWindow> window, bool bFocused)>;

public:
    virtual ~IPlatformSurface() = default;

    // System-level initialization. No window state here.

    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;

    // Window management

    virtual TSharedPtr<IPlatformWindow> CreateWindow(const FWindowDesc& windowDesc, bool bPrimary = false) = 0;
    virtual void DestroyWindow(const TSharedPtr<IPlatformWindow>& window) = 0;

    // Primary window: stable anchor for engine
    [[nodiscard]] virtual TSharedPtr<IPlatformWindow> GetPrimaryWindow() const = 0;

    // Focused window: currently active for input
    [[nodiscard]] virtual TSharedPtr<IPlatformWindow> GetFocusedWindow() const = 0;

    // Effective input window: focus if present, otherwise primary
    [[nodiscard]] virtual TSharedPtr<IPlatformWindow> GetEffectiveInputWindow() const = 0;

    // List all windows
    [[nodiscard]] virtual std::vector<TSharedPtr<IPlatformWindow>> GetAllWindows() const = 0;

    virtual void SetWindowFocusChangedDelegate(FOnWindowFocusChanged delegate) = 0;

    // Bind a given window’s context (backend-agnostic wrapper)
    virtual void MakeContextCurrent(const TSharedPtr<IPlatformWindow>& window) = 0;

    virtual void Present(const TSharedPtr<IPlatformWindow>& window) = 0;

    // Optional override, for GL/EGL backends that use implicit swap-chains
    virtual void SwapBuffers(const TSharedPtr<IPlatformWindow>& window) {}

    // Process OS/system events for all windows
    virtual void PollSurfaceEvents() = 0;

    virtual float GetTimeSeconds() = 0;

    virtual void* GetPlatformSpecificHandle() const { return nullptr; } // optional override

    // Optional: for OpenGL-style loaders
    virtual GetProcAddressFunc GetProcAddressFunction() const { return nullptr; }

    // Native file dialogues:

    // Return empty string if user cancels.
    virtual std::string OpenFileDialog(
        const char* filterList,   // e.g. "png,jpg;fbx,obj"
        const char* defaultPath)  // nullptr = OS default
    = 0;

    // Multiple selection
    virtual std::vector<std::string> OpenFileDialogMultiple(
        const char* filterList,
        const char* defaultPath) = 0;

    // Folder picker
    virtual std::string OpenFolderDialog(const char* defaultPath) = 0;

    // Save dialog
    virtual std::string SaveFileDialog(
        const char* filterList,
        const char* defaultPath,
        const char* defaultName) = 0;

    virtual bool TryGetClipboardText(std::string& outText) = 0;
    virtual void SetClipboardText(std::string_view text) = 0;
};
