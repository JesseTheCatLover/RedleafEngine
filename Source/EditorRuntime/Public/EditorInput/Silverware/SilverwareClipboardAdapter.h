// Copyright 2026 JesseTheCatLover. All Rights Reserved.

#pragma once

#include "Silverware/Core/Platform/IClipboard.h"

class IPlatformSurface;

class SilverwareClipboardAdapter final : public Silverware::IClipboard
{
private:
    IPlatformSurface& m_PlatformSurface;

public:
    explicit SilverwareClipboardAdapter(IPlatformSurface& platformSurface);

    bool TryGetText(std::string& outText) override;
    void SetText(std::string_view text) override;
};