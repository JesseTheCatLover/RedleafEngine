// Copyright 2025-2026 JesseTheCatLover. All Rights Reserved.

#pragma once

#include <cstdint>

#include "RRenderTarget.h"
#include "Rendering/RHandles.h"

enum class ERenderTargetFilterType : std::uint8_t
{
    GaussianBlur
};

struct FRenderTargetFilterDesc
{
    ERenderTargetFilterType type{};
    float sigma = 1.0f;
};

struct FRenderTargetRegion
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return x >= 0 && y >= 0 && width > 0 && height > 0;
    }
};

class IRenderTargetProcessor
{
public:
    virtual ~IRenderTargetProcessor() = default;

    [[nodiscard]] virtual bool ApplyFilter(
        RRenderTargetHandle source,
        RRenderTargetHandle output,
        const FRenderTargetFilterDesc& filter,
        const FRenderTargetRegion& region) = 0;
};