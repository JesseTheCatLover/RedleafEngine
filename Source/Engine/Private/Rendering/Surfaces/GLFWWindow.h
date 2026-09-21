//  Copyright 2025-2026 JesseTheCatLover. All Rights Reserved.

#pragma once
#include "Rendering/IPlatformWindow.h"

struct GLFWwindow;

class GLFWWindow : public IPlatformWindow
{
public:
    using FResizeCallback = IPlatformWindow::FResizeCallback;
    using FFocusCallback = IPlatformWindow::FFocusCallback;

    using FCloseHandler = std::function<void(GLFWWindow&)>;

private:
    GLFWwindow* m_Window = nullptr;
    GLFWwindow* m_ShareContext = nullptr;
    FWindowDesc m_State{};
    ECursorMode m_CursorMode = ECursorMode::Visible;

    FResizeCallback m_FramebufferResizeCallback;
    FResizeCallback m_WindowResizeCallback;
    FFocusCallback m_FocusCallback;
    FCloseHandler m_CloseHandler;

    friend class GLFWSurface;

    int m_WindowedX = 0;
    int m_WindowedY = 0;
    int m_WindowedWidth = 0;
    int m_WindowedHeight = 0;

    bool m_bHasSavedWindowedGeometry = false;

    // Lifecycle of this window only (Handled by the surface)
    void Shutdown();

    // Set by the surface; used by the window to delegate close requests
    void SetCloseHandler(FCloseHandler handler);

public:
    explicit GLFWWindow(const FWindowDesc& initialState, GLFWwindow* shareContext = nullptr);
    ~GLFWWindow() override;

    bool Initialize();

    void SetSurfaceSize(int width, int height) override;
    void GetWindowSize(int& w, int& h) const override;
    void GetFramebufferSize(int& w, int& h) const override;
    void GetWindowFrameSize(int &left, int &top, int &right, int &bottom) const override;

    bool ShouldClose() const override;
    void SetShouldClose(bool bShould) override;

    int GetWidth() const override;
    int GetHeight() const override;
    float GetAspectRatio() const override;

    void GetMonitorWorkArea(int &x, int &y, int &width, int &height) const override;

    void SetMinSize(int minWidth, int minHeight) override;
    void SetMaxSize(int maxWidth, int maxHeight) override;

    auto IsVSyncEnabled() const -> bool override;
    bool IsFullscreen() const override;
    FWindowDesc GetWindowState() const override;

    void SetWindowState(EWindowState state) override;

    void SetPosition(int x, int y) override;

    void Show() override;
    void Hide() override;
    bool IsVisible() const override;

    void SetTitle(const std::string& title) override;
    void SetVSync(bool vSync) override;

#if defined(JENGINE_PLATFORM_WINDOWS)
    void SetIcon(const std::string &iconPath) override;
#endif

    std::string GetTitle() override;

    void* GetNativeHandle() const override;

    void SetCursorMode(ECursorMode mode) override;
    void SetCursorVisible() override;
    void SetCursorHidden() override;
    void SetCursorDisabled() override;

    void SetFramebufferResizeCallback(FResizeCallback callback) override;
    void SetWindowResizeCallback(FResizeCallback callback) override;
    void SetFocusCallback(FFocusCallback callback) override;

private:
    // Internal helper
    void UpdateCursor();
};
