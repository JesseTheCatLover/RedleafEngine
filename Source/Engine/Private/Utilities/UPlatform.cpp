// Copyright 2026 JesseTheCatLover. All Rights Reserved.

#include "Utilities/UPlatform.h"

#include <cstdlib>
#include <string>

#if JENGINE_PLATFORM_WINDOWS
    #include <windows.h>
    #include <shellapi.h>
#endif

namespace UPlatform
{
    void OpenURL(const char* url)
    {
        if (!url || url[0] == '\0')
            return;

#if JENGINE_PLATFORM_WINDOWS

        ShellExecuteA(
            nullptr,
            "open",
            url,
            nullptr,
            nullptr,
            SW_SHOWNORMAL
        );

#elif JENGINE_PLATFORM_MACOS

        const std::string command =
            "open \"" +
            std::string(url) +
            "\"";

        std::system(command.c_str());

#elif JENGINE_PLATFORM_LINUX

        const std::string command =
            "xdg-open \"" +
            std::string(url) +
            "\"";

        std::system(command.c_str());

#endif
    }
}