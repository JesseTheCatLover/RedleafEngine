// Copyright 2025-2026 JesseTheCatLover. All Rights Reserved.

#pragma once

#include "RRenderOperations.h"
#include "Rendering/RHandles.h"
#include "Core/Math/FVector3.h"
#include "Rendering/EProjectionType.h"
#include "Rendering/FProjectionDesc.h"

struct RFramebuffer;
struct RTexture;
struct RMesh;
struct RShader;
// This describes how the backend's axes relate to ENGINE axes.
// Engine coordinate system : X=Forward, Y=Right, Z=Up
struct FBackendCoordDesc
{
    // Backend basis vectors expressed in ENGINE space.
    // Example: if backend X is engine Right, then X = (0, 1, 0).
    FVector3 X; // backend +X in engine coordinates
    FVector3 Y; // backend +Y in engine coordinates
    FVector3 Z; // backend +Z in engine coordinates

    // Clip-space / depth flags
    bool depthZeroToOne = false; // true for D3D/Vulkan, false for OpenGL
    bool clipSpaceYUp = true; // etc. if we care about NDC orientation
};

class IPlatformSurface;
struct FMatrix4;
struct RLightData;
struct FDebugClipVertex;
struct FDebugVertex;
struct FDebugWorldVertex;

class IRenderBackend
{
public:
    enum class EResolveMask   : uint8_t { Color = 1, Depth = 2, Stencil = 4, ColorDepth = 3, All = 7 };
    enum class EResolveFilter : uint8_t { Nearest, Linear };

    enum class ECompareFunc   : uint8_t { Never, Less, LessEqual, Equal, Greater, GreaterEqual, NotEqual, Always };
    enum class EBlendFactor   : uint8_t { Zero, One, SrcColor, OneMinusSrcColor, DstColor, OneMinusDstColor,
                                          SrcAlpha, OneMinusSrcAlpha, DstAlpha, OneMinusDstAlpha };
    enum class EStencilOp     : uint8_t { Keep, Zero, Replace, Incr, IncrWrap, Decr, DecrWrap, Invert };
    enum class ECullMode      : uint8_t { None, Back, Front };

    struct FStencilState
    {
        bool bEnable = false;

        // Comparison
        ECompareFunc func = ECompareFunc::Always;
        uint8_t ref = 0;
        uint8_t readMask = 0xFF;

        // Writes
        uint8_t writeMask = 0xFF;

        // Ops: what happens to the stencil value
        EStencilOp sfail = EStencilOp::Keep;  // stencil test fails
        EStencilOp zfail = EStencilOp::Keep;  // stencil passes, depth fails
        EStencilOp zpass = EStencilOp::Keep;  // both pass
    };

    static constexpr uint32_t kMaxLights = 128;

    virtual ~IRenderBackend() = default;

    [[nodiscard]] virtual FBackendCoordDesc GetCoordConvention() const = 0;
    [[nodiscard]] virtual FMatrix4 BuildProjectionMatrix(const FProjectionDesc& desc) const = 0;

    // Lifecycle
    virtual bool Initialize(IPlatformSurface* surface) = 0;
    virtual void Shutdown() = 0;

    // Frame control
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void SetViewport(int x, int y, int w, int h) = 0;
    virtual void ClearColorDepth(float r, float g, float b, float a, bool bClearDepth = true) = 0;
    virtual void ClearDepthOnly(bool bClearDepth = true) = 0;
    virtual void ClearStencilBuffer(int value) = 0;
    virtual void ClearDepthStencil(float depth, int stencil) = 0;
    virtual void ClearColorDepthStencil(float r, float g, float b, float a, float depth, int stencil) = 0;

    // State control
    virtual void SetDepthState(bool bTestEnable, bool bWriteEnable, ECompareFunc func) = 0;
    virtual void SetBlendState(bool bEnable, EBlendFactor src, EBlendFactor dst) = 0;
    virtual void SetCullMode(ECullMode mode) = 0;

    // Resources
    virtual RMeshHandle CreateMesh(const RMesh& meshData) = 0;
    virtual void DestroyMesh(RMeshHandle handle) = 0;

    virtual RTextureHandle CreateTexture(const RTexture& textureData) = 0;
    virtual void DestroyTexture(RTextureHandle handle) = 0;
    virtual void BindTexture(RTextureHandle texture, uint32_t slot) = 0;

    virtual RShaderHandle CreateShader(const RShader& shaderData) = 0;
    virtual void DestroyShader(RShaderHandle handle) = 0;
    virtual void BindShader(RShaderHandle shader) = 0;

    virtual RFramebufferHandle CreateFramebuffer(const RFramebuffer& framebufferData) = 0;
    virtual void DestroyFramebuffer(RFramebufferHandle handle) = 0;
    virtual void BindFramebuffer(RFramebufferHandle handle) = 0;
    virtual void UnbindFramebuffer() = 0;
    virtual void ResolveFramebuffer(RFramebufferHandle src, RFramebufferHandle dst,
                                EResolveMask mask = EResolveMask::Color,
                                EResolveFilter filter = EResolveFilter::Nearest) = 0;

    virtual bool PushFramebuffer(RFramebufferHandle handle) = 0;
    virtual bool PopFramebuffer() = 0;

    virtual void ClearFramebuffer(const FRenderClearDesc& desc) = 0;

    virtual void SetStencilState(const FStencilState& state) = 0;

    virtual void SetColorWriteMask(bool r, bool g, bool b, bool a) = 0;

    virtual RTextureHandle GetFramebufferColorTexture(RFramebufferHandle) = 0;
    virtual RTextureHandle GetFramebufferDepthTexture(RFramebufferHandle) = 0;

    [[nodiscard]] virtual void* GetNativeTextureHandle(RTextureHandle handle) const = 0;

    virtual void SubmitDebugLineList(RShaderHandle shader, const FDebugVertex* verts, uint32_t vertCount) = 0;

    virtual void SubmitDebugClipTriList(RShaderHandle shader, const FDebugClipVertex* verts, uint32_t vertCount) = 0;

    virtual void SubmitDebugWorldTriList(RShaderHandle shader, const FDebugWorldVertex* verts, uint32_t vertCount) = 0;

    // Rendering
    virtual void SubmitMesh(RMeshHandle mesh, RShaderHandle shader, const FMatrix4& transform) = 0;

    virtual void UploadLights(const RLightData* lights, uint32_t count) = 0;

    // Scissor
    virtual void EnableScissorRegion(bool bEnable) = 0;
    virtual void SetScissorRegion(int x, int y, int width, int height) = 0;

    // --- Uniforms (scalars/vectors/matrices) ---
    virtual void SetUniformInt  (RShaderHandle sh, const char* name, int v) = 0;
    virtual void SetUniformFloat(RShaderHandle sh, const char* name, float v) = 0;
    virtual void SetUniformVec2 (RShaderHandle sh, const char* name, const float* v2) = 0;
    virtual void SetUniformVec3 (RShaderHandle sh, const char* name, const float* v3) = 0;
    virtual void SetUniformVec4 (RShaderHandle sh, const char* name, const float* v4) = 0;
    virtual void SetUniformMat4 (RShaderHandle sh, const char* name, const float* mat4) = 0;

    virtual void LinkUniformBlock(RShaderHandle sh, const char* blockName, uint32_t bindingPoint) = 0;
};