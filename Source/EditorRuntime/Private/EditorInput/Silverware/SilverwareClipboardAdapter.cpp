// Copyright 2026 JesseTheCatLover. All Rights Reserved.

#include "EditorInput/Silverware/SilverwareClipboardAdapter.h"

#include "Rendering/IPlatformSurface.h"

SilverwareClipboardAdapter::SilverwareClipboardAdapter(IPlatformSurface& platformSurface)
    : m_PlatformSurface(platformSurface)
{
}

bool SilverwareClipboardAdapter::TryGetText(std::string& outText)
{
    return m_PlatformSurface.TryGetClipboardText(outText);
}

void SilverwareClipboardAdapter::SetText(std::string_view text)
{
    m_PlatformSurface.SetClipboardText(text);
}