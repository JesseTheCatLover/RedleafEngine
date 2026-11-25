//  Copyright 2025-2026 JesseTheCatLover. All Rights Reserved.

#pragma once
#include <string>
#include <vector>

#include "Core/Memory/SmartPointers.h"

struct FWindowDesc;
class IPlatformWindow;
class InputManager;
enum class ECursorMode;
class IPlatformSurface;
class EngineContext;

class EditorSurfaceAPI
{
private:
    EngineContext& m_Context;
    IPlatformSurface& m_PlatformSurface;
    InputManager& m_InputManager;

public:
    EditorSurfaceAPI(EngineContext& ctx, IPlatformSurface& surface, InputManager& inputManager);

    [[nodiscard]] IPlatformSurface& GetPlatformSurface() const
    {
        return m_PlatformSurface;
    }

    // File dialogs
    std::string OpenFileDialog(const char* filterList, const char* defaultPath);
    std::vector<std::string> OpenFileDialogMultiple(const char* filterList, const char* defaultPath);
    std::string OpenFolderDialog(const char* defaultPath);
    std::string SaveFileDialog(const char* filterList,
                               const char* defaultPath,
                               const char* defaultName);

    // Windows
    TSharedPtr<IPlatformWindow> CreateWindow(const FWindowDesc& desc);
    void DestroyWindow(const TSharedPtr<IPlatformWindow>& window);

    // Cursor
    void SetCursorDisabled();
    void SetCursorHidden();
    void SetCursorVisible();

    InputManager& GetInputManager() const { return m_InputManager; }
};
