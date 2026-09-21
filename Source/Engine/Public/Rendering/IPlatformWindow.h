//  Copyright 2025-2026 JesseTheCatLover. All Rights Reserved.

#pragma once
#include <string>
#include <functional>
#include "Rendering/WindowDescTypes.h"

class IPlatformWindow
{
public:
    using FResizeCallback = std::function<void(int width, int height)>;
    using FFocusCallback = std::function<void(IPlatformWindow&, bool)>;

public:
    virtual ~IPlatformWindow() = default;

    // Close / lifetime

    [[nodiscard]] virtual bool ShouldClose() const = 0;
    virtual void SetShouldClose(bool bShould) = 0;

    // Size / geometry

    virtual void SetSurfaceSize(int width, int height) = 0;
    virtual void GetWindowSize(int& w, int& h) const = 0;
    virtual void GetFramebufferSize(int& w, int& h) const = 0;
    virtual void GetWindowFrameSize(int& left, int& top, int& right, int& bottom) const = 0;

    [[nodiscard]] virtual int GetWidth() const = 0;
    [[nodiscard]] virtual int GetHeight() const = 0;
    [[nodiscard]] virtual float GetAspectRatio() const = 0;

    virtual void GetMonitorWorkArea(int& x, int& y, int& width, int& height) const = 0;

    virtual void SetMinSize(int minWidth, int minHeight) {}
    virtual void SetMaxSize(int maxWidth, int maxHeight) {}

    virtual void SetWindowState(EWindowState state) = 0;

    // State and meta

    [[nodiscard]] virtual bool IsVSyncEnabled() const = 0;
    [[nodiscard]] virtual bool IsFullscreen() const = 0;
    [[nodiscard]] virtual FWindowDesc GetWindowState() const = 0;
    virtual void SetPosition(int x, int y) = 0;

    virtual void Show() = 0;
    virtual void Hide() = 0;
    virtual bool IsVisible() const = 0;

    virtual void SetTitle(const std::string& title) = 0;
    virtual void SetVSync(bool vSync) = 0;

    virtual void SetIcon(const std::string& iconPath) {}

    virtual std::string GetTitle() = 0;

    // Native handle (HWND/NSWindow/GLFWwindow/XboxSwapChain/etc.)
    virtual void* GetNativeHandle() const = 0;

    // Cursor control per window (can be no-op for some platforms)

    virtual void SetCursorMode(ECursorMode mode) {}
    virtual void SetCursorVisible() {}
    virtual void SetCursorHidden() {}
    virtual void SetCursorDisabled() {}

    // Per-window callbacks

    virtual void SetFramebufferResizeCallback(FResizeCallback callback) {}
    virtual void SetWindowResizeCallback(FResizeCallback callback) {}
    virtual void SetFocusCallback(FFocusCallback callback) {}
};
