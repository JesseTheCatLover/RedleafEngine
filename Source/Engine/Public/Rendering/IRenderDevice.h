//  Copyright 2025-2026 JesseTheCatLover. All Rights Reserved.

#pragma once
#include <functional>
#include <cstdint>
#include <array>
#include <span>
#include <string>
#include <variant>

#include "RRenderOperations.h"
#include "RRenderTarget.h"
#include "Rendering/RHandles.h"

struct FDebugTri;
struct FDebugClipVertex;
struct FDebugLine;
struct FDebugVertex;
class IRenderSubmission;
struct FSurfaceDesc;
struct RShader;
struct RTexture;
struct RMesh;
struct FRenderView;

enum class EClipMaskStencilOperation : std::uint8_t
{
    Keep,
    Replace,
    Increment
};

struct FClipMaskStencilState
{
    bool bEnable = false;
    bool bWrite = false;
    std::uint8_t reference = 0;
    EClipMaskStencilOperation operation = EClipMaskStencilOperation::Keep;
};

using FScreenSpaceShaderUniformValue = std::variant<
    bool,
    int,
    float,
    std::array<float, 2>,
    std::array<float, 3>,
    std::array<float, 4>,
    std::array<float, 16>
>;

struct FScreenSpaceShaderUniform
{
    std::string name;
    FScreenSpaceShaderUniformValue value;
};

class IRenderDevice
{
public:
    virtual ~IRenderDevice();

    virtual RMeshHandle CreateMesh (const RMesh& data) = 0;
    virtual void DestroyMesh (RMeshHandle h) = 0;

    virtual RTextureHandle CreateTexture (const RTexture& data) = 0;
    virtual void DestroyTexture(RTextureHandle h) = 0;

    [[nodiscard]] virtual RTextureHandle CreateTextureFromRenderTargetRegion(
    RRenderTargetHandle source,
    int x,
    int y,
    int width,
    int height) = 0;

    // Render targets
    virtual RRenderTargetHandle CreateRenderTarget(const FRenderTargetDesc& desc) = 0;
    virtual void DestroyRenderTarget(RRenderTargetHandle target) = 0;

    [[nodiscard]] virtual RTextureHandle GetRenderTargetColorTexture(RRenderTargetHandle target) const = 0;

    virtual bool PushRenderTarget(RRenderTargetHandle target) = 0;
    virtual bool PopRenderTarget() = 0;

    virtual bool ClearRenderTarget(RRenderTargetHandle target, const FRenderClearDesc& desc) = 0;

    virtual bool CompositeRenderTargets(
    RRenderTargetHandle source,
    RRenderTargetHandle destination,
    ERenderTargetBlendMode blendMode) = 0;

    virtual RShaderHandle CreateShader  (const RShader& data) = 0;
    virtual void DestroyShader (RShaderHandle h) = 0;

    // Material
    virtual RMaterialHandle CreateMaterial(const FSurfaceDesc& surface) = 0;
    virtual void DestroyMaterial(RMaterialHandle h) = 0;

    [[nodiscard]] virtual RTextureHandle GetSceneColorTarget() const = 0;

    virtual void SubmitDebugLines(const FRenderView& view, const FDebugLine* lines, uint32_t lineCount) = 0;
    virtual void SubmitDebugTriangles(const FRenderView& view, const FDebugTri* tris, uint32_t triCount) = 0;

    virtual void SubmitScreenSpaceGeometry(RMeshHandle mesh, RTextureHandle texture,
    float translationX, float translationY,
    int viewportWidth, int viewportHeight, bool bWriteColor = true, const float* transform = nullptr) = 0;

    virtual void SubmitScreenSpaceGeometryWithShader(
    RMeshHandle mesh, RShaderHandle shader,
    RTextureHandle texture,
    float translationX, float translationY,
    int viewportWidth, int viewportHeight,
    bool bWriteColor,
    const float* transform,
    std::span<const FScreenSpaceShaderUniform> uniforms) = 0;

    virtual void EnableScissorRegion(bool bEnable) = 0;
    virtual void SetScissorRegion(int x, int y, int width, int height) = 0;

    virtual void ClearStencilBuffer(std::uint8_t value) = 0;

    virtual void SetClipMaskStencilState(const FClipMaskStencilState& state) = 0;

    // virtual RMaterialHandle CreateMaterial(const RMaterialDesc& d) = 0;
    // Queue a lambda to run on the render thread.
    virtual void EnqueueRenderTask(std::function<void()> fn) = 0;
};

inline IRenderDevice::~IRenderDevice() = default;
