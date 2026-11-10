// Copyright 2025-2026 JesseTheCatLover. All Rights Reserved.

#pragma once
#include <unordered_map>
#include <vector>

#include "FSurfaceDesc.h"
#include "Rendering/IRenderDevice.h"
#include "Rendering/RHandles.h"
#include "RCommandBuffer.h"
#include "Rendering/FProjectionDesc.h"
#include "Rendering/IRenderTargetProcessor.h"

struct FProjectionDesc;
struct FDebugWorldVertex;
struct FFramePostParams;
class JScene;
struct FRenderView;
class EngineContext;
struct FBackendCoordDesc;

struct FCoordAdapter
{
    ///< Use before sending transforms to the render backend
    FMatrix4 EngineToBackend;
    ///< Use when interpreting backend-space results in engine space
    FMatrix4 BackendToEngine;
};

struct FSceneBatch
{
    JScene* scene = nullptr;
    std::vector<const FRenderView*> views;
};

struct FPassParam;
class PostProcessManager;
struct RRenderProxy;
class IRenderBackend;

class RendererSubsystem : public IRenderDevice, public IRenderTargetProcessor
{
private:
    friend class JEngine;

private:
    explicit RendererSubsystem(IRenderBackend* backend, EngineContext& ctx);

    void RenderFrame(const std::vector<FRenderView> &views);

    void Shutdown();

    EngineContext& m_Context;
    IRenderBackend* m_Backend = nullptr;
    PostProcessManager* m_PPM = nullptr;

    FGPUStateCache m_GPUStateCache;

    RShaderHandle m_DefaultShader{};
    void BuildDefaultShader();
    RShaderHandle m_CustomDepthShader{};
    void EnsureCustomDepthShader();
    RShaderHandle m_OutlineShader{};
    void EnsureOutlineShader();
    RShaderHandle m_FXAAShader{}; // TODO: These messy stuff need to be refactored for the future
    void EnsureFXAAShader();

    RShaderHandle m_UIGeometryShader{};
    void EnsureUIGeometryShader();

    RShaderHandle m_GaussianBlurShader{};
    void EnsureGaussianBlurShader();

    RRenderTargetHandle m_FilterScratchTarget{};
    bool EnsureFilterScratchTarget(int width, int height);

    RShaderHandle m_DebugLineShader{};
    void EnsureDebugLineShader();

    RShaderHandle m_DebugClipTriShader{};
    void EnsureDebugClipTriShader();

    RShaderHandle m_DebugWorldTriShader;
    void EnsureDebugWorldTriShader();

    FCoordAdapter m_CoordAdaptor;

    // Cached per-view matrices for debug + thick lines projection.
    // Recomputed once per view in RenderSceneBatch.
    struct FViewCache
    {
        FMatrix4 viewEngine;
        FProjectionDesc projDesc;

        FMatrix4 viewBackend;
        FMatrix4 projBackend;
        FMatrix4 vpBackend; // projBackend * viewBackend

        int vpW = 0;
        int vpH = 0;
        uint32_t viewIndex = 0;
    };

    FViewCache m_ViewCache;

private:
    struct FMaterialEntry
    {
        FSurfaceDesc surface;
    };

    std::unordered_map<Rint, FMaterialEntry> m_Materials{};
    Rint m_NextMaterialID = 1;

    struct FTarget
    {
        RFramebufferHandle fbo{};
        RFramebufferHandle resolveFbo{};

        RTextureHandle color{};
        RTextureHandle depth{};

        int w = 0;
        int h = 0;
        int samples = 1;

        bool bHasDepthStencil = false;
    };

    // Scene targets
    FTarget m_SceneMSAA{};
    FTarget m_Scene{};
    // Ping-Pong for post
    FTarget m_Ping{};
    FTarget m_Pong{};
    // Custom target
    FTarget m_Custom; // holds: fbo, color(id), depth(customDepth)

    std::unordered_map<Rint, FTarget> m_RenderTargets{};
    std::vector<RRenderTargetHandle> m_RenderTargetStack{};
    Rint m_NextRenderTargetId = 1;

    // Command buffer for scene-gather stage
    RCommandBuffer m_SceneCmd;

    // Fullscreen quad/tri
    RMeshHandle   m_FSQuad{};
    RShaderHandle m_LinearCopyShader; // raw linear copy
    RShaderHandle m_PresentShader;    // tone map + gamma

    // Pass kernels
    struct FPassKernel
    {
        RShaderHandle shader{};
        // function that sets uniforms from FPassParam
        void (*BindParams)(IRenderBackend*, RShaderHandle, const FPassParam&) = nullptr;
    };

    std::unordered_map<std::string, FPassKernel> m_Kernels;

    void EnsureTargets(int w, int h, int samples);
    void DestroyTarget(FTarget& t);
    void BuildTarget(FTarget& t, int w, int h, int samples, bool withDepth = false, bool hdr = false, bool bSrgb = false);
    void BuildCustomTarget(FTarget& t, int w, int h);
    RTextureHandle RunPostProcessChain(RTextureHandle sceneColor, int w, int h, uint32_t profileId,
        const FFramePostParams& frameParams);
    void RenderSceneBatch(const FSceneBatch& batch);
    void EnsureFullscreenQuad();
    void BlitFullscreen(RShaderHandle sh, RTextureHandle inputTex, int w, int h);
    void RebuildKernelsIfDirty(uint32_t profileId);
    void DrawCommandBuffer(RCommandBuffer& buffer, const FMatrix4& viewMat, const FProjectionDesc& projDesc);
    void DrawCustomDepthPass(const RCommandBuffer& cmd, const FMatrix4& viewMat, const FProjectionDesc& projDesc);
    void DrawSceneStencilMaskPass(const RCommandBuffer& cmd, const FMatrix4& viewMat, const FProjectionDesc& projDesc);

    [[nodiscard]] bool DrawFullscreenTexture(RTextureHandle texture);

    FCoordAdapter BuildCoordAdapter(const FBackendCoordDesc& d);

    void ApplyCamera(const RShaderHandle& shaderToUse, const FMatrix4& viewEngine, const FProjectionDesc& projDesc);
    void DrawMesh(const RMeshHandle& meshHandle, const RShaderHandle& shaderToUse, const FMatrix4& modelEngine);

    void SubmitDebugLineList_Internal(const FDebugVertex* verts, uint32_t vertCount, bool bDepthTest);
    void SubmitDebugClipTriList_Internal(const FRenderView& view, const FDebugClipVertex* verts, uint32_t vertCount,
                                               bool bDepthTest);
    void SubmitDebugWorldTriList_Internal(const FRenderView& view, const FDebugWorldVertex* verts, uint32_t vertCount,
                                                         bool bDepthTest);

public:
    ~RendererSubsystem() = default;

    void SetPostProcessManager(PostProcessManager* ppm) { m_PPM = ppm;}

    [[nodiscard]] RTextureHandle GetSceneColorTarget() const override { return m_Scene.color; }

    [[nodiscard]] void* GetNativeTextureHandle(RTextureHandle handle) const;

    void SubmitDebugTriangles(const FRenderView &view, const FDebugTri *tris, uint32_t triCount) override;
    void SubmitDebugLines(const FRenderView &view, const FDebugLine *lines, uint32_t lineCount) override;

    RMeshHandle CreateMesh(const RMesh &data) override;
    void DestroyMesh(RMeshHandle h) override;

    RTextureHandle CreateTexture(const RTexture &data) override;
    void DestroyTexture(RTextureHandle h) override;

    [[nodiscard]] RTextureHandle CreateTextureFromRenderTargetRegion(RRenderTargetHandle source, int x, int y,
        int width, int height) override;

    RRenderTargetHandle CreateRenderTarget(const FRenderTargetDesc& desc) override;
    void DestroyRenderTarget(RRenderTargetHandle target) override;

    [[nodiscard]] RTextureHandle GetRenderTargetColorTexture(RRenderTargetHandle target) const override;

    bool PushRenderTarget(RRenderTargetHandle target) override;
    bool PopRenderTarget() override;

    bool ClearRenderTarget(RRenderTargetHandle target, const FRenderClearDesc &desc) override;

    bool CompositeRenderTargets(RRenderTargetHandle source, RRenderTargetHandle destination,
        ERenderTargetBlendMode blendMode) override;

    [[nodiscard]] bool ApplyFilter(
    RRenderTargetHandle source,
    RRenderTargetHandle output,
    const FRenderTargetFilterDesc& filter,
    const FRenderTargetRegion& region) override;

    void SubmitScreenSpaceGeometry(RMeshHandle mesh, RTextureHandle texture, float translationX, float translationY,
                                   int viewportWidth, int viewportHeight, bool bWriteColor,
                                   const float* transform = nullptr) override;

    void SubmitScreenSpaceGeometryWithShader(
    RMeshHandle mesh,RShaderHandle shader,
    RTextureHandle texture,
    float translationX, float translationY,
    int viewportWidth, int viewportHeight,
    bool bWriteColor,
    const float* transform,
    std::span<const FScreenSpaceShaderUniform> uniforms) override;

    void EnableScissorRegion(bool bEnable) override;

    void SetScissorRegion(int x, int y, int width, int height) override;

    void ClearStencilBuffer(std::uint8_t value) override;

    void SetClipMaskStencilState(const FClipMaskStencilState& state) override;

    RShaderHandle CreateShader(const RShader &data) override;
    void DestroyShader(RShaderHandle h) override;

    RMaterialHandle CreateMaterial(const FSurfaceDesc &surface) override;
    void DestroyMaterial(RMaterialHandle h) override;

    RFramebufferHandle CreateColorTarget(int width, int height, RTextureHandle& outColor); // TODO: Move the pipeline into viewports and shape the renderer into a stateless machine
    void DestroyColorTarget(RFramebufferHandle fbo);

    void SetDefaultShader(RShaderHandle h) { m_DefaultShader = h; }
    [[nodiscard]] RShaderHandle GetDefaultShader() const { return m_DefaultShader; }

    // Allow draw path to query surface by handle
    const FSurfaceDesc* GetMaterialSurface(RMaterialHandle h) const;

    void EnqueueRenderTask(std::function<void()> fn) override;
};
