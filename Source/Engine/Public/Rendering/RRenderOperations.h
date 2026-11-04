// Copyright 2025-2026 JesseTheCatLover. All Rights Reserved.

#pragma once

#include <cstdint>

struct FRenderClearDesc
{
    bool bClearColor = false;
    float color[4] = { 0.f, 0.f, 0.f, 0.f };

    bool bClearDepth = false;
    float depth = 1.f;

    bool bClearStencil = false;
    std::uint8_t stencil = 0;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return bClearColor || bClearDepth || bClearStencil;
    }
};

enum class ERenderTargetBlendMode : std::uint8_t
{
    Blend,
    Replace
};