// Copyright 2025-2026 JesseTheCatLover. All Rights Reserved.

#pragma once

#include "Rendering/RHandles.h"

#include <cstdint>

struct RRenderTargetHandle
{
    Rint id = 0;

    [[nodiscard]] bool IsValid() const noexcept { return id != 0; }

    [[nodiscard]] static constexpr RRenderTargetHandle Invalid() noexcept
    {
        return {};
    }

    friend constexpr bool operator==(
        const RRenderTargetHandle&,
        const RRenderTargetHandle&) = default;
};

enum class ERenderTargetColorFormat : std::uint8_t
{
    RGBA8,
    RGBA16F,
    SRGBA8
};

struct FRenderTargetDesc
{
    int width = 0;
    int height = 0;

    ERenderTargetColorFormat colorFormat = ERenderTargetColorFormat::RGBA8;

    int sampleCount = 1;
    bool bHasDepthStencil = false;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return width > 0 && height > 0 && sampleCount > 0;
    }
};