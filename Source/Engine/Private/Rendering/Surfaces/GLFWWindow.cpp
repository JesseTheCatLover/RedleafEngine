//  Copyright 2025-2026 JesseTheCatLover. All Rights Reserved.

#include "GLFWWindow.h"

#include "GLFW/glfw3.h"
#include <iostream>

#if defined(JENGINE_PLATFORM_WINDOWS)
#include "stb/stb_image.h"
#endif

GLFWWindow::GLFWWindow(const FWindowDesc& initialState, GLFWwindow* shareContext)
    : m_State(initialState)
    , m_ShareContext(shareContext)
{
}

GLFWWindow::~GLFWWindow()
{
}

bool GLFWWindow::Initialize()
{
    // GLFW must already be initialized by GLFWSurface::Initialize()

    GLFWmonitor* monitor = static_cast<GLFWmonitor*>(m_State.monitorHandle);
    if (!monitor)
        monitor = glfwGetPrimaryMonitor();

    if (IsFullscreen())
    {
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if (!mode)
            return false;

        glfwWindowHint(GLFW_RED_BITS,     mode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS,   mode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS,    mode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

        m_State.width  = mode->width;
        m_State.height = mode->height;
    }
    else
    {
        // windowed / maximized
        glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);
        glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);
        glfwWindowHint(GLFW_FLOATING, GLFW_FALSE);

        glfwWindowHint(GLFW_MAXIMIZED, m_State.windowState == EWindowState::Maximized ? GLFW_TRUE : GLFW_FALSE);

        // keep width/height from m_State as passed in
    }

    // Before creating the window, set hints based on m_State
    glfwWindowHint(GLFW_RESIZABLE, m_State.bResizable ? GLFW_TRUE : GLFW_FALSE);

    GLFWmonitor* winMonitor = IsFullscreen() ? monitor : nullptr;
    m_Window = glfwCreateWindow(
        m_State.width,
        m_State.height,
        m_State.title.c_str(),
        winMonitor,
        m_ShareContext
    );

    if (!m_Window)
    {
        std::cerr << "[GLFWWindow]: Failed to create GLFW window\n";
        return false;
    }

    // if (m_State.windowState == EWindowState::Maximized)
    // {
    //     glfwMaximizeWindow(m_Window);
    // }

    m_State.nativeHandle  = m_Window;
    m_State.monitorHandle = monitor;

    glfwSetWindowUserPointer(m_Window, this);

    // --- Clamp minimum/maximum size if specified ---
    int minW = (m_State.minWidth  > 0) ? m_State.minWidth  : GLFW_DONT_CARE;
    int minH = (m_State.minHeight > 0) ? m_State.minHeight : GLFW_DONT_CARE;
    int maxW = (m_State.maxWidth  > 0) ? m_State.maxWidth  : GLFW_DONT_CARE;
    int maxH = (m_State.maxHeight > 0) ? m_State.maxHeight : GLFW_DONT_CARE;

    glfwSetWindowSizeLimits(
        m_Window,
        minW, minH,   // min width/height
        maxW, maxH    // max width/height
    );

    // Framebuffer resize callback
    glfwSetFramebufferSizeCallback(
        m_Window,
        [](GLFWwindow* window, int width, int height)
        {
            auto* self = static_cast<GLFWWindow*>(glfwGetWindowUserPointer(window));
            if (!self) return;

            if (self->m_FramebufferResizeCallback)
                self->m_FramebufferResizeCallback(width, height);
        });

    // Window size callback
    glfwSetWindowSizeCallback(
        m_Window,
        [](GLFWwindow* window, int width, int height)
        {
            auto* self = static_cast<GLFWWindow*>(glfwGetWindowUserPointer(window));
            if (!self) return;

            // update cached size
            self->m_State.width  = width;
            self->m_State.height = height;

            if (self->m_WindowResizeCallback)
                self->m_WindowResizeCallback(width, height);
        });

    // Window focus callback
    glfwSetWindowFocusCallback(
    m_Window,
    [](GLFWwindow* window, int focused)
    {
        auto* self =
            static_cast<GLFWWindow*>(
                glfwGetWindowUserPointer(window));

        if (!self)
            return;

        if (self->m_FocusCallback)
        {
            self->m_FocusCallback(
                *self,
                focused == GLFW_TRUE);
        }
    });

    // Window close callback
    glfwSetWindowCloseCallback(
        m_Window,
        [](GLFWwindow* window)
        {
            auto* self = static_cast<GLFWWindow*>(glfwGetWindowUserPointer(window));
            if (!self) return;

            // This will call the close handler, which will call surface->DestroyWindow(...)
            self->SetShouldClose(true);
        });

    // Apply vsync
    glfwSwapInterval(m_State.bvSync ? 1 : 0);

    UpdateCursor();
    return true;
}

void GLFWWindow::Shutdown()
{
    if (m_Window)
    {
        // Clear the user pointer so any future GLFW callbacks become harmless
        glfwSetWindowUserPointer(m_Window, nullptr);

        glfwDestroyWindow(m_Window);
        m_Window = nullptr;
    }
}

void GLFWWindow::SetCloseHandler(FCloseHandler handler)
{
    m_CloseHandler = std::move(handler);
}

void GLFWWindow::SetSurfaceSize(int width, int height)
{
    m_State.width  = width;
    m_State.height = height;

    if (m_Window)
        glfwSetWindowSize(m_Window, width, height);
}

void GLFWWindow::GetWindowSize(int& w, int& h) const
{
    if (m_Window)
    {
        glfwGetWindowSize(m_Window, &w, &h);
    }
    else
    {
        w = 0;
        h = 0;
    }
}

void GLFWWindow::GetFramebufferSize(int& w, int& h) const
{
    if (m_Window)
    {
        glfwGetFramebufferSize(m_Window, &w, &h);
    }
    else
    {
        w = 0;
        h = 0;
    }
}

void GLFWWindow::GetWindowFrameSize(int &left, int &top, int &right, int &bottom) const
{
    if (!m_Window)
    {
        left = 0;
        top = 0;
        right = 0;
        bottom = 0;
        return;
    }

    glfwGetWindowFrameSize(m_Window, &left, &top, &right, &bottom);
}

bool GLFWWindow::ShouldClose() const
{
    return m_Window ? glfwWindowShouldClose(m_Window) == GLFW_TRUE : true;
}

void GLFWWindow::SetShouldClose(bool bShould)
{
    if (!m_Window)
        return;

    // Let GLFW know for consistency (e.g., ShouldClose() checks, etc.)
    glfwSetWindowShouldClose(m_Window, bShould ? GLFW_TRUE : GLFW_FALSE);

    if (bShould && m_CloseHandler)
    {
        // IMPORTANT: do NOT destroy the window here.
        // Just notify the surface via the handler. The surface will call Shutdown() and erase from its list.
        m_CloseHandler(*this);
    }
}

int GLFWWindow::GetWidth() const
{
    return m_State.width;
}

int GLFWWindow::GetHeight() const
{
    return m_State.height;
}

float GLFWWindow::GetAspectRatio() const
{
    if (m_State.height <= 0 || m_State.width <= 0)
        return 1.0f;
    return static_cast<float>(m_State.width) / static_cast<float>(m_State.height);
}

void GLFWWindow::GetMonitorWorkArea(int &x, int &y, int &width, int &height) const
{
    GLFWmonitor* monitor = static_cast<GLFWmonitor*>(m_State.monitorHandle);

    if (!monitor)
    {
        monitor = glfwGetPrimaryMonitor();
    }

    if (!monitor)
    {
        x = 0;
        y = 0;
        width = 0;
        height = 0;
        return;
    }

    glfwGetMonitorWorkarea(monitor, &x, &y, &width, &height);
}

void GLFWWindow::SetMinSize(int minWidth, int minHeight)
{
    m_State.minWidth  = minWidth;
    m_State.minHeight = minHeight;

    if (m_Window)
    {
        int minW = (minWidth  > 0) ? minWidth  : GLFW_DONT_CARE;
        int minH = (minHeight > 0) ? minHeight : GLFW_DONT_CARE;

        // Keep current max limits
        int maxW = (m_State.maxWidth  > 0) ? m_State.maxWidth  : GLFW_DONT_CARE;
        int maxH = (m_State.maxHeight > 0) ? m_State.maxHeight : GLFW_DONT_CARE;

        glfwSetWindowSizeLimits(m_Window, minW, minH, maxW, maxH);
    }
}

void GLFWWindow::SetMaxSize(int maxWidth, int maxHeight)
{
    m_State.maxWidth  = maxWidth;
    m_State.maxHeight = maxHeight;

    if (m_Window)
    {
        // Keep current min limits
        int minW = (m_State.minWidth  > 0) ? m_State.minWidth  : GLFW_DONT_CARE;
        int minH = (m_State.minHeight > 0) ? m_State.minHeight : GLFW_DONT_CARE;

        int maxW = (maxWidth  > 0) ? maxWidth  : GLFW_DONT_CARE;
        int maxH = (maxHeight > 0) ? maxHeight : GLFW_DONT_CARE;

        glfwSetWindowSizeLimits(m_Window, minW, minH, maxW, maxH);
    }
}

bool GLFWWindow::IsVSyncEnabled() const
{
    return m_State.bvSync;
}

bool GLFWWindow::IsFullscreen() const
{
    return m_State.windowState == EWindowState::Fullscreen;
}

FWindowDesc GLFWWindow::GetWindowState() const
{
    return m_State;
}

void GLFWWindow::SetWindowState(EWindowState state)
{
    if (!m_Window)
        return;

    if (m_State.windowState == state)
        return;

    GLFWmonitor* monitor = static_cast<GLFWmonitor*>(m_State.monitorHandle);
    if (!monitor)
        monitor = glfwGetPrimaryMonitor();

    if (!monitor)
        return;

    switch (state)
    {
        case EWindowState::Normal:
        {
            if (m_State.windowState == EWindowState::Fullscreen)
            {
                if (!m_bHasSavedWindowedGeometry)
                {
                    m_State.windowState = EWindowState::Normal;

                    glfwRestoreWindow(m_Window);
                    return;
                }

                glfwSetWindowMonitor(
                    m_Window,
                    nullptr,
                    m_WindowedX,
                    m_WindowedY,
                    m_WindowedWidth,
                    m_WindowedHeight,
                    GLFW_DONT_CARE
                );
            }
            else
            {
                glfwRestoreWindow(m_Window);
            }

            m_State.windowState =
                EWindowState::Normal;

            break;
        }

        case EWindowState::Maximized:
        {
            if (m_State.windowState == EWindowState::Fullscreen)
            {
                if (m_bHasSavedWindowedGeometry)
                {
                    glfwSetWindowMonitor(
                        m_Window,
                        nullptr,
                        m_WindowedX,
                        m_WindowedY,
                        m_WindowedWidth,
                        m_WindowedHeight,
                        GLFW_DONT_CARE
                    );
                }
                else
                {
                    glfwRestoreWindow(m_Window);
                }
            }

            glfwMaximizeWindow(m_Window);

            m_State.windowState =
                EWindowState::Maximized;

            break;
        }

        case EWindowState::Fullscreen:
        {
            // Preserve the current windowed geometry so we
            // can restore it when leaving fullscreen.
            if (m_State.windowState != EWindowState::Fullscreen)
            {
                glfwGetWindowPos(
                    m_Window,
                    &m_WindowedX,
                    &m_WindowedY
                );

                glfwGetWindowSize(
                    m_Window,
                    &m_WindowedWidth,
                    &m_WindowedHeight
                );

                m_bHasSavedWindowedGeometry = true;
            }

            const GLFWvidmode* mode =
                glfwGetVideoMode(monitor);

            if (!mode)
                return;

            glfwSetWindowMonitor(
                m_Window,
                monitor,
                0,
                0,
                mode->width,
                mode->height,
                mode->refreshRate
            );

            m_State.width = mode->width;
            m_State.height = mode->height;
            m_State.windowState =
                EWindowState::Fullscreen;

            break;
        }

        default:
            break;
    }
}

void GLFWWindow::SetPosition(int x, int y)
{
    if (!m_Window)
        return;

    glfwSetWindowPos(
        m_Window,
        x,
        y
    );
}

void GLFWWindow::Show()
{
    glfwShowWindow(m_Window);
}

void GLFWWindow::Hide()
{
    glfwHideWindow(m_Window);
}

bool GLFWWindow::IsVisible() const
{
    return glfwGetWindowAttrib(m_Window, GLFW_VISIBLE) == GLFW_TRUE;
}

void GLFWWindow::SetTitle(const std::string& title)
{
    m_State.title = title;
    if (m_Window)
        glfwSetWindowTitle(m_Window, title.c_str());
}

void GLFWWindow::SetVSync(bool vSync)
{
    m_State.bvSync = vSync;
    if (m_Window)
        glfwSwapInterval(vSync ? 1 : 0);
}

#if defined(JENGINE_PLATFORM_WINDOWS)
void GLFWWindow::SetIcon(const std::string& iconPath)
{
    int width = 0;
    int height = 0;
    int channels = 0;

    unsigned char* pixels = stbi_load(
        iconPath.c_str(),
        &width,
        &height,
        &channels,
        4
    );

    if (!pixels)
        return;

    GLFWimage image{
        width,
        height,
        pixels
    };

    glfwSetWindowIcon(
        m_Window,
        1,
        &image
    );

    stbi_image_free(pixels);
}
#endif

std::string GLFWWindow::GetTitle()
{
    return std::string(glfwGetWindowTitle(m_Window));
}

void* GLFWWindow::GetNativeHandle() const
{
    return reinterpret_cast<void*>(m_Window);
}

void GLFWWindow::SetCursorMode(ECursorMode mode)
{
    m_CursorMode = mode;
    UpdateCursor();
}

void GLFWWindow::SetCursorVisible()
{
    m_CursorMode = ECursorMode::Visible;
    UpdateCursor();
}

void GLFWWindow::SetCursorHidden()
{
    m_CursorMode = ECursorMode::Hidden;
    UpdateCursor();
}

void GLFWWindow::SetCursorDisabled()
{
    m_CursorMode = ECursorMode::Disabled;
    UpdateCursor();
}

void GLFWWindow::SetFramebufferResizeCallback(FResizeCallback callback)
{
    m_FramebufferResizeCallback = std::move(callback);
}

void GLFWWindow::SetWindowResizeCallback(FResizeCallback callback)
{
    m_WindowResizeCallback = std::move(callback);
}

void GLFWWindow::SetFocusCallback(FFocusCallback callback)
{
    m_FocusCallback = std::move(callback);
}

void GLFWWindow::UpdateCursor()
{
    if (!m_Window) return;

    switch (m_CursorMode)
    {
        case ECursorMode::Visible:
            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            break;
        case ECursorMode::Hidden:
            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
            break;
        case ECursorMode::Disabled:
            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            break;
    }
}
