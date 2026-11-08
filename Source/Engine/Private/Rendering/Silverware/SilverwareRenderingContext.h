// Copyright 2026 JesseTheCatLover. All Rights Reserved.

#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Rendering/IRenderDevice.h"
#include "Rendering/IRenderTargetProcessor.h"
#include "Silverware/Backend/IRenderingContext.h"

class SilverwareRenderingContext final : public Silverware::IRenderingContext
{
private:
    struct FTextureEntry
    {
        RTextureHandle resource{};
        bool bOwned = false;
        std::uint64_t renderTargetOwner = 0;
    };

    struct FRenderTargetEntry
    {
        RRenderTargetHandle resource{};
        Silverware::FTextureHandle texture{};
        int width = 0;
        int height = 0;
    };

    struct FRenderTargetStackEntry
    {
        std::uint64_t target = 0;
        int previousViewportWidth = 0;
        int previousViewportHeight = 0;
    };

private:
    IRenderDevice& m_RenderDevice;
    IRenderTargetProcessor& m_RenderTargetProcessor;

    std::unordered_map<std::uint64_t, RMeshHandle> m_Geometries;
    std::unordered_map<std::uint64_t, FTextureEntry> m_Textures;

    std::uint64_t m_NextGeometryHandle = 1;
    std::uint64_t m_NextTextureHandle = 1;

    int m_ViewportWidth = 0;
    int m_ViewportHeight = 0;

    std::unordered_map<std::uint64_t, FRenderTargetEntry> m_RenderTargets;
    std::vector<FRenderTargetStackEntry> m_RenderTargetStack;

    std::uint64_t m_NextRenderTargetHandle = 1;

    std::unordered_map<std::uint64_t, Silverware::EScreenSpaceShaderType> m_Shaders;
    std::uint64_t m_NextShaderHandle = 1;

    RShaderHandle m_GradientShader{};

public:
    explicit SilverwareRenderingContext(IRenderDevice& renderDevice, IRenderTargetProcessor& renderTargetProcessor);
    ~SilverwareRenderingContext() override;

    SilverwareRenderingContext(const SilverwareRenderingContext&) = delete;
    SilverwareRenderingContext& operator=(const SilverwareRenderingContext&) = delete;
    SilverwareRenderingContext(SilverwareRenderingContext&&) = delete;
    SilverwareRenderingContext& operator=(SilverwareRenderingContext&&) = delete;

    void SetViewportSize(int width, int height) override;

    [[nodiscard]] Silverware::FGeometryHandle CreateGeometry(
        std::span<const Silverware::FRenderVertex> vertices,
        std::span<const std::uint32_t> indices) override;

    void DestroyGeometry(Silverware::FGeometryHandle geometry) override;

    [[nodiscard]] Silverware::FTextureHandle CreateTexture(int width, int height,
        std::span<const std::byte> rgbaPixels) override;

    void DestroyTexture(Silverware::FTextureHandle texture) override;

    [[nodiscard]] Silverware::FTextureHandle CreateTextureFromRenderTargetRegion(Silverware::FRenderTargetHandle source,
        int x, int y, int width, int height) override;

    [[nodiscard]] Silverware::FTextureHandle CreateExternalTexture(Silverware::FExternalTextureReference texture) override;

    bool UpdateExternalTexture(Silverware::FTextureHandle texture, Silverware::FExternalTextureReference externalTexture) override;

    void RenderGeometry(Silverware::FGeometryHandle geometry, Silverware::FTextureHandle texture,
        float translationX, float translationY, const Silverware::FRenderTransform& transform, bool bWriteColor) override;

    void EnableScissorRegion(bool bEnable) override;

    void SetScissorRegion(int x, int y, int width, int height) override;

    Silverware::FRenderTargetHandle CreateRenderTarget(const Silverware::FRenderTargetDesc& desc) override;

    void DestroyRenderTarget(Silverware::FRenderTargetHandle target) override;

    Silverware::FTextureHandle GetRenderTargetTexture(Silverware::FRenderTargetHandle target) override;

    bool PushRenderTarget(Silverware::FRenderTargetHandle target) override;
    bool PopRenderTarget() override;

    bool ClearRenderTarget(Silverware::FRenderTargetHandle target, const Silverware::FRenderTargetClearDesc &desc) override;

    bool CompositeRenderTargets(Silverware::FRenderTargetHandle source, Silverware::FRenderTargetHandle destination,
        Silverware::ECompositeBlendMode blendMode) override;

    bool ApplyFilter(Silverware::FRenderTargetHandle source, Silverware::FRenderTargetHandle output,
        const Silverware::FRenderTargetFilterDesc &filter, const Silverware::FRenderTargetRegion &region) override;

    void ClearStencilBuffer(std::uint8_t value) override;

    void SetClipMaskStencilState(const Silverware::FClipMaskStencilState &state) override;

    Silverware::FShaderHandle CreateShader(Silverware::EScreenSpaceShaderType type) override;

    void DestroyShader(Silverware::FShaderHandle shader) override;

    void RenderGeometryWithShader(Silverware::FGeometryHandle geometry, Silverware::FShaderHandle shader,
        Silverware::FTextureHandle texture, float translationX, float translationY,
        const Silverware::FRenderTransform &transform, std::span<const Silverware::FShaderUniform> uniforms,
        bool bWriteColor) override;

private:
    RShader BuildGradientShader();
};