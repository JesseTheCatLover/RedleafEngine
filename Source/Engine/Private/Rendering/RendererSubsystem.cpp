// Copyright 2025-2026 JesseTheCatLover. All Rights Reserved.

#include "RendererSubsystem.h"

#include "Rendering/IRenderBackend.h"
#include "RRenderProxies.h"
#include "Core/EngineGlobals.h"
#include "Framework/PostProcessManager.h"

#include <algorithm>
#include <unordered_map>
#include <limits>
#include <type_traits>

#include "RObjects.h"
#include "Core/EngineContext.h"
#include "Framework/DebugDrawFramework.h"
#include "Rendering/FRenderView.h"
#include "Scene/JScene.h"
#include "Scene/SceneComponents/JCameraComponent.h"

static void BindFrameAndPassParams(
    IRenderBackend* backend,
    RShaderHandle shader,
    const FPassParam& passParams,
    const FFramePostParams& frameParams,
    int& texUnit)
{
    for (auto& [k,v] : passParams.floats) backend->SetUniformFloat(shader, k.c_str(), v);
    for (auto& [k,v] : frameParams.floats) backend->SetUniformFloat(shader, k.c_str(), v);

    for (auto& [k,v] : passParams.ints) backend->SetUniformInt(shader, k.c_str(), v);
    for (auto& [k,v] : frameParams.ints) backend->SetUniformInt(shader, k.c_str(), v);

    // TODO: Implement this in future
    // for (auto& [k,v] : passParams.vec2s) backend->SetUniformVec2(shader, k.c_str(), v);
    // for (auto& [k,v] : frameParams.vec2s) backend->SetUniformVec2(shader, k.c_str(), v);
    //
    // for (auto& [k,v] : passParams.vec4s) backend->SetUniformVec4(shader, k.c_str(), v);
    // for (auto& [k,v] : frameParams.vec4s) backend->SetUniformVec4(shader, k.c_str(), v);

    // Textures (pass-level first, then frame-level so frame can override)
    for (const auto& [k, tex] : passParams.textures)
    {
        if (k == "u_Input") continue;
        if (!tex.IsValid()) continue;

        backend->BindTexture(tex, texUnit);
        backend->SetUniformInt(shader, k.c_str(), texUnit);
        ++texUnit;
    }

    for (const auto& [k, tex] : frameParams.textures)
    {
        if (k == "u_Input") continue;
        if (!tex.IsValid()) continue;

        backend->BindTexture(tex, texUnit);
        backend->SetUniformInt(shader, k.c_str(), texUnit);
        ++texUnit;
    }
}

RendererSubsystem::RendererSubsystem(IRenderBackend *backend, EngineContext& ctx):
m_Context(ctx),
m_Backend(backend)
{
    BuildDefaultShader();
    m_CoordAdaptor = BuildCoordAdapter(m_Backend->GetCoordConvention());
}

void RendererSubsystem::RenderFrame(const std::vector<FRenderView> &views)
{
    m_Backend->BeginFrame();

    // // Even if there are no views, it's nice to at least clear the platform surface
    // if (views.empty())
    // {
    //     // Fallback: clear default framebuffer to something neutral
    //     m_Backend->BindFramebuffer(RFramebufferHandle{});
    //     m_Backend->SetViewport(0, 0,
    //                            m_Context.GetFramebufferWidth(),
    //                            m_Context.GetFramebufferHeight());
    //     m_Backend->ClearColorDepth(0.2f, 0.3f, 0.3f, 1.f, true);
    //     m_Backend->EndFrame();
    //     return;
    // }

    // Actually clear the platform surface every frame.
    m_Backend->BindFramebuffer(RFramebufferHandle{});
    m_Backend->SetViewport(
        0, 0,
        m_Context.GetFramebufferWidth(),
        m_Context.GetFramebufferHeight());

    m_Backend->ClearColorDepth(0.2f, 0.3f, 0.3f, 1.f, true);

    if (views.empty())
    {
        m_Backend->EndFrame();
        return;
    }

    // 0) Group all views by scene pointer
    std::unordered_map<JScene*, FSceneBatch> sceneBatches;
    sceneBatches.reserve(views.size());

    for (const FRenderView& view : views)
    {
        if (!view.scene || !view.camera || view.viewportW <= 0 || view.viewportH <= 0)
            continue;

        auto& batch = sceneBatches[view.scene];
        batch.scene = view.scene;
        batch.views.push_back(&view);
    }

    // Reset per-frame cache
    m_GPUStateCache.ResetForFrame();

    // 1) Render each scene batch
    for (auto& [scenePtr, batch] : sceneBatches)
    {
        RenderSceneBatch(batch);
    }

    // 2) Restore default framebuffer + viewport for UI/editor
    m_Backend->BindFramebuffer(RFramebufferHandle{});
    m_Backend->SetViewport(0, 0,
                           m_Context.GetFramebufferWidth(),
                           m_Context.GetFramebufferHeight());

    m_Backend->EndFrame();
}

void RendererSubsystem::Shutdown()
{
    while (PopRenderTarget())
    {
    }

    m_RenderTargetStack.clear();

    for (auto& [_, target] : m_RenderTargets)
        DestroyTarget(target);

    m_RenderTargets.clear();

    m_Backend->Shutdown();
}

void RendererSubsystem::DrawCommandBuffer(RCommandBuffer& buffer, const FMatrix4& viewMat, const FProjectionDesc& projDesc)
{
    // Upload lights for this buffer
    if (!buffer.GetLights().empty())
    {
        m_Backend->UploadLights(
            buffer.GetLights().data(),
            static_cast<uint32_t>(buffer.GetLights().size())
        );
    }

    // Local lambda for state changes
    auto setLayerState = [&](ERenderLayer layer)
    {
        switch (layer)
        {
            case ERenderLayer::Opaque:
                m_Backend->SetCullMode(IRenderBackend::ECullMode::Back);
                m_Backend->SetDepthState(true, true, IRenderBackend::ECompareFunc::LessEqual);
                m_Backend->SetBlendState(false,
                    IRenderBackend::EBlendFactor::One,
                    IRenderBackend::EBlendFactor::Zero);
                break;

            case ERenderLayer::Alpha:
                m_Backend->SetCullMode(IRenderBackend::ECullMode::Back);
                m_Backend->SetDepthState(true, false, IRenderBackend::ECompareFunc::LessEqual);
                m_Backend->SetBlendState(true,
                    IRenderBackend::EBlendFactor::SrcAlpha,
                    IRenderBackend::EBlendFactor::OneMinusSrcAlpha);
                break;

            case ERenderLayer::Overlay:
                m_Backend->SetCullMode(IRenderBackend::ECullMode::None);
                m_Backend->SetDepthState(false, false, IRenderBackend::ECompareFunc::Always);
                m_Backend->SetBlendState(true,
                    IRenderBackend::EBlendFactor::SrcAlpha,
                    IRenderBackend::EBlendFactor::OneMinusSrcAlpha);
                break;
        }
    };

    auto drawList = [&](const std::vector<RDrawCommand>& cmds, ERenderLayer L)
    {
        if (cmds.empty()) return;
        setLayerState(L);

        for (const auto& c : cmds)
        {
            // Choose shader handle (use model shader if valid, otherwise fallback)
            RShaderHandle shaderToUse = c.state.shader.IsValid()
                ? c.state.shader
                : m_DefaultShader;

            if (!shaderToUse.IsValid())
            {
                BuildDefaultShader();
                shaderToUse = m_DefaultShader;
            }

            // Only re-bind when shader actually changed
            if (shaderToUse.id != m_GPUStateCache.shader.id)
            {
                m_GPUStateCache.shader = shaderToUse;
                m_Backend->BindShader(shaderToUse);

                // Upload camera matrices to the shader we just bound
                ApplyCamera(shaderToUse,  viewMat, projDesc);

                // Set light count once per shader bind
                const int lightCount =
                    (int)std::min<size_t>(buffer.GetLights().size(),
                                          IRenderBackend::kMaxLights);
                m_Backend->SetUniformInt(shaderToUse, "u_LightCount", lightCount);
            }

            // Material cache update
            if (c.state.material != m_GPUStateCache.material)
            {
                m_GPUStateCache.material = c.state.material;

                if (const FSurfaceDesc* surf = GetMaterialSurface(c.state.material))
                {
                    int unit = 0;

                    if (surf->baseColor.IsValid())
                    {
                        m_Backend->BindTexture(surf->baseColor, unit);
                        m_Backend->SetUniformInt(shaderToUse, "u_BaseColor", unit);
                        m_Backend->SetUniformInt(shaderToUse, "u_UseBaseColorMap", 1);
                        ++unit;
                    }
                    else
                    {
                        m_Backend->SetUniformVec4(shaderToUse,
                            "u_BaseColorFactor", surf->params.baseColorFactor);
                        m_Backend->SetUniformInt(shaderToUse, "u_UseBaseColorMap", 0);
                    }

                    if (surf->normal.IsValid())
                    {
                        m_Backend->BindTexture(surf->normal, unit);
                        m_Backend->SetUniformInt(shaderToUse, "u_NormalMap", unit);
                        ++unit;
                    }

                    if (surf->metallicRoughness.IsValid())
                    {
                        m_Backend->BindTexture(surf->metallicRoughness, unit);
                        m_Backend->SetUniformInt(shaderToUse, "u_MetalRoughMap", unit);
                        ++unit;

                        m_Backend->SetUniformFloat(shaderToUse,
                            "u_MetallicFactor",  surf->params.metallicFactor);
                        m_Backend->SetUniformFloat(shaderToUse,
                            "u_RoughnessFactor", surf->params.roughnessFactor);
                    }

                    if (surf->emissive.IsValid())
                    {
                        m_Backend->BindTexture(surf->emissive, unit);
                        m_Backend->SetUniformInt(shaderToUse, "u_EmissiveMap", unit);
                        ++unit;
                    }

                    m_Backend->SetUniformVec2(shaderToUse,
                        "u_UVTiling", surf->params.uvTiling);
                }
            }

            DrawMesh(c.state.mesh, shaderToUse, c.transform);
        }
    };

    // Now draw for each layer
    drawList(buffer.opaque.GetDrawCommands(),   ERenderLayer::Opaque);
    drawList(buffer.alpha.GetDrawCommands(),    ERenderLayer::Alpha);
    drawList(buffer.overlay.GetDrawCommands(),  ERenderLayer::Overlay);
}
void RendererSubsystem::DrawCustomDepthPass(const RCommandBuffer& cmd,
                                            const FMatrix4& viewMat,
                                            const FProjectionDesc& projDesc)
{
    EnsureCustomDepthShader();

    // CustomDepth pass state:
    // - Depth test/write ON
    // - Blend OFF
    // - Stencil ON (write stencil ref on depth pass)
    m_Backend->SetCullMode(IRenderBackend::ECullMode::Back);
    m_Backend->SetDepthState(true, true, IRenderBackend::ECompareFunc::LessEqual);
    m_Backend->SetBlendState(false,
        IRenderBackend::EBlendFactor::One,
        IRenderBackend::EBlendFactor::Zero);

    // Color write: optional. If you only want stencil+depth, disable color writes.
    // For now, we can KEEP color writes so your existing outline shader sampling u_CustomID still works.
    m_Backend->SetColorWriteMask(true, true, true, true);

    // Base stencil config (we’ll vary ref per draw)
    IRenderBackend::FStencilState st{};
    st.bEnable   = true;
    st.func      = IRenderBackend::ECompareFunc::Always; // always pass
    st.readMask  = 0xFF;
    st.writeMask = 0xFF;
    st.sfail     = IRenderBackend::EStencilOp::Keep;
    st.zfail     = IRenderBackend::EStencilOp::Keep;
    st.zpass     = IRenderBackend::EStencilOp::Replace;  // write ref when depth passes

    m_Backend->BindShader(m_CustomDepthShader);
    ApplyCamera(m_CustomDepthShader, viewMat, projDesc);

    auto drawQueue = [&](const std::vector<RDrawCommand>& cmds)
    {
        for (const auto& dc : cmds)
        {
            if (!dc.state.mesh.IsValid()) continue;
            if (!dc.bWriteCustomDepth) continue;
            if (dc.customStencil == 0) continue;

            // 1) Real stencil write
            st.ref = dc.customStencil;
            m_Backend->SetStencilState(st);

            // 2) Optional bridge: still write ID into color so post can sample u_CustomID
            const float id01 = (float)dc.customStencil / 255.0f;
            m_Backend->SetUniformFloat(m_CustomDepthShader, "u_ID01", id01);

            DrawMesh(dc.state.mesh, m_CustomDepthShader, dc.transform);
        }
    };

    drawQueue(cmd.opaque.GetDrawCommands());
    drawQueue(cmd.alpha.GetDrawCommands());
    drawQueue(cmd.overlay.GetDrawCommands());

    // Clean up: don't leak stencil state into later passes
    IRenderBackend::FStencilState off{};
    off.bEnable = false;
    m_Backend->SetStencilState(off);
}

void RendererSubsystem::DrawSceneStencilMaskPass(const RCommandBuffer& cmd,const FMatrix4& viewMat,
    const FProjectionDesc& projDesc)
{
    EnsureCustomDepthShader(); // good enough: position-only

    // Don't touch color
    m_Backend->SetColorWriteMask(false, false, false, false);

    // Only mark visible fragments
    m_Backend->SetCullMode(IRenderBackend::ECullMode::Back);
    m_Backend->SetDepthState(true, false, IRenderBackend::ECompareFunc::LessEqual);
    m_Backend->SetBlendState(false,
        IRenderBackend::EBlendFactor::One,
        IRenderBackend::EBlendFactor::Zero);

    IRenderBackend::FStencilState st{};
    st.bEnable   = true;
    st.func      = IRenderBackend::ECompareFunc::Always;
    st.readMask  = 0xFF;
    st.writeMask = 0xFF;
    st.sfail     = IRenderBackend::EStencilOp::Keep;
    st.zfail     = IRenderBackend::EStencilOp::Keep;
    st.zpass     = IRenderBackend::EStencilOp::Replace;

    m_Backend->BindShader(m_CustomDepthShader);
    ApplyCamera(m_CustomDepthShader, viewMat, projDesc);

    auto drawQueue = [&](const std::vector<RDrawCommand>& cmds)
    {
        for (const auto& dc : cmds)
        {
            if (!dc.state.mesh.IsValid()) continue;
            if (!dc.bWriteCustomDepth)    continue;   // "marked"
            if (dc.customStencil == 0)    continue;

            st.ref = dc.customStencil;
            m_Backend->SetStencilState(st);

            // Color is off, so FragColor doesn't matter.
            DrawMesh(dc.state.mesh, m_CustomDepthShader, dc.transform);
        }
    };

    drawQueue(cmd.opaque.GetDrawCommands());
    drawQueue(cmd.alpha.GetDrawCommands());
    drawQueue(cmd.overlay.GetDrawCommands());

    // Restore
    m_Backend->SetColorWriteMask(true, true, true, true);
    IRenderBackend::FStencilState off{}; off.bEnable = false;
    m_Backend->SetStencilState(off);
}

bool RendererSubsystem::DrawFullscreenTexture(RTextureHandle texture)
{
    if (!m_Backend || !texture.IsValid())
        return false;

    EnsureFullscreenQuad();

    if (!m_FSQuad.IsValid() || !m_LinearCopyShader.IsValid())
        return false;

    m_Backend->BindShader(m_LinearCopyShader);
    m_Backend->SetUniformInt(m_LinearCopyShader, "u_Input", 0);
    m_Backend->BindTexture(texture, 0);
    m_Backend->SubmitMesh(m_FSQuad, m_LinearCopyShader, FMatrix4::Identity());

    return true;
}

void RendererSubsystem::BuildDefaultShader()
{
    if (m_DefaultShader.IsValid())
        return;

    // Attributes:
    //   location=0: position (vec3)
    //   location=1: normal   (vec3)
    //   location=2: uv       (vec2)
    //
    // Uniforms expected from engine:
    //   u_Model, u_View, u_Proj        : mat4
    //   u_BaseColor                    : sampler2D (optional)
    //   u_BaseColorFactor              : vec4      (used if no texture)
    //   u_UseBaseColorMap              : int       (0/1)
    //
    // Notes:
    //  - Hemisphere lighting avoids needing a light buffer/UBO for the "first pixels".
    //  - Normal transform uses a standard normal matrix from u_Model.
    const char* kVertex = R"(#version 330 core
    layout (location=0) in vec3 aPos;
    layout (location=1) in vec3 aNormal;
    layout (location=2) in vec2 aUV;

    uniform mat4 u_Model;
    uniform mat4 u_View;
    uniform mat4 u_Proj;

    out vec3 vWorldNormal;
    out vec2 vUV;

    void main()
    {
        // normal matrix = transpose(inverse(mat3(u_Model)))
        mat3 N = transpose(inverse(mat3(u_Model)));
        vWorldNormal = normalize(N * aNormal);
        vUV = aUV;

        gl_Position = u_Proj * u_View * u_Model * vec4(aPos, 1.0);
    }
)";

    const char* kFragment = R"(#version 330 core
    in vec3 vWorldNormal;
    in vec2 vUV;
    out vec4 FragColor;

    uniform sampler2D u_BaseColor;        // only valid if u_UseBaseColorMap == 1
    uniform vec4      u_BaseColorFactor;  // used when no texture
    uniform int       u_UseBaseColorMap;  // 0 or 1

    // Simple hemisphere lighting so there's always something visible.
    const vec3 SKY_COLOR    = vec3(0.6, 0.7, 0.9);
    const vec3 GROUND_COLOR = vec3(0.3, 0.25, 0.2);
    const float AMBIENT     = 0.15;

    void main()
    {
        vec4 baseColorTex = texture(u_BaseColor, vUV);
        vec4 baseColor    = (u_UseBaseColorMap == 1) ? baseColorTex : u_BaseColorFactor;

        // Hemisphere term (y-up)
        // float hemiT = clamp(vWorldNormal.y * 0.5 + 0.5, 0.0, 1.0);
        // vec3 hemi   = mix(GROUND_COLOR, SKY_COLOR, hemiT);

        // vec3 lit = baseColor.rgb * (AMBIENT + hemi);
        // FragColor = vec4(lit, baseColor.a);

        // DEBUG: UNLIT, NO LIGHTING
        FragColor = baseColor;
    }

)";

    RShader sh{};
    sh.vertexSource = kVertex;
    sh.fragmentSource = kFragment;
    m_DefaultShader = m_Backend->CreateShader(sh);
}

void RendererSubsystem::EnsureCustomDepthShader()
{
    if (m_CustomDepthShader.IsValid()) return;

    RShader s{};
    s.vertexSource = R"(
        #version 330 core
        layout(location=0) in vec3 aPos;
        uniform mat4 u_Model;
        uniform mat4 u_View;
        uniform mat4 u_Proj;
        void main()
        {
            gl_Position = u_Proj * u_View * u_Model * vec4(aPos, 1.0);
        }
    )";

    // ID written into color (R8 or RGBA8)
    s.fragmentSource = R"(
        #version 330 core
        out vec4 FragColor;
        uniform float u_ID01; // stencil/id normalized [0..1]
        void main()
        {
            FragColor = vec4(u_ID01, 0.0, 0.0, 1.0);
        }
    )";

    m_CustomDepthShader = m_Backend->CreateShader(s);
}

void RendererSubsystem::EnsureOutlineShader()
{
    if (m_OutlineShader.IsValid())
        return;

    // Not strictly required for compilation, but ensures fullscreen tri exists early.
    EnsureFullscreenQuad();

    RShader s{};

    // Fullscreen vertex
    s.vertexSource = R"(#version 330 core
        layout(location=0) in vec3 aPos;
        layout(location=2) in vec2 aUV;

        out vec2 vUV;

        void main()
        {
            vUV = aUV;
            gl_Position = vec4(aPos, 1.0);
        }
    )";

    // Outline: CustomID + CustomDepth + SceneDepth
    // - Edge is computed using a dilation ring (radius 1..4 px controlled by u_Thickness)
    // - Occlusion is computed using min neighbor CustomDepth (important!)
    // - Optional fill tint inside selected object
    s.fragmentSource = R"(#version 330 core
        in vec2 vUV;
        out vec4 FragColor;

        uniform sampler2D u_Input;
        uniform sampler2D u_CustomID;
        uniform sampler2D u_CustomDepth;
        uniform sampler2D u_SceneDepth;

        // Selected stencil id in 0..1 (your engine passes stencil/255)
        uniform float u_StencilRef01;

        // Thickness in pixels (1..4 recommended with this shader)
        uniform float u_Thickness;

        // Texture texel size (1/width, 1/height)
        uniform float u_InvW;
        uniform float u_InvH;

        // Depth test epsilon for occlusion comparisons
        uniform float u_DepthEpsilon;

        // Outline color provided as floats (since your param binder currently does floats/ints/textures)
        uniform float u_OutlineR;
        uniform float u_OutlineG;
        uniform float u_OutlineB;
        uniform float u_OutlineA;

        // Fill tint strength inside the selected object (0 disables fill)
        uniform float u_FillAlpha;

        // 1 = hide when occluded by scene depth, 0 = x-ray outline
        uniform float u_Occlusion;

        const float kIDHalfStep = 0.5 / 255.0;

        float IsSelected(float id01)
        {
            // IDs are quantized to 8-bit in CustomID render target.
            // Use a half-step threshold to match the bucket.
            return step(abs(id01 - u_StencilRef01), kIDHalfStep);
        }

        void SampleNeighbor(vec2 uv, float include, inout float anySel, inout float minCd)
        {
            if (include < 0.5) return;

            float id = texture(u_CustomID, uv).r;
            float s  = IsSelected(id);
            if (s > 0.5)
            {
                anySel = 1.0;
                float cd = texture(u_CustomDepth, uv).r;
                minCd = min(minCd, cd);
            }
        }

        void main()
        {
            vec4 scene = texture(u_Input, vUV);

            // If no selection requested, passthrough.
            if (u_StencilRef01 <= 0.0)
            {
                FragColor = scene;
                return;
            }

            vec2 texel = vec2(u_InvW, u_InvH);

            // Center mask + depths
            float idC = texture(u_CustomID, vUV).r;
            float mC  = IsSelected(idC);

            float sd  = texture(u_SceneDepth,  vUV).r;
            float cdC = texture(u_CustomDepth, vUV).r;

            // Fill visibility uses the center depth (valid because center is inside the mask for fill)
            float visibleFill = 1.0;
            if (u_Occlusion > 0.5)
                visibleFill = step(cdC, sd + u_DepthEpsilon);

            // Edge detection:
            // edge pixel = (any neighbor selected) AND (center NOT selected)
            float anySel = 0.0;
            float minCd  = 1.0; // neighbor-based occlusion uses min neighbor custom depth

            // Dilation ring: radius 1..4
            // (Keeping it fixed-size avoids driver weirdness and keeps perf predictable.)
            for (int r = 1; r <= 4; ++r)
            {
                // include this radius if r <= thickness (+0.5 for nicer thresholding)
                float include = step(float(r) - 0.5, u_Thickness);
                vec2 off = texel * float(r);

                // 8-neighborhood sampling
                SampleNeighbor(vUV + vec2( off.x, 0.0), include, anySel, minCd);
                SampleNeighbor(vUV + vec2(-off.x, 0.0), include, anySel, minCd);
                SampleNeighbor(vUV + vec2(0.0,  off.y), include, anySel, minCd);
                SampleNeighbor(vUV + vec2(0.0, -off.y), include, anySel, minCd);

                SampleNeighbor(vUV + vec2( off.x,  off.y), include, anySel, minCd);
                SampleNeighbor(vUV + vec2( off.x, -off.y), include, anySel, minCd);
                SampleNeighbor(vUV + vec2(-off.x,  off.y), include, anySel, minCd);
                SampleNeighbor(vUV + vec2(-off.x, -off.y), include, anySel, minCd);
            }

            float edge = clamp(anySel - mC, 0.0, 1.0);

            // Edge visibility uses min neighbor depth (CRITICAL):
            // outline pixels are outside the mask, so center depth is often cleared to 1.0.
            float visibleEdge = 1.0;
            if (u_Occlusion > 0.5)
                visibleEdge = step(minCd, sd + u_DepthEpsilon);

            vec3 outlineColor = vec3(u_OutlineR, u_OutlineG, u_OutlineB);

            // Fill inside object (subtle tint)
            float fillA = clamp(u_FillAlpha, 0.0, 1.0) * mC * visibleFill;
            vec3 color = mix(scene.rgb, outlineColor, fillA);

            // Outline on top
            float outA = clamp(u_OutlineA, 0.0, 1.0) * edge * visibleEdge;
            color = mix(color, outlineColor, outA);

            FragColor = vec4(color, scene.a);
        }
    )";

    m_OutlineShader = m_Backend->CreateShader(s);
}

void RendererSubsystem::EnsureFXAAShader()
{
    if (m_FXAAShader.IsValid())
        return;

    EnsureFullscreenQuad();

    RShader s{};

    s.vertexSource = R"(#version 330 core
        layout(location=0) in vec3 aPos;
        layout(location=2) in vec2 aUV;
        out vec2 vUV;
        void main()
        {
            vUV = aUV;
            gl_Position = vec4(aPos, 1.0);
        }
    )";

    s.fragmentSource = R"(#version 330 core
        in vec2 vUV;
        out vec4 FragColor;

        uniform sampler2D u_Input;

        // REQUIRED: 1/width and 1/height
        uniform float u_InvW;
        uniform float u_InvH;

        // Tuning (good defaults)
        // Reduce = how much to smooth low-contrast edges
        // SpanMax = max search distance in pixels (8 is typical)
        uniform float u_FXAA_ReduceMin;   // e.g. 1.0/128.0
        uniform float u_FXAA_ReduceMul;   // e.g. 1.0/8.0
        uniform float u_FXAA_SpanMax;     // e.g. 8.0

        float Luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

        void main()
        {
            vec2 texel = vec2(u_InvW, u_InvH);

            vec3 rgbM  = texture(u_Input, vUV).rgb;
            vec3 rgbNW = texture(u_Input, vUV + texel * vec2(-1.0, -1.0)).rgb;
            vec3 rgbNE = texture(u_Input, vUV + texel * vec2( 1.0, -1.0)).rgb;
            vec3 rgbSW = texture(u_Input, vUV + texel * vec2(-1.0,  1.0)).rgb;
            vec3 rgbSE = texture(u_Input, vUV + texel * vec2( 1.0,  1.0)).rgb;

            float lumaM  = Luma(rgbM);
            float lumaNW = Luma(rgbNW);
            float lumaNE = Luma(rgbNE);
            float lumaSW = Luma(rgbSW);
            float lumaSE = Luma(rgbSE);

            float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
            float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

            vec2 dir;
            dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
            dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

            float dirReduce = max(
                (lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * u_FXAA_ReduceMul),
                u_FXAA_ReduceMin
            );

            float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);

            dir = clamp(dir * rcpDirMin,
                        vec2(-u_FXAA_SpanMax, -u_FXAA_SpanMax),
                        vec2( u_FXAA_SpanMax,  u_FXAA_SpanMax)) * texel;

            vec3 rgbA = 0.5 * (
                texture(u_Input, vUV + dir * (1.0/3.0 - 0.5)).rgb +
                texture(u_Input, vUV + dir * (2.0/3.0 - 0.5)).rgb
            );

            vec3 rgbB = rgbA * 0.5 + 0.25 * (
                texture(u_Input, vUV + dir * (-0.5)).rgb +
                texture(u_Input, vUV + dir * ( 0.5)).rgb
            );

            float lumaB = Luma(rgbB);

            vec3 outRGB = (lumaB < lumaMin || lumaB > lumaMax) ? rgbA : rgbB;

            FragColor = vec4(outRGB, 1.0);
        }
    )";

    m_FXAAShader = m_Backend->CreateShader(s);
}

void RendererSubsystem::EnsureUIGeometryShader()
{
    if (m_UIGeometryShader.IsValid())
        return;

    RShader shader{};

    shader.vertexSource = R"(
        #version 330 core

        layout(location = 0) in vec2 aPosition;
        layout(location = 1) in vec2 aTexCoord;
        layout(location = 2) in vec4 aColour;

        uniform vec2 u_Translation;
        uniform mat4 u_Transform;

        out vec2 vTexCoord;
        out vec4 vColour;

        void main()
        {
            vec2 position = aPosition + u_Translation;

            gl_Position = u_Transform * vec4(position, 0.0, 1.0);

            vTexCoord = aTexCoord;
            vColour = aColour;
        }
    )";

    shader.fragmentSource = R"(
        #version 330 core

        in vec2 vTexCoord;
        in vec4 vColour;

        uniform sampler2D u_Texture;
        uniform int u_UseTexture;

        out vec4 FragColor;

        void main()
        {
            vec4 texel = u_UseTexture != 0
                ? texture(u_Texture, vTexCoord)
                : vec4(1.0);

            FragColor = texel * vColour;
        }
    )";

    m_UIGeometryShader = m_Backend->CreateShader(shader);
}

void RendererSubsystem::EnsureGaussianBlurShader()
{
    if (m_GaussianBlurShader.IsValid())
        return;

    RShader shader{};

    shader.vertexSource = R"(
        #version 330 core

        layout(location = 0) in vec3 aPosition;
        layout(location = 2) in vec2 aTexCoord;

        out vec2 vUV;

        void main()
        {
            vUV = aTexCoord;
            gl_Position = vec4(aPosition, 1.0);
        }
    )";

    shader.fragmentSource = R"(
        #version 330 core

        in vec2 vUV;
        out vec4 FragColor;

        uniform sampler2D u_Input;
        uniform float u_Sigma;
        uniform vec2 u_TexelOffset;
        uniform vec2 u_TexCoordMin;
        uniform vec2 u_TexCoordMax;

        const int MAX_BLUR_RADIUS = 32;

        void main()
        {
            // Preserve pixels outside the requested region.
            if (vUV.x < u_TexCoordMin.x ||
                vUV.y < u_TexCoordMin.y ||
                vUV.x > u_TexCoordMax.x ||
                vUV.y > u_TexCoordMax.y)
            {
                FragColor = texture(u_Input, vUV);
                return;
            }

            float sigma = max(u_Sigma, 0.0001);
            int radius = min(
                int(ceil(3.0 * sigma)),
                MAX_BLUR_RADIUS);

            vec4 color = texture(u_Input, vUV);
            float totalWeight = 1.0;

            for (int i = 1; i <= MAX_BLUR_RADIUS; ++i)
            {
                if (i > radius)
                    break;

                float sampleOffset = float(i);
                float weight = exp(
                    -(sampleOffset * sampleOffset) /
                    (2.0 * sigma * sigma));

                vec2 offset = u_TexelOffset * sampleOffset;

                vec2 positiveUV = clamp(
                    vUV + offset,
                    u_TexCoordMin,
                    u_TexCoordMax);

                vec2 negativeUV = clamp(
                    vUV - offset,
                    u_TexCoordMin,
                    u_TexCoordMax);

                color += texture(u_Input, positiveUV) * weight;
                color += texture(u_Input, negativeUV) * weight;

                totalWeight += 2.0 * weight;
            }

            FragColor = color / totalWeight;
        }
    )";

    m_GaussianBlurShader = m_Backend->CreateShader(shader);
}

bool RendererSubsystem::EnsureFilterScratchTarget(int width, int height)
{
    if (!m_Backend || width <= 0 || height <= 0)
        return false;

    if (m_FilterScratchTarget.IsValid())
    {
        const auto it = m_RenderTargets.find(m_FilterScratchTarget.id);

        if (it != m_RenderTargets.end())
        {
            const bool bActive = std::find(
                m_RenderTargetStack.begin(),
                m_RenderTargetStack.end(),
                m_FilterScratchTarget) != m_RenderTargetStack.end();

            if (bActive)
                return false;

            if (it->second.w == width &&
                it->second.h == height &&
                it->second.color.IsValid() &&
                !it->second.bHasDepthStencil)
                return true;

            DestroyRenderTarget(m_FilterScratchTarget);
        }

        m_FilterScratchTarget = {};
    }

    FRenderTargetDesc desc{};
    desc.width = width;
    desc.height = height;
    desc.colorFormat = ERenderTargetColorFormat::RGBA16F;
    desc.bHasDepthStencil = false;

    m_FilterScratchTarget = CreateRenderTarget(desc);

    return m_FilterScratchTarget.IsValid();
}

void RendererSubsystem::EnsureDebugLineShader()
{
    if (m_DebugLineShader.IsValid())
        return;

    RShader s{};

    s.vertexSource = R"(
        #version 330 core
        layout(location=0) in vec3 aPos;
        layout(location=1) in vec4 aColor;

        uniform mat4 u_View;
        uniform mat4 u_Proj;

        out vec4 vColor;

        void main()
        {
            vColor = aColor;
            gl_Position = u_Proj * u_View * vec4(aPos, 1.0);
        }
    )";

    s.fragmentSource = R"(
        #version 330 core
        in vec4 vColor;
        out vec4 FragColor;
        void main() { FragColor = vColor; }
    )";

    m_DebugLineShader = m_Backend->CreateShader(s);
}

void RendererSubsystem::EnsureDebugClipTriShader()
{
    if (m_DebugClipTriShader.IsValid()) return;

    RShader s{};
    s.vertexSource = R"(
        #version 330 core
        layout(location=0) in vec4 aPos;    // clip space
        layout(location=1) in vec4 aColor;

        out vec4 vColor;
        void main()
        {
            vColor = aColor;
            gl_Position = aPos;
        }
    )";

    s.fragmentSource = R"(
        #version 330 core
        in vec4 vColor;
        out vec4 FragColor;
        void main() { FragColor = vColor; }
    )";

    m_DebugClipTriShader = m_Backend->CreateShader(s);
}

void RendererSubsystem::EnsureDebugWorldTriShader()
{
    if (m_DebugWorldTriShader.IsValid()) return;

    RShader s{};
    s.vertexSource = R"(
        #version 330 core
        layout(location=0) in vec3 aPos;
        layout(location=1) in vec3 aNrm;
        layout(location=2) in vec4 aColor;

        uniform mat4 u_View;
        uniform mat4 u_Proj;

        out vec3 vNrmVS;
        out vec3 vPosVS;
        out vec4 vColor;

        void main()
        {
            vec4 posVS = u_View * vec4(aPos, 1.0);
            vPosVS = posVS.xyz;
            vNrmVS = normalize(mat3(u_View) * aNrm);

            vColor = aColor;
            gl_Position = u_Proj * posVS;
        }
    )";

    s.fragmentSource = R"(
        #version 330 core
        in vec3 vNrmVS;
        in vec3 vPosVS;
        in vec4 vColor;
        out vec4 FragColor;

        uniform vec3  u_LightDirVS;
        uniform float u_Ambient;
        uniform float u_Rim;
        uniform float u_Spec;

        void main()
        {
            vec3 N = normalize(vNrmVS);
            vec3 V = normalize(-vPosVS);
            vec3 L = normalize(u_LightDirVS);

            float ndl = max(dot(N, L), 0.0);
            float diffuse = mix(u_Ambient, 1.0, ndl);

            float rim = pow(1.0 - max(dot(N, V), 0.0), 2.0) * u_Rim;

            vec3 R = reflect(-L, N);
            float spec = pow(max(dot(R, V), 0.0), 32.0) * u_Spec;

            vec3 rgb = vColor.rgb * diffuse + rim + spec;
            FragColor = vec4(rgb, vColor.a);
        }
    )";

    m_DebugWorldTriShader = m_Backend->CreateShader(s);
}

void RendererSubsystem::EnsureTargets(int w, int h, int samples)
{
    auto needExact = [&](const FTarget& t, int ew, int eh, int es)
    {
        return !t.fbo.IsValid() || t.w != ew || t.h != eh || t.samples != es;
    };

    // Scene single-sample (always 1)
    if (needExact(m_Scene, w, h, 1))
    {
        DestroyTarget(m_Scene);
        BuildTarget(m_Scene, w, h, 1, /*withDepth*/true, /*hdr*/true, /*srgb*/false);
    }

    // Scene MSAA (only if samples > 1)
    if (samples > 1)
    {
        if (needExact(m_SceneMSAA, w, h, samples))
        {
            DestroyTarget(m_SceneMSAA);
            BuildTarget(m_SceneMSAA, w, h, samples, /*withDepth*/true, /*hdr*/true, /*srgb*/false);
        }
    }
    else
    {
        if (m_SceneMSAA.fbo.IsValid())
            DestroyTarget(m_SceneMSAA);
    }

    // Ping/pong (always 1)
    if (needExact(m_Ping, w, h, 1) || needExact(m_Pong, w, h, 1))
    {
        DestroyTarget(m_Ping);
        DestroyTarget(m_Pong);
        BuildTarget(m_Ping, w, h, 1, /*withDepth*/false, /*hdr*/true, /*srgb*/false);
        BuildTarget(m_Pong, w, h, 1, /*withDepth*/false, /*hdr*/true, /*srgb*/false);
    }

    // Custom (always 1)
    if (needExact(m_Custom, w, h, 1))
    {
        DestroyTarget(m_Custom);
        BuildCustomTarget(m_Custom, w, h);
    }
}

void RendererSubsystem::DestroyTarget(FTarget &t)
{
    if (t.fbo.IsValid()) m_Backend->DestroyFramebuffer(t.fbo);
    t = {}; // backend should have invalidated the exported handles
}

void RendererSubsystem::BuildTarget(FTarget& t, int w, int h, int samples, bool withDepth, bool hdr, bool srgb)
{
    // Describe the framebuffer; the backend owns its attachments.
    RFramebuffer fb{};
    fb.width = w;
    fb.height = h;
    fb.samples = std::max(1, samples);

    fb.bHasColor = true;
    fb.bColorAsTexture = true;
    fb.bDepthAsTexture = withDepth && fb.samples == 1;

    fb.colorMode = srgb ? EColorMode::sRGB8_A8 : hdr ? EColorMode::HDR16F : EColorMode::LDR8;
    fb.depthMode = withDepth ? EDepthMode::D24S8 : EDepthMode::None;

    t.fbo = m_Backend->CreateFramebuffer(fb);
    if (!t.fbo.IsValid())
        return;

    // Retrieve handles for the framebuffer attachments.
    t.color = m_Backend->GetFramebufferColorTexture(t.fbo);

    if (fb.bDepthAsTexture)
        t.depth = m_Backend->GetFramebufferDepthTexture(t.fbo);

    t.bHasDepthStencil = withDepth;
    t.w = w;
    t.h = h;
    t.samples = fb.samples;
}
void RendererSubsystem::BuildCustomTarget(FTarget& t, int w, int h)
{
    RFramebuffer fb{};
    fb.width  = w;
    fb.height = h;
    fb.samples = 1;

    fb.bHasColor = true;     // (ID is stored in color)
    fb.bColorAsTexture = true;
    fb.bDepthAsTexture = true;

    fb.colorMode = EColorMode::LDR8;
    fb.depthMode = EDepthMode::D24S8; // Custom depth buffer

    t.bHasDepthStencil = true;

    t.fbo = m_Backend->CreateFramebuffer(fb);
    t.color = m_Backend->GetFramebufferColorTexture(t.fbo); // CustomID
    t.depth = m_Backend->GetFramebufferDepthTexture(t.fbo); // CustomDepth
    t.w = w; t.h = h; t.samples = 1;
}


RTextureHandle RendererSubsystem::RunPostProcessChain(RTextureHandle sceneColor, int w, int h, uint32_t profileId,
                                                      const FFramePostParams& frameParams)
{
    // If there's no PPM or no active passes, just return the input scene color
    if (!m_PPM)
    {
        return sceneColor;
    }

    // Sync shaders with UI/gameplay changes
    RebuildKernelsIfDirty(profileId);

    const auto& chain = m_PPM->GetChain(profileId);

    bool bAnyEnabled = false;
    for (auto& p : chain)
        if (p.bEnabled)
            { bAnyEnabled = true; break; }

    if (!bAnyEnabled)
    {
        return sceneColor;
    }

    // Ping-pong
    bool bUsePing = true;
    RTextureHandle current = sceneColor;

    for (const auto& pass : chain)
    {
        if (!pass.bEnabled) continue;

        auto it = m_Kernels.find(pass.name);
        if (it == m_Kernels.end()) continue;

        FTarget& dst = bUsePing ? m_Ping : m_Pong;
        m_Backend->BindFramebuffer(dst.fbo);
        m_Backend->SetViewport(0, 0, dst.w, dst.h);

        // Bind shader once here (BlitFullscreen will bind again; uniforms persist)
        m_Backend->BindShader(it->second.shader);

        int texUnit = 1; // 0 reserved for u_Input
        BindFrameAndPassParams(m_Backend, it->second.shader, pass.params, frameParams, texUnit);

        // Optional kernel-specific param binding (leave for later, or keep if needed)
        if (it->second.BindParams)
            it->second.BindParams(m_Backend, it->second.shader, pass.params);

        // Draw (this binds u_Input to unit 0 and current texture to unit 0)
        BlitFullscreen(it->second.shader, current, dst.w, dst.h);

        m_Backend->UnbindFramebuffer();

        current = dst.color;
        bUsePing = !bUsePing;
    }

    // Final texture
    return current;
}

void RendererSubsystem::RenderSceneBatch(const FSceneBatch& batch)
{
    if (!batch.scene || batch.views.empty())
        return;

    // Gather renderables once for this scene
    m_SceneCmd.Clear();
    {
        FRenderContext ctx{};
        // TODO: With scene batching we have this trade off. We can make GatherRenderables view-agnostic. anything else
        // is done after gather, per-view. just like the depthbuckets.
        ctx.view = nullptr;

        batch.scene->GatherRenderables(m_SceneCmd, ctx);
    }

    // 2) For each view, do per-view depth buckets, sort, draw, post, present.
    for (const FRenderView* viewPtr : batch.views)
    {
        const FRenderView& view = *viewPtr;

        if (!view.camera || view.viewportW <= 0 || view.viewportH <= 0)
            continue;

        // Reset GPU states per view
        m_GPUStateCache.ResetForView();

        // 1) Build per-view targets (size + samples)
        EnsureTargets(view.viewportW, view.viewportH, view.sampleCount);

        FTarget& sceneRT = (view.sampleCount > 1 && m_SceneMSAA.fbo.IsValid())
            ? m_SceneMSAA
            : m_Scene;

        m_Backend->BindFramebuffer(sceneRT.fbo);
        m_Backend->SetViewport(0, 0, sceneRT.w, sceneRT.h);

        // 2) Clear Color/Depth if requested
        if (view.bClearColor)
        {
            m_Backend->ClearColorDepthStencil(
                view.clearColorValue.x, view.clearColorValue.y, view.clearColorValue.z, view.clearColorValue.w,
                1.f, 0 // depth=1, stencil=0
            );
        }
        else
        {
            // if you keep a separate function, at least clear depth+stencil here
            m_Backend->ClearDepthStencil(1.f, 0);
        }


        // 3) Build camera matrices
        const float aspect = (sceneRT.h > 0)
            ? static_cast<float>(sceneRT.w) / static_cast<float>(sceneRT.h)
            : 1.f;

        const FMatrix4 viewMat = view.camera->GetViewMatrix();
        const FProjectionDesc projDesc = view.camera->GetProjectionDesc(aspect);

        // 4) Build a per-view command buffer from the scene-wide one
        // TODO: For now we just copy it; later we can optimize this to avoid full copy.
        RCommandBuffer viewCmd = m_SceneCmd;

        const float nearPlane = view.camera->GetNearPlane();
        const float farPlane  = view.camera->GetFarPlane();

        RCommandQueue::ComputeDepthBucketsFor(
            viewCmd.opaque, viewMat, nearPlane, farPlane);
        RCommandQueue::ComputeDepthBucketsFor(
            viewCmd.alpha,  viewMat, nearPlane, farPlane);

        viewCmd.SortAllQueues();

        // 5) Draw this view into sceneRT
        DrawCommandBuffer(viewCmd, viewMat, projDesc);

        // Cache for debug + thick line clip building
        {
            m_ViewCache.viewEngine = viewMat;
            m_ViewCache.projDesc   = projDesc;

            m_ViewCache.viewBackend = m_CoordAdaptor.EngineToBackend * viewMat;
            m_ViewCache.projBackend = m_Backend->BuildProjectionMatrix(projDesc);
            m_ViewCache.vpBackend   = m_ViewCache.projBackend * m_ViewCache.viewBackend;

            m_ViewCache.vpW = view.viewportW;
            m_ViewCache.vpH = view.viewportH;
            m_ViewCache.viewIndex = view.viewIndex;
        }

        // Draw debug lines into the same target as the scene
        if (GEngine && GEngine->GetDebugDraw())
        {
            GEngine->GetDebugDraw()->RenderForView(*this, view);
        }

        // 6) Resolve MSAA if needed
        if (view.sampleCount > 1 && m_SceneMSAA.fbo.IsValid())
        {
            m_Backend->ResolveFramebuffer(
                m_SceneMSAA.fbo, m_Scene.fbo,
                (IRenderBackend::EResolveMask)(
                    (uint8_t)IRenderBackend::EResolveMask::Color |
                    (uint8_t)IRenderBackend::EResolveMask::Depth |
                    (uint8_t)IRenderBackend::EResolveMask::Stencil
                ),
                IRenderBackend::EResolveFilter::Nearest
            );

            // IMPORTANT: switch to resolved target for any further drawing
            m_Backend->BindFramebuffer(m_Scene.fbo);
            m_Backend->SetViewport(0, 0, m_Scene.w, m_Scene.h);
        }

        RTextureHandle finalColor = m_Scene.color;

        // 7) Custom pass
        {
            m_Backend->BindFramebuffer(m_Custom.fbo);
            m_Backend->SetViewport(0, 0, m_Custom.w, m_Custom.h);

            // Clear ID(color) + depth + stencil
            m_Backend->ClearColorDepthStencil(0.f, 0.f, 0.f, 0.f, 1.f, 0);

            DrawCustomDepthPass(viewCmd, viewMat, projDesc);
            m_Backend->UnbindFramebuffer();
        }

        // 8) Post-process
        if (view.bEnablePostProcess)
        {
            FFramePostParams frameParams{};
            frameParams.textures["u_CustomID"]    = m_Custom.color;
            frameParams.textures["u_CustomDepth"] = m_Custom.depth;
            frameParams.textures["u_SceneDepth"]  = m_Scene.depth;

            // REQUIRED for thickness in pixels
            frameParams.floats["u_InvW"] = 1.0f / float(sceneRT.w);
            frameParams.floats["u_InvH"] = 1.0f / float(sceneRT.h);

            // Selection target
            frameParams.floats["u_StencilRef01"] = float(m_Context.GetEditorSelectionState().selectionStencil) / 255.0f;

            finalColor = RunPostProcessChain(finalColor, sceneRT.w, sceneRT.h, view.postProfileId, frameParams);
        }

        // 9) Present this view
        if (view.targetFBO.IsValid())  // Editor/RT
            m_Backend->BindFramebuffer(view.targetFBO);
        else                           // Game = backbuffer
            m_Backend->UnbindFramebuffer();

        m_Backend->SetViewport(view.viewportX, view.viewportY, view.viewportW, view.viewportH);

        RShaderHandle presentShader = view.bApplyPostGamma
            ? m_PresentShader       // tone + gamma
            : m_LinearCopyShader;   // pure linear copy

        BlitFullscreen(presentShader, finalColor, view.viewportW, view.viewportH);
    }
}

void RendererSubsystem::EnsureFullscreenQuad()
{
    if (!m_FSQuad.IsValid())
    {
        // Fullscreen triangle: (x,y,z,u,v)
        std::vector<float> verts = {
            //    x     y     z     u    v
            -1.f, -1.f, 0.f,  0.f,  0.f,
             3.f, -1.f, 0.f,  2.f,  0.f,
            -1.f,  3.f, 0.f,  0.f,  2.f
        };

        RMesh m{};
        m.vertices     = std::move(verts);
        m.indices.clear();
        m.vertexStride = sizeof(float) * 5; // 3 pos + 2 uv
        m.bHasNormals  = false;
        m.bHasUVs      = true;
        m.bHasTangents = false;
        m_FSQuad = m_Backend->CreateMesh(m);
    }

    // Linear copy shader (no tone/gamma)
    if (!m_LinearCopyShader.IsValid())
    {
        RShader s{};
        s.vertexSource = R"(#version 330 core
            layout(location=0) in vec3 aPos;
            layout(location=2) in vec2 aUV;
            out vec2 vUV;
            void main()
            {
                vUV = aUV;
                gl_Position = vec4(aPos, 1.0);
            }
        )";

        s.fragmentSource = R"(#version 330 core
            in vec2 vUV;
            out vec4 FragColor;
            uniform sampler2D u_Input;

            void main()
            {
                // Pure linear copy, no tone/gamma
                FragColor = texture(u_Input, vUV);
            }
        )";

        m_LinearCopyShader = m_Backend->CreateShader(s);
    }

    // Present shader: tone map + gamma
    if (!m_PresentShader.IsValid())
    {
        RShader s{};
        s.vertexSource = R"(#version 330 core
            layout(location=0) in vec3 aPos;
            layout(location=2) in vec2 aUV;
            out vec2 vUV;
            void main()
            {
                vUV = aUV;
                gl_Position = vec4(aPos, 1.0);
            }
        )";

        s.fragmentSource = R"(
            #version 330 core
            in vec2 vUV;
            out vec4 FragColor;

            uniform sampler2D u_Input;

            void main()
            {
                vec3 color = texture(u_Input, vUV).rgb;

                // No tone map, just clamp for safety
                color = clamp(color, 0.0, 1.0);

                // Single gamma encode to sRGB
                color = pow(color, vec3(1.0 / 2.2));

                FragColor = vec4(color, 1.0);
            }
        )";

        m_PresentShader = m_Backend->CreateShader(s);
    }
}

void RendererSubsystem::BlitFullscreen(RShaderHandle sh, RTextureHandle inputTex, int w, int h)
{
    EnsureFullscreenQuad();

    RShaderHandle shaderToUse = sh; // must be valid
    if (!shaderToUse.IsValid())
        shaderToUse = m_LinearCopyShader; // safe fallback

    m_Backend->SetViewport(0, 0, w, h);
    m_Backend->BindShader(shaderToUse);
    m_Backend->SetUniformInt(shaderToUse, "u_Input", 0);
    m_Backend->BindTexture(inputTex, 0);
    m_Backend->SubmitMesh(m_FSQuad, shaderToUse, FMatrix4::Identity());
}

void RendererSubsystem::RebuildKernelsIfDirty(uint32_t profileId)
{
    if (!m_PPM) return;
    if (!m_PPM->IsDirtyAndClear(profileId)) return;

    std::unordered_map<std::string, FPassKernel> newKernels;

    for (const auto& pass : m_PPM->GetChain(profileId))
    {
        if (auto it = m_Kernels.find(pass.name); it != m_Kernels.end())
        {
            newKernels.emplace(pass.name, it->second); // Keep compiled kernel
        }
        else if (pass.name == "Outline")
        {
            EnsureOutlineShader(); // compiles once, stores m_OutlinePPShader
            FPassKernel k{};
            k.shader = m_OutlineShader;
            newKernels.emplace(pass.name, k);
            continue;
        }
        else if (pass.name == "FXAA")
        {
            EnsureFXAAShader();
            FPassKernel k{};
            k.shader = m_FXAAShader;
            newKernels.emplace(pass.name, k);
            continue;
        }
        else
        {
            EnsureFullscreenQuad();

            FPassKernel k{};
            k.shader = m_LinearCopyShader; // linear copy, NO gamma
            k.BindParams = nullptr; // can be specialized later

            newKernels.emplace(pass.name, k);
        }
    }

    m_Kernels.swap(newKernels);
}

FCoordAdapter RendererSubsystem::BuildCoordAdapter(const FBackendCoordDesc& d)
{
    FMatrix4 T = FMatrix4::Identity();

    // Build an Engine->Backend axis remap matrix.
    // The backend provides its basis axes (X/Y/Z) expressed in *engine* space,
    // and this matrix converts vectors/points from engine coordinates into
    // the backend coordinate convention (handedness + axis directions).
    T.SetBasisX(d.X);
    T.SetBasisY(d.Y);
    T.SetBasisZ(d.Z);

    FCoordAdapter adapter;
    adapter.EngineToBackend = T;
    adapter.BackendToEngine = T.Inverse();

    return adapter;
}

void RendererSubsystem::ApplyCamera(const RShaderHandle& shaderToUse, const FMatrix4& viewEngine, const FProjectionDesc& projDesc)
{
    // Convert from engine space to backend space
    const FMatrix4& E2B = m_CoordAdaptor.EngineToBackend;
    const FMatrix4& B2E = m_CoordAdaptor.BackendToEngine;

    // Engine view -> Backend view
    FMatrix4 viewBackend = E2B * viewEngine * B2E;

    FMatrix4 projBackend = m_Backend->BuildProjectionMatrix(projDesc);

    m_Backend->SetUniformMat4(shaderToUse, "u_View", viewBackend.ToFloat16());
    m_Backend->SetUniformMat4(shaderToUse, "u_Proj", projBackend.ToFloat16());
}

void RendererSubsystem::DrawMesh(const RMeshHandle& meshHandle, const RShaderHandle &shaderToUse, const FMatrix4 &modelEngine)
{
    const FMatrix4& E2B = m_CoordAdaptor.EngineToBackend;
    const FMatrix4& B2E = m_CoordAdaptor.BackendToEngine;

    const FMatrix4 modelBackend = E2B * modelEngine * B2E;

    m_Backend->SubmitMesh(meshHandle, shaderToUse, modelBackend);
}

void RendererSubsystem::SubmitDebugLineList_Internal(const FDebugVertex* verts,
                                                     uint32_t vertCount,
                                                     bool bDepthTest)
{
    if (!verts || vertCount < 2) return;

    EnsureDebugLineShader();

    m_Backend->SetCullMode(IRenderBackend::ECullMode::None);
    m_Backend->SetBlendState(true,
        IRenderBackend::EBlendFactor::SrcAlpha,
        IRenderBackend::EBlendFactor::OneMinusSrcAlpha);

    if (bDepthTest)
        m_Backend->SetDepthState(true, false, IRenderBackend::ECompareFunc::LessEqual);
    else
        m_Backend->SetDepthState(false, false, IRenderBackend::ECompareFunc::Always);

    // positions are WORLD SPACE, so we need camera uniforms
    ApplyCamera(m_DebugLineShader, m_ViewCache.viewEngine, m_ViewCache.projDesc);

    m_Backend->SubmitDebugLineList(m_DebugLineShader, verts, vertCount);
}

void* RendererSubsystem::GetNativeTextureHandle(RTextureHandle handle) const
{
    return m_Backend->GetNativeTextureHandle(handle);
}

void RendererSubsystem::SubmitDebugTriangles(const FRenderView& view,
                                             const FDebugTri* tris,
                                             uint32_t triCount)
{
    if (!tris || triCount == 0) return;

    EnsureDebugClipTriShader();
    EnsureDebugWorldTriShader();

    const FMatrix4& VP = m_ViewCache.vpBackend;

    // ---- Buckets ----
    std::vector<FDebugClipVertex> overlay_unlit;
    std::vector<FDebugClipVertex> depth_unlit;

    std::vector<FDebugWorldVertex> overlay_lit;
    std::vector<FDebugWorldVertex> depth_lit;

    overlay_unlit.reserve((size_t)triCount * 3);
    depth_unlit.reserve((size_t)triCount * 3);
    overlay_lit.reserve((size_t)triCount * 3);
    depth_lit.reserve((size_t)triCount * 3);

    auto pickClip = [&](EDebugDepthMode d) -> std::vector<FDebugClipVertex>& {
        return (d == EDebugDepthMode::Overlay) ? overlay_unlit : depth_unlit;
    };
    auto pickWorld = [&](EDebugDepthMode d) -> std::vector<FDebugWorldVertex>& {
        return (d == EDebugDepthMode::Overlay) ? overlay_lit : depth_lit;
    };

    auto pushClip = [&](std::vector<FDebugClipVertex>& out, const FVector3& p, const FVector4& c)
    {
        const FVector4 pE(p.x, p.y, p.z, 1.0f);
        const FVector4 cp = VP * pE;
        out.push_back(FDebugClipVertex{ cp.x, cp.y, cp.z, cp.w, c.x, c.y, c.z, c.w });
    };

    auto pushWorld = [&](std::vector<FDebugWorldVertex>& out, const FVector3& p, const FVector3& n, const FVector4& c)
    {
        // position to backend world
        const FVector4 pb4 = m_CoordAdaptor.EngineToBackend * FVector4(p.x, p.y, p.z, 1.0f);
        const FVector3 pB(pb4.x, pb4.y, pb4.z);

        // normal to backend world (w = 0 so translation is ignored)
        const FVector4 nb4 = m_CoordAdaptor.EngineToBackend * FVector4(n.x, n.y, n.z, 0.0f);
        FVector3 nB(nb4.x, nb4.y, nb4.z);
        const float len = nB.Length();
        if (len > 1e-6f) nB = nB / len;

        out.push_back(FDebugWorldVertex{
            pB.x, pB.y, pB.z,
            nB.x, nB.y, nB.z,
            c.x, c.y, c.z, c.w
        });
    };

    // Build per-tri normals (flat shading)
    auto triNormal = [&](const FVector3& a, const FVector3& b, const FVector3& c) -> FVector3
    {
        const FVector3 n = (b - a).Cross(c - a);
        const float len = n.Length();
        return (len > 1e-6f) ? (n / len) : FVector3(0,0,1);
    };

    auto ToBackendPos = [&](const FVector3& pE) -> FVector3
    {
        const FVector4 p4 = m_CoordAdaptor.EngineToBackend * FVector4(pE.x, pE.y, pE.z, 1.0f);
        return FVector3(p4.x, p4.y, p4.z);
    };

    auto ToBackendNrm = [&](const FVector3& nE) -> FVector3
    {
        const FVector4 n4 = m_CoordAdaptor.EngineToBackend * FVector4(nE.x, nE.y, nE.z, 0.0f);
        FVector3 nB(n4.x, n4.y, n4.z);
        const float len = nB.Length();
        return (len > 1e-6f) ? (nB / len) : FVector3(0,0,1);
    };

    auto PushWorldBackend = [&](std::vector<FDebugWorldVertex>& out,
                               const FVector3& pB, const FVector3& nB,
                               const FVector4& c)
    {
        out.push_back(FDebugWorldVertex{
            pB.x, pB.y, pB.z,
            nB.x, nB.y, nB.z,
            c.x, c.y, c.z, c.w
        });
    };

    auto HasNormal = [](const FVector3& n) { return n.Length() > 1e-6f; };

    for (uint32_t i = 0; i < triCount; ++i)
    {
        const FDebugTri& t = tris[i];

        if (t.style.fill == EDebugFillMode::Wireframe)
            continue; // wire comes from lines

        if (t.style.shading == EDebugShading::FixedLit)
        {
            auto& out = pickWorld(t.style.depth);

            const FVector3 aB = ToBackendPos(t.a);
            const FVector3 bB = ToBackendPos(t.b);
            const FVector3 cB = ToBackendPos(t.c);

            FVector3 nFlatB = (bB - aB).Cross(cB - aB);
            const float lf = nFlatB.Length();
            nFlatB = (lf > 1e-6f) ? (nFlatB / lf) : FVector3(0,0,1);

            const bool wantSmooth = (t.style.normalMode == EDebugNormalMode::Smooth);
            const bool hasSmooth  = HasNormal(t.na) && HasNormal(t.nb) && HasNormal(t.nc);

            if (wantSmooth && hasSmooth)
            {
                FVector3 naB = ToBackendNrm(t.na);
                FVector3 nbB = ToBackendNrm(t.nb);
                FVector3 ncB = ToBackendNrm(t.nc);

                // CRITICAL: keep smooth normals consistent with the triangle facing
                // (handles LH/RH reflection without any global "flip sign")
                const float da = naB.Dot(nFlatB);
                const float db = nbB.Dot(nFlatB);
                const float dc = ncB.Dot(nFlatB);
                const float dAvg = (da + db + dc) * (1.0f / 3.0f);
                if (dAvg < 0.0f) { naB = -naB; nbB = -nbB; ncB = -ncB; }

                PushWorldBackend(out, aB, naB, t.color);
                PushWorldBackend(out, bB, nbB, t.color);
                PushWorldBackend(out, cB, ncB, t.color);
            }
            else
            {
                // flat (same as before)
                PushWorldBackend(out, aB, nFlatB, t.color);
                PushWorldBackend(out, bB, nFlatB, t.color);
                PushWorldBackend(out, cB, nFlatB, t.color);
            }
        }
        else
        {
            auto& out = pickClip(t.style.depth);
            pushClip(out, t.a, t.color);
            pushClip(out, t.b, t.color);
            pushClip(out, t.c, t.color);
        }
    }

    // ---- Submit UNLIT ----
    if (!overlay_unlit.empty())
        SubmitDebugClipTriList_Internal(view, overlay_unlit.data(), (uint32_t)overlay_unlit.size(), false);

    if (!depth_unlit.empty())
        SubmitDebugClipTriList_Internal(view, depth_unlit.data(), (uint32_t)depth_unlit.size(), true);

    // ---- Submit FIXED-LIT  ----
    if (!overlay_lit.empty())
        SubmitDebugWorldTriList_Internal(view, overlay_lit.data(), (uint32_t)overlay_lit.size(), false);

    if (!depth_lit.empty())
        SubmitDebugWorldTriList_Internal(view, depth_lit.data(), (uint32_t)depth_lit.size(), true);
}

void RendererSubsystem::SubmitDebugLines(const FRenderView& view, const FDebugLine* lines, uint32_t lineCount)
{
    if (!lines || lineCount == 0) return;

    // Buckets: thin = world-pos lines, thick = clip-pos triangles
    std::vector<FDebugVertex> thinOverlay;
    std::vector<FDebugVertex> thinDepth;
    std::vector<FDebugClipVertex> thickOverlay;
    std::vector<FDebugClipVertex> thickDepth;

    thinOverlay.reserve(lineCount * 2);
    thinDepth.reserve(lineCount * 2);
    thickOverlay.reserve(lineCount * 6);
    thickDepth.reserve(lineCount * 6);

    auto pick = [&](EDebugDepthMode d, auto& overlayBuf, auto& depthBuf) -> auto&
    {
        return (d == EDebugDepthMode::Overlay) ? overlayBuf : depthBuf;
    };

    auto pushThin = [&](std::vector<FDebugVertex>& out, const FVector3& a, const FVector3& b, const FVector4& c)
    {
        const FVector4 ab = m_CoordAdaptor.EngineToBackend * FVector4(a.x, a.y, a.z, 1.0f);
        const FVector4 bb = m_CoordAdaptor.EngineToBackend * FVector4(b.x, b.y, b.z, 1.0f);

        out.push_back(FDebugVertex{a.x, a.y, a.z, c.x, c.y, c.z, c.w});
        out.push_back(FDebugVertex{b.x, b.y, b.z, c.x, c.y, c.z, c.w});
    };


    auto pushTri = [&](std::vector<FDebugClipVertex>& out,
                       const FVector4& p0, const FVector4& p1, const FVector4& p2,
                       const FVector4& c)
    {
        out.push_back(FDebugClipVertex{p0.x, p0.y, p0.z, p0.w, c.x, c.y, c.z, c.w});
        out.push_back(FDebugClipVertex{p1.x, p1.y, p1.z, p1.w, c.x, c.y, c.z, c.w});
        out.push_back(FDebugClipVertex{p2.x, p2.y, p2.z, p2.w, c.x, c.y, c.z, c.w});
    };

    const FMatrix4& VP = m_ViewCache.vpBackend;

    const float invW = (view.viewportW > 0) ? (1.0f / float(view.viewportW)) : 0.0f;
    const float invH = (view.viewportH > 0) ? (1.0f / float(view.viewportH)) : 0.0f;

    auto emitThick = [&](std::vector<FDebugClipVertex>& out,
                         const FVector3& a, const FVector3& b,
                         const FVector4& color,
                         float thicknessPx)
    {
        if (thicknessPx <= 1.0f || invW <= 0.0f || invH <= 0.0f)
            return false;

        const FVector4 ca = VP * FVector4(a.x,a.y,a.z,1);
        const FVector4 cb = VP * FVector4(b.x,b.y,b.z,1);


        // If both behind camera, skip. (Simple; avoids worst artifacts.)
        if (ca.w <= 0.0f && cb.w <= 0.0f)
            return false;

        // NDC
        const float ax = ca.x / ca.w;
        const float ay = ca.y / ca.w;
        const float az = ca.z / ca.w;

        const float bx = cb.x / cb.w;
        const float by = cb.y / cb.w;
        const float bz = cb.z / cb.w;

        float dx = bx - ax;
        float dy = by - ay;

        const float len2 = dx*dx + dy*dy;
        if (len2 <= 1e-12f)
            return false;

        const float invLen = 1.0f / std::sqrt(len2);
        dx *= invLen;
        dy *= invLen;

        // Perp in NDC
        float px = -dy;
        float py =  dx;

        // Half thickness in NDC units
        const float half = thicknessPx * 0.5f;
        const float offX = px * (half * 2.0f * invW);
        const float offY = py * (half * 2.0f * invH);

        // Build 4 corners in clip-space by offsetting x/y and re-multiplying by w
        const FVector4 a0((ax + offX) * ca.w, (ay + offY) * ca.w, az * ca.w, ca.w);
        const FVector4 a1((ax - offX) * ca.w, (ay - offY) * ca.w, az * ca.w, ca.w);
        const FVector4 b0((bx + offX) * cb.w, (by + offY) * cb.w, bz * cb.w, cb.w);
        const FVector4 b1((bx - offX) * cb.w, (by - offY) * cb.w, bz * cb.w, cb.w);

        // Two triangles: a0-b0-b1 and a0-b1-a1
        pushTri(out, a0, b0, b1, color);
        pushTri(out, a0, b1, a1, color);
        return true;
    };

    for (uint32_t i = 0; i < lineCount; ++i)
    {
        const FDebugLine& ln = lines[i];

        // Route once; no duplicated overlay/depth logic
        if (ln.style.thicknessPx > 1.0f)
        {
            auto& out = pick(ln.style.depth, thickOverlay, thickDepth);
            const bool ok = emitThick(out, ln.a, ln.b, ln.color, ln.style.thicknessPx);
            if (!ok)
            {
                // fallback to thin if thick failed (degenerate, etc.)
                auto& t = pick(ln.style.depth, thinOverlay, thinDepth);
                pushThin(t, ln.a, ln.b, ln.color);
            }
        }
        else
        {
            auto& out = pick(ln.style.depth, thinOverlay, thinDepth);
            pushThin(out, ln.a, ln.b, ln.color);
        }
    }

    // Submit thin
    if (!thinOverlay.empty())
        SubmitDebugLineList_Internal(thinOverlay.data(), (uint32_t)thinOverlay.size(), /*bDepthTest=*/false);
    if (!thinDepth.empty())
        SubmitDebugLineList_Internal(thinDepth.data(), (uint32_t)thinDepth.size(), /*bDepthTest=*/true);

    // Submit thick
    if (!thickOverlay.empty())
        SubmitDebugClipTriList_Internal(view, thickOverlay.data(), (uint32_t)thickOverlay.size(), /*bDepthTest=*/false);
    if (!thickDepth.empty())
        SubmitDebugClipTriList_Internal(view, thickDepth.data(), (uint32_t)thickDepth.size(), /*bDepthTest=*/true);
}

void RendererSubsystem::SubmitDebugClipTriList_Internal(const FRenderView& view, const FDebugClipVertex* verts, uint32_t vertCount,
                                               bool bDepthTest)
{
    if (!verts || vertCount == 0) return;

    EnsureDebugClipTriShader();

    // state
    m_Backend->SetCullMode(IRenderBackend::ECullMode::None);
    m_Backend->SetBlendState(true,
        IRenderBackend::EBlendFactor::SrcAlpha,
        IRenderBackend::EBlendFactor::OneMinusSrcAlpha);

    if (bDepthTest)
        m_Backend->SetDepthState(true, false, IRenderBackend::ECompareFunc::LessEqual);
    else
        m_Backend->SetDepthState(false, false, IRenderBackend::ECompareFunc::Always);

    // NOTE: no camera uniforms needed; positions already clip-space
    m_Backend->BindShader(m_DebugClipTriShader);
    m_Backend->SubmitDebugClipTriList(m_DebugClipTriShader, verts, vertCount);
}

void RendererSubsystem::SubmitDebugWorldTriList_Internal(const FRenderView& view,
                                                         const FDebugWorldVertex* verts,
                                                         uint32_t vertCount,
                                                         bool bDepthTest)
{
    if (!verts || vertCount == 0) return;

    EnsureDebugWorldTriShader();

    m_Backend->SetCullMode(IRenderBackend::ECullMode::Back);
    m_Backend->SetBlendState(true,
        IRenderBackend::EBlendFactor::SrcAlpha,
        IRenderBackend::EBlendFactor::OneMinusSrcAlpha);

    if (bDepthTest)
        m_Backend->SetDepthState(true, true, IRenderBackend::ECompareFunc::LessEqual);
    else
        m_Backend->SetDepthState(false, false, IRenderBackend::ECompareFunc::Always);

    // uniforms
    ApplyCamera(m_DebugWorldTriShader, m_ViewCache.viewEngine, m_ViewCache.projDesc);

    // stable “editor light” in view space
    m_Backend->SetUniformVec3(m_DebugWorldTriShader, "u_LightDirVS", FVector3(-0.35f, 0.65f, 0.70f).Normalized().ToFloat3());
    m_Backend->SetUniformFloat(m_DebugWorldTriShader, "u_Ambient", 0.35f);
    m_Backend->SetUniformFloat(m_DebugWorldTriShader, "u_Rim", 0.05f);
    m_Backend->SetUniformFloat(m_DebugWorldTriShader, "u_Spec", 0.18f);

    m_Backend->SubmitDebugWorldTriList(m_DebugWorldTriShader, verts, vertCount);
}

RMeshHandle RendererSubsystem::CreateMesh(const RMesh &data)
{
    return m_Backend->CreateMesh(data);
}

void RendererSubsystem::DestroyMesh(RMeshHandle h)
{
    m_Backend->DestroyMesh(h);
}

RTextureHandle RendererSubsystem::CreateTexture(const RTexture &data)
{
    return m_Backend->CreateTexture(data);
}

void RendererSubsystem::DestroyTexture(RTextureHandle h)
{
    m_Backend->DestroyTexture(h);
}

RTextureHandle RendererSubsystem::CreateTextureFromRenderTargetRegion(RRenderTargetHandle source, int x, int y,
    int width, int height)
{
    if (!m_Backend || !source.IsValid() ||
        x < 0 || y < 0 || width <= 0 || height <= 0)
        return {};

    const auto it = m_RenderTargets.find(source.id);

    if (it == m_RenderTargets.end())
        return {};

    const FTarget& target = it->second;

    if (x > target.w || y > target.h ||
        width > target.w - x ||
        height > target.h - y)
        return {};

    // Convert top-left UI coordinates to framebuffer coordinates.
    const int sourceY = target.h - y - height;

    return m_Backend->CreateTextureFromFramebufferRegion(
        target.fbo,
        x,
        sourceY,
        width,
        height);
}

RRenderTargetHandle RendererSubsystem::CreateRenderTarget(const FRenderTargetDesc& desc)
{
    if (!m_Backend || !desc.IsValid() || m_NextRenderTargetId <= 0)
        return {};

    bool bHDR = false;
    bool bSRGB = false;

    switch (desc.colorFormat)
    {
        case ERenderTargetColorFormat::RGBA8:
            break;

        case ERenderTargetColorFormat::RGBA16F:
            bHDR = true;
            break;

        case ERenderTargetColorFormat::SRGBA8:
            bSRGB = true;
            break;

        default:
            return {};
    }

    FTarget target{};

    BuildTarget(target, desc.width, desc.height, 1,
        desc.bHasDepthStencil, bHDR, bSRGB);

    if (!target.fbo.IsValid() || !target.color.IsValid())
    {
        DestroyTarget(target);
        return {};
    }

    const Rint id = m_NextRenderTargetId;

    m_NextRenderTargetId = id == std::numeric_limits<Rint>::max()
        ? 0
        : id + 1;

    if (!m_RenderTargets.emplace(id, target).second)
    {
        DestroyTarget(target);
        return {};
    }

    return { id };
}

void RendererSubsystem::DestroyRenderTarget(RRenderTargetHandle target)
{
    if (!target.IsValid())
        return;

    const auto it = m_RenderTargets.find(target.id);

    if (it == m_RenderTargets.end())
        return;

    // A target cannot be destroyed while it is active or saved
    // in the render-target nesting hierarchy.
    if (std::find(m_RenderTargetStack.begin(), m_RenderTargetStack.end(), target) !=
        m_RenderTargetStack.end())
        return;

    DestroyTarget(it->second);
    m_RenderTargets.erase(it);
}

RTextureHandle RendererSubsystem::GetRenderTargetColorTexture(
    RRenderTargetHandle target) const
{
    if (!target.IsValid())
        return RTextureHandle::Invalid();

    const auto it = m_RenderTargets.find(target.id);

    if (it == m_RenderTargets.end())
        return RTextureHandle::Invalid();

    return it->second.color;
}

bool RendererSubsystem::PushRenderTarget(RRenderTargetHandle target)
{
    if (!target.IsValid() || !m_Backend)
        return false;

    const auto it = m_RenderTargets.find(target.id);

    if (it == m_RenderTargets.end() || !it->second.fbo.IsValid())
        return false;

    m_RenderTargetStack.push_back(target);

    if (!m_Backend->PushFramebuffer(it->second.fbo))
    {
        m_RenderTargetStack.pop_back();
        return false;
    }

    return true;
}

bool RendererSubsystem::PopRenderTarget()
{
    if (!m_Backend || m_RenderTargetStack.empty())
        return false;

    if (!m_Backend->PopFramebuffer())
        return false;

    m_RenderTargetStack.pop_back();
    return true;
}

bool RendererSubsystem::ClearRenderTarget(RRenderTargetHandle target, const FRenderClearDesc& desc)
{
    if (!target.IsValid() || !m_Backend || !desc.IsValid())
        return false;

    const auto it = m_RenderTargets.find(target.id);

    if (it == m_RenderTargets.end())
        return false;

    const FTarget& renderTarget = it->second;

    if ((desc.bClearDepth || desc.bClearStencil) &&
        !renderTarget.bHasDepthStencil)
        return false;

    if (!PushRenderTarget(target))
        return false;

    m_Backend->ClearFramebuffer(desc);

    return PopRenderTarget();
}

bool RendererSubsystem::CompositeRenderTargets(RRenderTargetHandle source, RRenderTargetHandle destination,
    ERenderTargetBlendMode blendMode)
{
    if (!m_Backend || !source.IsValid())
        return false;

    const auto sourceIt = m_RenderTargets.find(source.id);

    if (sourceIt == m_RenderTargets.end() || !sourceIt->second.color.IsValid())
        return false;

    if (destination.IsValid() && !m_RenderTargets.contains(destination.id))
        return false;

    switch (blendMode)
    {
        case ERenderTargetBlendMode::Blend:
        case ERenderTargetBlendMode::Replace:
            break;

        default:
            return false;
    }

    const RTextureHandle sourceTexture = sourceIt->second.color;
    const int sourceWidth = sourceIt->second.w;
    const int sourceHeight = sourceIt->second.h;

    if (!m_Backend->PushRenderState())
        return false;

    RRenderTargetHandle temporaryTarget{};

    // Sampling from a texture while rendering into that same texture
    // creates a framebuffer feedback loop. Copy it into a temporary target.
    if (source == destination)
    {
        FRenderTargetDesc desc{};
        desc.width = sourceWidth;
        desc.height = sourceHeight;
        desc.colorFormat = ERenderTargetColorFormat::RGBA16F;
        desc.bHasDepthStencil = false;

        temporaryTarget = CreateRenderTarget(desc);

        if (!temporaryTarget.IsValid())
        {
            m_Backend->PopRenderState();
            return false;
        }
    }

    auto compositeInto = [&](RTextureHandle texture,
                             RRenderTargetHandle target,
                             ERenderTargetBlendMode mode) -> bool
    {
        std::vector<RRenderTargetHandle> suspendedTargets;
        bool bPushedDestination = false;

        bool bDestinationReady = false;

        if (!target.IsValid())
        {
            bDestinationReady = true;

            while (!m_RenderTargetStack.empty())
            {
                const RRenderTargetHandle activeTarget = m_RenderTargetStack.back();

                if (!PopRenderTarget())
                {
                    bDestinationReady = false;
                    break;
                }

                suspendedTargets.push_back(activeTarget);
            }

            bDestinationReady = bDestinationReady && m_RenderTargetStack.empty();
        }
        else
        {
            const bool bTargetAlreadyActive =
                std::find(m_RenderTargetStack.begin(),
                          m_RenderTargetStack.end(),
                          target) != m_RenderTargetStack.end();

            if (bTargetAlreadyActive)
            {
                bDestinationReady = true;

                while (!m_RenderTargetStack.empty() &&
                       m_RenderTargetStack.back() != target)
                {
                    const RRenderTargetHandle activeTarget = m_RenderTargetStack.back();

                    if (!PopRenderTarget())
                    {
                        bDestinationReady = false;
                        break;
                    }

                    suspendedTargets.push_back(activeTarget);
                }

                bDestinationReady = bDestinationReady &&
                    !m_RenderTargetStack.empty() &&
                    m_RenderTargetStack.back() == target;
            }
            else
            {
                bDestinationReady = PushRenderTarget(target);
                bPushedDestination = bDestinationReady;
            }
        }

        auto restoreTargets = [&]() -> bool
        {
            bool bRestored = true;

            if (bPushedDestination && !PopRenderTarget())
                bRestored = false;

            for (auto it = suspendedTargets.rbegin();
                 it != suspendedTargets.rend(); ++it)
            {
                if (!PushRenderTarget(*it))
                {
                    bRestored = false;
                    break;
                }
            }

            return bRestored;
        };

        if (!bDestinationReady)
        {
            restoreTargets();
            return false;
        }

        m_Backend->SetCullMode(IRenderBackend::ECullMode::None);
        m_Backend->SetDepthState(
            false, false, IRenderBackend::ECompareFunc::Always);
        m_Backend->SetColorWriteMask(true, true, true, true);

        switch (mode)
        {
            case ERenderTargetBlendMode::Blend:
                m_Backend->SetBlendState(
                    true,
                    IRenderBackend::EBlendFactor::One,
                    IRenderBackend::EBlendFactor::OneMinusSrcAlpha);
                break;

            case ERenderTargetBlendMode::Replace:
                m_Backend->SetBlendState(
                    false,
                    IRenderBackend::EBlendFactor::One,
                    IRenderBackend::EBlendFactor::Zero);
                break;

            default:
                restoreTargets();
                return false;
        }

        // Deliberately preserve the active scissor and stencil tests.
        const bool bDrawSucceeded = DrawFullscreenTexture(texture);
        const bool bRestoreSucceeded = restoreTargets();

        return bDrawSucceeded && bRestoreSucceeded;
    };

    bool bSucceeded = false;

    if (temporaryTarget.IsValid())
    {
        const RTextureHandle temporaryTexture =
            GetRenderTargetColorTexture(temporaryTarget);

        if (temporaryTexture.IsValid())
        {
            bSucceeded = compositeInto(
                sourceTexture,
                temporaryTarget,
                ERenderTargetBlendMode::Replace);

            if (bSucceeded)
                bSucceeded = compositeInto(
                    temporaryTexture,
                    destination,
                    blendMode);
        }

        DestroyRenderTarget(temporaryTarget);
    }
    else
    {
        bSucceeded = compositeInto(sourceTexture, destination, blendMode);
    }

    const bool bStateRestored = m_Backend->PopRenderState();

    return bSucceeded && bStateRestored;
}

bool RendererSubsystem::ApplyFilter(
    RRenderTargetHandle source,
    RRenderTargetHandle output,
    const FRenderTargetFilterDesc& filter,
    const FRenderTargetRegion& region)
{
    if (!m_Backend || !source.IsValid() || !output.IsValid() || source == output)
        return false;

    const auto sourceIt = m_RenderTargets.find(source.id);
    const auto outputIt = m_RenderTargets.find(output.id);

    if (sourceIt == m_RenderTargets.end() || outputIt == m_RenderTargets.end())
        return false;

    const FTarget& sourceTarget = sourceIt->second;
    const FTarget& outputTarget = outputIt->second;

    const int width = sourceTarget.w;
    const int height = sourceTarget.h;

    const RTextureHandle sourceTexture = sourceTarget.color;

    if (!sourceTexture.IsValid() || !outputTarget.color.IsValid())
        return false;

    if (width != outputTarget.w || height != outputTarget.h)
        return false;

    if (!region.IsValid() ||
        region.width > width ||
        region.height > height ||
        region.x > width - region.width ||
        region.y > height - region.height)
        return false;

    switch (filter.type)
    {
        case ERenderTargetFilterType::GaussianBlur:
            if (!std::isfinite(filter.sigma) || filter.sigma < 0.0f)
                return false;
            break;

        default:
            return false;
    }

    // The output cannot be modified while it is already active.
    if (std::find(
        m_RenderTargetStack.begin(),
        m_RenderTargetStack.end(),
        output) != m_RenderTargetStack.end())
        return false;

    if (!EnsureFilterScratchTarget(width, height))
        return false;

    const RRenderTargetHandle scratch = m_FilterScratchTarget;

    if (scratch == source || scratch == output)
        return false;

    // EnsureFilterScratchTarget may insert into m_RenderTargets,
    // potentially invalidating iterators and references through rehashing.
    const auto outputAfterScratchIt = m_RenderTargets.find(output.id);
    const auto scratchIt = m_RenderTargets.find(scratch.id);

    if (outputAfterScratchIt == m_RenderTargets.end() ||
        scratchIt == m_RenderTargets.end())
        return false;

    const RTextureHandle outputTexture = outputAfterScratchIt->second.color;
    const RTextureHandle scratchTexture = scratchIt->second.color;

    if (!outputTexture.IsValid() || !scratchTexture.IsValid())
        return false;

    if (scratchIt->second.bHasDepthStencil)
        return false;

    EnsureFullscreenQuad();
    EnsureGaussianBlurShader();

    if (!m_FSQuad.IsValid() || !m_GaussianBlurShader.IsValid())
        return false;

    if (!m_Backend->PushRenderState())
        return false;

    // Intermediate processing must not inherit UI clipping.
    m_Backend->EnableScissorRegion(false);

    IRenderBackend::FStencilState disabledStencil{};
    disabledStencil.bEnable = false;
    m_Backend->SetStencilState(disabledStencil);

    // Seed output with the original source image.
    bool succeeded = CompositeRenderTargets(
        source,
        output,
        ERenderTargetBlendMode::Replace);

    const float inverseWidth = 1.0f / static_cast<float>(width);
    const float inverseHeight = 1.0f / static_cast<float>(height);

    // Convert the top-left UI region to bottom-left texture coordinates.
    const int bottom = height - region.y - region.height;

    const float regionMin[] = {
        (static_cast<float>(region.x) + 0.5f) * inverseWidth,
        (static_cast<float>(bottom) + 0.5f) * inverseHeight
    };

    const float regionMax[] = {
        (static_cast<float>(region.x + region.width) - 0.5f) * inverseWidth,
        (static_cast<float>(bottom + region.height) - 0.5f) * inverseHeight
    };

    auto drawBlurPass = [&](RTextureHandle input,
                            RRenderTargetHandle destination,
                            float directionX,
                            float directionY) -> bool
    {
        if (!PushRenderTarget(destination))
            return false;

        m_Backend->SetCullMode(IRenderBackend::ECullMode::None);

        m_Backend->SetDepthState(
            false,
            false,
            IRenderBackend::ECompareFunc::Always);

        m_Backend->SetBlendState(
            false,
            IRenderBackend::EBlendFactor::One,
            IRenderBackend::EBlendFactor::Zero);

        m_Backend->SetColorWriteMask(true, true, true, true);

        m_Backend->BindShader(m_GaussianBlurShader);

        m_Backend->SetUniformInt(m_GaussianBlurShader, "u_Input", 0);
        m_Backend->SetUniformFloat(m_GaussianBlurShader, "u_Sigma", filter.sigma);

        const float texelOffset[] = {
            directionX * inverseWidth,
            directionY * inverseHeight
        };

        m_Backend->SetUniformVec2(
            m_GaussianBlurShader,
            "u_TexelOffset",
            texelOffset);

        m_Backend->SetUniformVec2(
            m_GaussianBlurShader,
            "u_TexCoordMin",
            regionMin);

        m_Backend->SetUniformVec2(
            m_GaussianBlurShader,
            "u_TexCoordMax",
            regionMax);

        m_Backend->BindTexture(input, 0);

        m_Backend->SubmitMesh(
            m_FSQuad,
            m_GaussianBlurShader,
            FMatrix4::Identity());

        return PopRenderTarget();
    };

    // Pass 1: vertical blur, output -> scratch.
    if (succeeded)
    {
        succeeded = drawBlurPass(
            outputTexture,
            scratch,
            0.0f,
            1.0f);
    }

    // Pass 2: horizontal blur, scratch -> output.
    if (succeeded)
    {
        succeeded = drawBlurPass(
            scratchTexture,
            output,
            1.0f,
            0.0f);
    }

    const bool stateRestored = m_Backend->PopRenderState();

    return succeeded && stateRestored;
}

void RendererSubsystem::SubmitScreenSpaceGeometry(RMeshHandle mesh, RTextureHandle texture, float translationX,
                                                  float translationY, int viewportWidth, int viewportHeight, bool bWriteColor,
                                                  const float* transform)
{
    if (!mesh.IsValid() || viewportWidth <= 0 || viewportHeight <= 0)
        return;

    EnsureUIGeometryShader();

    if (!m_UIGeometryShader.IsValid())
        return;

    m_Backend->SetCullMode(IRenderBackend::ECullMode::None);
    m_Backend->SetDepthState(false, false, IRenderBackend::ECompareFunc::Always);

    m_Backend->SetBlendState(
        true,
        IRenderBackend::EBlendFactor::One,
        IRenderBackend::EBlendFactor::OneMinusSrcAlpha);

    m_Backend->SetColorWriteMask(bWriteColor, bWriteColor, bWriteColor, bWriteColor);
    m_Backend->BindShader(m_UIGeometryShader);

    const float translation[] = { translationX, translationY };

    static constexpr float identityTransform[16] = {
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f
    };

    m_Backend->SetUniformVec2(m_UIGeometryShader, "u_Translation", translation);

    m_Backend->SetUniformMat4(m_UIGeometryShader, "u_Transform", transform ? transform : identityTransform);

    m_Backend->SetUniformInt(m_UIGeometryShader, "u_Texture", 0);

    const bool bUseTexture = texture.IsValid();
    m_Backend->SetUniformInt(m_UIGeometryShader, "u_UseTexture", bUseTexture ? 1 : 0);

    if (bUseTexture)
        m_Backend->BindTexture(texture, 0);

    m_Backend->SubmitMesh(mesh, m_UIGeometryShader, FMatrix4::Identity());
    m_Backend->SetColorWriteMask(true, true, true, true); // Restore color writes
}

void RendererSubsystem::SubmitScreenSpaceGeometry(
    RMeshHandle mesh,
    RTextureHandle texture,
    float translationX,
    float translationY,
    int viewportWidth,
    int viewportHeight,
    bool bWriteColor,
    const float* transform)
{
    if (!mesh.IsValid() || viewportWidth <= 0 || viewportHeight <= 0)
        return;

    EnsureUIGeometryShader();

    if (!m_UIGeometryShader.IsValid())
        return;

    SubmitScreenSpaceGeometryWithShader(
        mesh,
        m_UIGeometryShader,
        texture,
        translationX,
        translationY,
        viewportWidth,
        viewportHeight,
        bWriteColor,
        transform,
        {});
}

void RendererSubsystem::SubmitScreenSpaceGeometryWithShader(
    RMeshHandle mesh, RShaderHandle shader,
    RTextureHandle texture,
    float translationX, float translationY,
    int viewportWidth, int viewportHeight,
    bool bWriteColor,
    const float* transform,
    std::span<const FScreenSpaceShaderUniform> uniforms)
{
    if (!m_Backend ||
        !mesh.IsValid() ||
        !shader.IsValid() ||
        viewportWidth <= 0 ||
        viewportHeight <= 0)
        return;

    m_Backend->SetCullMode(IRenderBackend::ECullMode::None);
    m_Backend->SetDepthState(false, false, IRenderBackend::ECompareFunc::Always);

    m_Backend->SetBlendState(
        true,
        IRenderBackend::EBlendFactor::One,
        IRenderBackend::EBlendFactor::OneMinusSrcAlpha);

    m_Backend->SetColorWriteMask(bWriteColor, bWriteColor, bWriteColor, bWriteColor);
    m_Backend->BindShader(shader);

    const float translation[] = { translationX, translationY };

    static constexpr float identityTransform[16] = {
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f
    };

    m_Backend->SetUniformVec2(shader, "u_Translation", translation);
    m_Backend->SetUniformMat4(shader, "u_Transform", transform ? transform : identityTransform);

    m_Backend->SetUniformInt(shader, "u_Texture", 0);
    m_Backend->SetUniformInt(shader, "u_UseTexture", texture.IsValid() ? 1 : 0);

    if (texture.IsValid())
        m_Backend->BindTexture(texture, 0);

    for (const FScreenSpaceShaderUniform& uniform : uniforms)
    {
        if (uniform.name.empty())
            continue;

        // These are reserved by the screen-space submission contract.
        if (uniform.name == "u_Translation" ||
            uniform.name == "u_Transform" ||
            uniform.name == "u_Texture" ||
            uniform.name == "u_UseTexture")
            continue;

        std::visit(
            [&](const auto& value)
            {
                using TValue = std::decay_t<decltype(value)>;

                if constexpr (std::is_same_v<TValue, bool>)
                    m_Backend->SetUniformInt(shader, uniform.name.c_str(), value ? 1 : 0);
                else if constexpr (std::is_same_v<TValue, int>)
                    m_Backend->SetUniformInt(shader, uniform.name.c_str(), value);
                else if constexpr (std::is_same_v<TValue, float>)
                    m_Backend->SetUniformFloat(shader, uniform.name.c_str(), value);
                else if constexpr (std::is_same_v<TValue, std::array<float, 2>>)
                    m_Backend->SetUniformVec2(shader, uniform.name.c_str(), value.data());
                else if constexpr (std::is_same_v<TValue, std::array<float, 3>>)
                    m_Backend->SetUniformVec3(shader, uniform.name.c_str(), value.data());
                else if constexpr (std::is_same_v<TValue, std::array<float, 4>>)
                    m_Backend->SetUniformVec4(shader, uniform.name.c_str(), value.data());
                else if constexpr (std::is_same_v<TValue, std::array<float, 16>>)
                    m_Backend->SetUniformMat4(shader, uniform.name.c_str(), value.data());
            },
            uniform.value);
    }

    m_Backend->SubmitMesh(mesh, shader, FMatrix4::Identity());

    // Preserve the existing UI submission behavior.
    m_Backend->SetColorWriteMask(true, true, true, true);
}

void RendererSubsystem::EnableScissorRegion(bool bEnable)
{
    m_Backend->EnableScissorRegion(bEnable);
}

void RendererSubsystem::SetScissorRegion(int x, int y, int width, int height)
{
    m_Backend->SetScissorRegion(x, y, width, height);
}

void RendererSubsystem::ClearStencilBuffer(std::uint8_t value)
{
    m_Backend->ClearStencilBuffer(value);
}

void RendererSubsystem::SetClipMaskStencilState(const FClipMaskStencilState& state)
{
    if (!m_Backend)
        return;

    IRenderBackend::FStencilState stencil{};
    stencil.bEnable = state.bEnable;

    if (!state.bEnable)
    {
        m_Backend->SetStencilState(stencil);
        return;
    }

    const bool bReplacesMask = state.bWrite &&
    state.operation != EClipMaskStencilOperation::Increment;

    stencil.func = bReplacesMask
        ? IRenderBackend::ECompareFunc::Always
        : IRenderBackend::ECompareFunc::Equal;

    stencil.ref = state.reference;
    stencil.readMask = 0xFF;
    stencil.writeMask = state.bWrite ? 0xFF : 0x00;
    stencil.sfail = IRenderBackend::EStencilOp::Keep;
    stencil.zfail = IRenderBackend::EStencilOp::Keep;

    switch (state.operation)
    {
        case EClipMaskStencilOperation::Keep:
            stencil.zpass = IRenderBackend::EStencilOp::Keep;
            break;

        case EClipMaskStencilOperation::Replace:
            stencil.zpass = IRenderBackend::EStencilOp::Replace;
            break;

        case EClipMaskStencilOperation::Increment:
            stencil.zpass = IRenderBackend::EStencilOp::Incr;
            break;
    }

    m_Backend->SetStencilState(stencil);
}

RShaderHandle RendererSubsystem::CreateShader(const RShader &data)
{
    return m_Backend->CreateShader(data);
}

void RendererSubsystem::DestroyShader(RShaderHandle h)
{
    m_Backend->DestroyShader(h);
}

RMaterialHandle RendererSubsystem::CreateMaterial(const FSurfaceDesc &surface)
{
    // Generate a new handle id
    const Rint id = m_NextMaterialID++;
    m_Materials.emplace(id, FMaterialEntry{ surface });
    return RMaterialHandle{ id };
}

void RendererSubsystem::DestroyMaterial(RMaterialHandle h)
{
    if (!h.IsValid()) return;
    m_Materials.erase(h.id);
    if (m_GPUStateCache.material == h)
        {
        m_GPUStateCache.material = RMaterialHandle::Invalid();
    }
}

RFramebufferHandle RendererSubsystem::CreateColorTarget(int w, int h, RTextureHandle& outColor)
{
    RFramebuffer fb{};
    fb.width         = w;
    fb.height        = h;
    fb.samples       = 1;
    fb.bColorAsTexture = true;
    fb.bDepthAsTexture = false;  // no depth; we just blit into it
    fb.colorMode     = EColorMode::HDR16F;
    fb.depthMode     = EDepthMode::None;

    RFramebufferHandle fbo = m_Backend->CreateFramebuffer(fb);
    outColor = m_Backend->GetFramebufferColorTexture(fbo);
    return fbo;
}

void RendererSubsystem::DestroyColorTarget(RFramebufferHandle fbo)
{
    if (fbo.IsValid())
    {
        m_Backend->DestroyFramebuffer(fbo);
    }
}

const FSurfaceDesc* RendererSubsystem::GetMaterialSurface(RMaterialHandle h) const
{
    if (!h.IsValid()) return nullptr;
    auto it = m_Materials.find(h.id);
    return (it == m_Materials.end()) ? nullptr : &it->second.surface;
}

void RendererSubsystem::EnqueueRenderTask(std::function<void()> fn)
{
    // TODO: later, push into a lock-free queue
    fn();
}