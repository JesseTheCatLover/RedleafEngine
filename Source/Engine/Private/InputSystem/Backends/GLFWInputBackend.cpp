//  Copyright 2025 JesseTheCatLover. All Rights Reserved.

#include "GLFWInputBackend.h"
#include <cmath>
#include "GLFWInputMapping.h"
#include "Rendering/IPlatformWindow.h"

namespace
{
    std::unordered_map<GLFWwindow*, GLFWInputBackend*> s_WindowToBackend;

    FRawInputModifiers MakeModifiers(int mods)
    {
        FRawInputModifiers modifiers{};

        modifiers.shift = (mods & GLFW_MOD_SHIFT) != 0;
        modifiers.control = (mods & GLFW_MOD_CONTROL) != 0;
        modifiers.alt = (mods & GLFW_MOD_ALT) != 0;
        modifiers.super = (mods & GLFW_MOD_SUPER) != 0;

#ifdef GLFW_MOD_CAPS_LOCK
        modifiers.capsLock = (mods & GLFW_MOD_CAPS_LOCK) != 0;
#endif

#ifdef GLFW_MOD_NUM_LOCK
        modifiers.numLock = (mods & GLFW_MOD_NUM_LOCK) != 0;
#endif

        return modifiers;
    }

    bool IsGlfwKeyDown(GLFWwindow* window, int key)
    {
        return glfwGetKey(window, key) == GLFW_PRESS;
    }

    FRawInputModifiers GetCurrentModifiers(GLFWwindow* window)
    {
        FRawInputModifiers modifiers{};

        modifiers.shift = IsGlfwKeyDown(window, GLFW_KEY_LEFT_SHIFT) ||
                          IsGlfwKeyDown(window, GLFW_KEY_RIGHT_SHIFT);

        modifiers.control = IsGlfwKeyDown(window, GLFW_KEY_LEFT_CONTROL) ||
                            IsGlfwKeyDown(window, GLFW_KEY_RIGHT_CONTROL);

        modifiers.alt = IsGlfwKeyDown(window, GLFW_KEY_LEFT_ALT) ||
                        IsGlfwKeyDown(window, GLFW_KEY_RIGHT_ALT);

        modifiers.super = IsGlfwKeyDown(window, GLFW_KEY_LEFT_SUPER) ||
                          IsGlfwKeyDown(window, GLFW_KEY_RIGHT_SUPER);

        return modifiers;
    }
}

GLFWInputBackend::GLFWInputBackend(GLFWwindow* window)
    : m_Window(window)
{
    // Register this backend for this window
    s_WindowToBackend[m_Window] = this;

    // Register GLFW callbacks
    glfwSetKeyCallback (m_Window, &GLFWInputBackend::KeyCallback);
    glfwSetMouseButtonCallback(m_Window, &GLFWInputBackend::MouseButtonCallback);
    glfwSetScrollCallback (m_Window, &GLFWInputBackend::ScrollCallback);
    glfwSetCursorPosCallback (m_Window, &GLFWInputBackend::CursorPosCallback);
    glfwSetCharCallback(m_Window, &GLFWInputBackend::CharCallback);

    // Initialize last mouse position
    glfwGetCursorPos(m_Window, &m_LastMouseX, &m_LastMouseY);
}

GLFWInputBackend::~GLFWInputBackend()
{
    s_WindowToBackend.erase(m_Window);
}

void GLFWInputBackend::SetTargetWindow(IPlatformWindow* platformWindow)
{
    // 1. If nullptr was passed, untangle our window reference and exit early
    if (!platformWindow)
    {
        m_Window = nullptr;
        return;
    }

    // 2. Extract the raw native handle safely inside the GLFW implementation boundary
    GLFWwindow* newRawWindow = static_cast<GLFWwindow*>(platformWindow->GetNativeHandle());
    if (!newRawWindow)
    {
        return;
    }

    m_Window = newRawWindow;

    // 3. Register tracking state so our static callbacks route events to this backend instance
    s_WindowToBackend[m_Window] = this;

    // 4. Update core viewport position metrics and flush old events safely
    glfwGetCursorPos(m_Window, &m_LastMouseX, &m_LastMouseY);
    m_PendingEvents.clear();
}

void GLFWInputBackend::FetchEvents(std::vector<FRawInputEvent>& outEvents)
{
    // glfwPollEvents() is called from the GLFWSurface.

    // Poll gamepads every frame (no callbacks in GLFW for them)
    PollGamepads();

    // Move pending events out
    outEvents = std::move(m_PendingEvents);
    m_PendingEvents.clear();
}

void GLFWInputBackend::GetMousePosition(float& outX, float& outY)
{
    if (!m_Window)
    {
        outX = outY = 0.0f;
        return;
    }
    double x, y;
    glfwGetCursorPos(m_Window, &x, &y);
    outX = static_cast<float>(x);
    outY = static_cast<float>(y);
}

// ---------------------- Static helpers ----------------------

GLFWInputBackend* GLFWInputBackend::GetBackend(GLFWwindow* window)
{
    auto it = s_WindowToBackend.find(window);
    if (it == s_WindowToBackend.end())
        return nullptr;
    return it->second;
}

void GLFWInputBackend::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    GLFWInputBackend* backend = GetBackend(window);
    if (!backend)
        return;

    backend->OnKey(key, scancode, action, mods);
}

void GLFWInputBackend::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    GLFWInputBackend* backend = GetBackend(window);
    if (!backend)
        return;

    backend->OnMouseButton(button, action, mods);
}

void GLFWInputBackend::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    GLFWInputBackend* backend = GetBackend(window);
    if (!backend)
        return;

    backend->OnScroll(xoffset, yoffset);
}

void GLFWInputBackend::CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    GLFWInputBackend* backend = GetBackend(window);
    if (!backend)
        return;

    backend->OnCursorPos(xpos, ypos);
}

void GLFWInputBackend::CharCallback(GLFWwindow* window, unsigned int codepoint)
{
    GLFWInputBackend* backend = GetBackend(window);
    if (!backend)
        return;

    backend->OnChar(codepoint);
}

// ---------------------- Instance handlers ----------------------

void GLFWInputBackend::OnKey(int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_UNKNOWN)
        return;

    EPhysicalInput phys = MapGlfwKeyToEngine(key);
    if (phys == EPhysicalInput::Unknown)
        return;

    FRawInputEvent ev{};
    ev.deviceID = 0;
    ev.code = static_cast<uint32_t>(phys);
    ev.timestamp = glfwGetTime();
    ev.modifiers = MakeModifiers(mods);

    if (action == GLFW_PRESS || action == GLFW_REPEAT)
    {
        ev.type = ERawInputType::KeyDown;
        ev.value = 1.0f;
        ev.isRepeat = action == GLFW_REPEAT;
    }
    else if (action == GLFW_RELEASE)
    {
        ev.type = ERawInputType::KeyUp;
        ev.value = 0.0f;
    }
    else
        return;

    m_PendingEvents.push_back(ev);
}

void GLFWInputBackend::OnMouseButton(int button, int action, int mods)
{
    EPhysicalInput phys = MapGlfwMouseButtonToEngine(button);
    if (phys == EPhysicalInput::Unknown)
        return;

    FRawInputEvent ev{};
    ev.deviceID = 0;
    ev.code = static_cast<uint32_t>(phys);
    ev.timestamp = glfwGetTime();
    ev.modifiers = MakeModifiers(mods);

    double x, y;
    glfwGetCursorPos(m_Window, &x, &y);

    ev.positionX = static_cast<float>(x);
    ev.positionY = static_cast<float>(y);

    if (action == GLFW_PRESS)
    {
        ev.type = ERawInputType::MouseButtonDown;
        ev.value = 1.0f;
    }
    else if (action == GLFW_RELEASE)
    {
        ev.type = ERawInputType::MouseButtonUp;
        ev.value = 0.0f;
    }
    else
        return;

    m_PendingEvents.push_back(ev);
}

void GLFWInputBackend::OnScroll(double xoffset, double yoffset)
{
    if (xoffset == 0.0 && yoffset == 0.0)
        return;

    FRawInputEvent ev{};
    ev.deviceID = 0;
    ev.type = ERawInputType::MouseWheel;
    ev.timestamp = glfwGetTime();
    ev.scrollX = static_cast<float>(xoffset);
    ev.scrollY = static_cast<float>(yoffset);
    ev.modifiers = GetCurrentModifiers(m_Window);

    double x, y;
    glfwGetCursorPos(m_Window, &x, &y);

    ev.positionX = static_cast<float>(x);
    ev.positionY = static_cast<float>(y);

    m_PendingEvents.push_back(ev);
}

void GLFWInputBackend::OnCursorPos(double xpos, double ypos)
{
    const double dx = xpos - m_LastMouseX;
    const double dy = ypos - m_LastMouseY;

    m_LastMouseX = xpos;
    m_LastMouseY = ypos;

    if (dx == 0.0 && dy == 0.0)
        return;

    FRawInputEvent ev{};
    ev.deviceID = 0;
    ev.type = ERawInputType::MouseMove;
    ev.timestamp = glfwGetTime();

    ev.positionX = static_cast<float>(xpos);
    ev.positionY = static_cast<float>(ypos);
    ev.deltaX = static_cast<float>(dx);
    ev.deltaY = static_cast<float>(dy);
    ev.modifiers = GetCurrentModifiers(m_Window);

    m_PendingEvents.push_back(ev);
}

void GLFWInputBackend::OnChar(unsigned int codepoint)
{
    FRawInputEvent ev{};
    ev.deviceID = 0;
    ev.type = ERawInputType::TextInput;
    ev.timestamp = glfwGetTime();
    ev.code = static_cast<uint32_t>(codepoint);
    ev.modifiers = GetCurrentModifiers(m_Window);

    m_PendingEvents.push_back(ev);
}

// ---------------------- Gamepad polling ----------------------

void GLFWInputBackend::PollGamepads()
{
    const double time = glfwGetTime();
    constexpr float AxisEpsilon = 0.001f;

    for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid)
    {
        const bool bIsNowConnect = (glfwJoystickIsGamepad(jid) == GLFW_TRUE);
        bool& bWasConnected = m_GamepadConnected[jid];

        // ---------------- DISCONNECTED THIS FRAME ----------------
        if (!bIsNowConnect && bWasConnected)
        {
            // 1) Mark as disconnected
            bWasConnected = false;

            // 2) Clear previous state so we don't use stale data
            m_PrevGamepadButtons[jid].clear();
            m_PrevGamepadAxes[jid].clear();

            // 3) TODO: future, explicit events, extend ERawInputType with
            // GamepadConnected / GamepadDisconnected and emit one here.
            //
            // FRawInputEvent ev{};
            // ev.deviceID  = jid;
            // ev.type      = ERawInputType::GamepadDisconnected;
            // ev.code      = 0;
            // ev.value     = 0.0f;
            // ev.timestamp = time;
            // m_PendingEvents.push_back(ev);

            continue; // nothing more to do for this jid
        }

        // ---------------- NOT A GAMEPAD / NEVER WAS ----------------
        if (!bIsNowConnect && !bWasConnected)
        {
            // still not a gamepad, skip
            continue;
        }

        // ---------------- NEWLY CONNECTED THIS FRAME ----------------
        if (bIsNowConnect && !bWasConnected)
        {
            bWasConnected = true;

            // Optionally emit a "connected" event if you add it to ERawInputType.
            //
            // FRawInputEvent ev{};
            // ev.deviceID  = jid;
            // ev.type      = ERawInputType::GamepadConnected;
            // ev.code      = 0;
            // ev.value     = 0.0f;
            // ev.timestamp = time;
            // m_PendingEvents.push_back(ev);
        }

        // If we reach here, we know: bIsNowConnect == true and bWasConnected == true (currently connected)

        GLFWgamepadstate state{};
        if (glfwGetGamepadState(jid, &state) != GLFW_TRUE)
            continue;

        const int deviceID = jid; // This will be used as FRawInputEvent::deviceID

        // --- Buttons ---
        auto& prevButtons = m_PrevGamepadButtons[jid];
        if (prevButtons.size() != GLFW_GAMEPAD_BUTTON_LAST + 1)
            prevButtons.assign(GLFW_GAMEPAD_BUTTON_LAST + 1, GLFW_RELEASE);

        for (int b = 0; b <= GLFW_GAMEPAD_BUTTON_LAST; ++b)
        {
            unsigned char prev = prevButtons[b];
            unsigned char curr = state.buttons[b];

            if (curr == prev)
                continue;

            EPhysicalInput phys = MapGlfwGamepadButtonToEngine(b);
            if (phys == EPhysicalInput::Unknown)
                continue;

            FRawInputEvent ev{};
            ev.deviceID  = deviceID;
            ev.code      = static_cast<uint32_t>(phys);
            ev.timestamp = time;

            if (curr == GLFW_PRESS)
            {
                ev.type  = ERawInputType::GamepadButtonDown;
                ev.value = 1.0f;
            }
            else // GLFW_RELEASE
            {
                ev.type  = ERawInputType::GamepadButtonUp;
                ev.value = 0.0f;
            }

            m_PendingEvents.push_back(ev);
            prevButtons[b] = curr;
        }

        // --- Axes ---
        auto& prevAxes = m_PrevGamepadAxes[jid];
        if (prevAxes.size() != GLFW_GAMEPAD_AXIS_LAST + 1)
            prevAxes.assign(GLFW_GAMEPAD_AXIS_LAST + 1, 0.0f);

        for (int a = 0; a <= GLFW_GAMEPAD_AXIS_LAST; ++a)
        {
            float prev = prevAxes[a];
            float curr = state.axes[a];

            if (std::abs(curr - prev) < AxisEpsilon)
                continue;

            EPhysicalInput phys = MapGlfwGamepadAxisToEngine(a);
            if (phys == EPhysicalInput::Unknown)
                continue;

            FRawInputEvent ev{};
            ev.deviceID  = deviceID;
            ev.type      = ERawInputType::GamepadAxis;
            ev.code      = static_cast<uint32_t>(phys);
            ev.value     = curr;
            ev.timestamp = time;

            m_PendingEvents.push_back(ev);
            prevAxes[a] = curr;
        }
    }
}