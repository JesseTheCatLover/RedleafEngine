//  Copyright 2025-2026 JesseTheCatLover. All Rights Reserved.

#pragma once
#include "Rendering/IRenderBackend.h"

#include <unordered_map>
#include <unordered_map>
#include <vector>

#include <glad/gl.h>

#include "Core/Math/FVector2.h"
#include "Core/Math/FVector3.h"
#include "Core/Memory/SmartPointers.h"
#include "Rendering/RObjects.h"

#define MAX_BONE_INFLUENCE 4

struct FDebugClipVertex;
struct FDebugVertex;
struct RLightData;

class GLBackend : public IRenderBackend
{
public:
    struct FVertex
    {
        FVector3 position;
        FVector3 normal;
        FVector2 texCoords;
        FVector3 tangent;
        FVector3 bitangent;
        int boneIDs[MAX_BONE_INFLUENCE]{};
        float weights[MAX_BONE_INFLUENCE]{};
    };

    struct FGLMesh
    {
        GLuint vao = 0, vbo = 0, ebo = 0;
        uint32_t indexCount = 0;
    };

    struct FGLShader
    {
        GLuint program = 0;
    };

    struct FGLTexture
    {
        GLuint handle = 0;
        GLenum target = GL_TEXTURE_2D;
        bool ownedByFBO = false; // True when created/owned by an FBO
    };

    struct FGLDepthStencilStorage
    {
        GLuint renderbuffer = 0;

        int width = 0;
        int height = 0;
        int samples = 1;

        GLenum internalFormat = GL_DEPTH24_STENCIL8;
        EDepthMode depthMode = EDepthMode::None;

        bool bHasStencil = false;

        ~FGLDepthStencilStorage()
        {
            if (renderbuffer)
                glDeleteRenderbuffers(1, &renderbuffer);
        }
    };

    struct FGLFramebuffer
    {
        GLuint fbo = 0;
        int width = 0, height = 0, samples = 1;

        // Color
        GLuint colorAttachment = 0;          // if single-sample texture
        GLuint colorAttachmentMS = 0;        // if multisample (texture or renderbuffer)
        bool bColorIsTexture = true;        // true: texture, false: renderbuffer
        GLenum colorTarget = GL_TEXTURE_2D; // or GL_TEXTURE_2D_MULTISAMPLE when MSAA texture
        GLint colorInternalFormat = GL_RGBA8;

        // Depth-stencil
        GLuint depthStencil = 0;             // renderbuffer for best perf, or texture if sampling
        bool bDepthIsTexture = false;
        GLenum depthInternalFormat = GL_DEPTH24_STENCIL8; // or GL_DEPTH_COMPONENT24, etc.

        TSharedPtr<FGLDepthStencilStorage> depthStencilStorage{};

        // Engine-visible handles (valid only when texture && !MSAA)
        RTextureHandle colorTexHandle{};
        RTextureHandle depthTexHandle{};

        [[nodiscard]] bool IsMSAA() const { return samples > 1; }
        [[nodiscard]] bool IsSRGBColor() const { return colorInternalFormat == GL_SRGB8_ALPHA8; }
        [[nodiscard]] bool HasColorTex() const { return bColorIsTexture && !IsMSAA(); }
        [[nodiscard]] bool HasDepthTex() const { return bDepthIsTexture && !IsMSAA(); }

        void BindAndSetViewport() const { glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0,0,width,height); }
        void Reset() { *this = FGLFramebuffer{}; }
    };

    struct FFramebufferState
    {
        GLint drawFramebuffer = 0;
        GLint readFramebuffer = 0;
        GLint viewport[4]{};
    };

    struct FStencilFaceState
    {
        GLint function = GL_ALWAYS;
        GLint reference = 0;
        GLint valueMask = 0xFF;
        GLint writeMask = 0xFF;

        GLint stencilFail = GL_KEEP;
        GLint depthFail = GL_KEEP;
        GLint depthPass = GL_KEEP;
    };

    struct FRenderState
    {
        GLboolean bBlendEnabled = GL_FALSE;
        GLint blendSrcRGB = GL_ONE;
        GLint blendDstRGB = GL_ZERO;
        GLint blendSrcAlpha = GL_ONE;
        GLint blendDstAlpha = GL_ZERO;

        GLboolean bDepthTestEnabled = GL_FALSE;
        GLboolean bDepthWriteEnabled = GL_TRUE;
        GLint depthFunction = GL_LESS;

        GLboolean bCullEnabled = GL_FALSE;
        GLint cullFaceMode = GL_BACK;

        GLboolean colorWriteMask[4]{
            GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE
        };

        GLboolean bScissorEnabled = GL_FALSE;
        GLint scissorBox[4]{};

        GLboolean bStencilEnabled = GL_FALSE;
        FStencilFaceState stencilFront{};
        FStencilFaceState stencilBack{};

        GLint currentProgram = 0;
        GLint vertexArray = 0;

        GLint activeTexture = GL_TEXTURE0;
        GLint texture2DBindingUnit0 = 0;
    };

    struct FGLLight {
        // std140-friendly: pack intensity into w of position
        GLfloat pos_intensity[4]; // x,y,z,intensity
        GLfloat color_pad[4];  // r,g,b, pad
    };

private:
    static std::unordered_map<RMeshHandle, FGLMesh> m_Meshes;
    static std::unordered_map<RShaderHandle, FGLShader> m_Shaders;
    static std::unordered_map<RTextureHandle, FGLTexture> m_Textures;
    static std::unordered_map<RFramebufferHandle, FGLFramebuffer> m_Framebuffers;
    std::vector<FFramebufferState> m_FramebufferStack{};
    std::vector<FRenderState> m_RenderStateStack{};
    Rint m_NextID = 1;

    IPlatformSurface* m_Surface = nullptr;

    RTextureHandle RegisterTextureFromGL(GLuint gltex, GLenum target, bool ownedByFBO);

    GLuint m_LightUBO = 0;
    GLsizeiptr m_LightUBOSize = 0;
    static constexpr GLuint LIGHTS_BINDING = 2;
    std::vector<FGLLight> m_LightStaging;
    int m_LastLightCount = 0;

    GLuint  m_DebugLineVAO = 0;
    GLuint  m_DebugLineVBO = 0;
    size_t  m_DebugLineVBBytes = 0;

    GLuint  m_DebugClipTriVAO = 0;
    GLuint  m_DebugClipTriVBO = 0;
    size_t  m_DebugClipTriVBBytes = 0;

    uint32_t m_DebugWorldTriVAO = 0;
    uint32_t m_DebugWorldTriVBO = 0;
    size_t   m_DebugWorldTriVBBytes = 0;

public:
    bool Initialize(IPlatformSurface* surface) override;
    void Shutdown() override;

    void BeginFrame() override;
    void EndFrame() override;

    void SetViewport(int x, int y, int width, int height) override;
    void ClearColorDepth(float r, float g, float b, float a, bool bClearDepth) override;
    void ClearDepthOnly(bool bClearDepth) override;
    void ClearStencilBuffer(int value) override;
    void ClearDepthStencil(float depth, int stencil) override;
    void ClearColorDepthStencil(float r, float g, float b, float a, float depth, int stencil) override;

    [[nodiscard]] void* GetNativeTextureHandle(RTextureHandle handle) const override;

    void SetDepthState(bool bTestEnable, bool bWriteEnable, ECompareFunc func) override;

    void SetBlendState(bool bEnable, EBlendFactor src, EBlendFactor dst) override;

    void SetCullMode(ECullMode mode) override;

    // Resources
    RMeshHandle CreateMesh(const RMesh& meshData) override;
    void DestroyMesh(RMeshHandle handle) override;
    FGLMesh* FindMesh(RMeshHandle handle);

    RTextureHandle CreateTexture(const RTexture& textureData) override;
    void DestroyTexture(RTextureHandle handle) override;
    void BindTexture(RTextureHandle texture, uint32_t slot) override;

    [[nodiscard]] RTextureHandle CreateTextureFromFramebufferRegion(RFramebufferHandle source, int x, int y, int width,
        int height) override;

    RShaderHandle CreateShader(const RShader& shaderData) override;
    void DestroyShader(RShaderHandle handle) override;
    void BindShader(RShaderHandle shader) override;

    RFramebufferHandle CreateFramebuffer(const RFramebuffer& framebufferData) override;
    void DestroyFramebuffer(RFramebufferHandle handle) override;
    void BindFramebuffer(RFramebufferHandle handle) override;
    void UnbindFramebuffer() override;
    void ResolveFramebuffer(RFramebufferHandle src, RFramebufferHandle dst,
                            EResolveMask mask = EResolveMask::Color,
                            EResolveFilter filter = EResolveFilter::Nearest) override;

    bool PushFramebuffer(RFramebufferHandle handle) override;
    bool PopFramebuffer() override;

    void ClearFramebuffer(const FRenderClearDesc &desc) override;

    bool PushRenderState() override;
    bool PopRenderState() override;

    void SetStencilState(const FStencilState &state) override;

    void SetColorWriteMask(bool r, bool g, bool b, bool a) override;

    RTextureHandle GetFramebufferColorTexture(RFramebufferHandle) override;
    RTextureHandle GetFramebufferDepthTexture(RFramebufferHandle) override;
    void EnsureDebugLineStream();
    void SubmitDebugLineList(RShaderHandle shader, const FDebugVertex* verts, uint32_t vertCount) override;
    void EnsureDebugClipTriStream();
    void SubmitDebugClipTriList(RShaderHandle shader, const FDebugClipVertex* verts, uint32_t vertCount) override;
    void EnsureDebugWorldTriStream();
    void SubmitDebugWorldTriList(RShaderHandle shader, const FDebugWorldVertex *verts, uint32_t vertCount) override;


    void SetUniformInt   (RShaderHandle sh, const char* name, int v) override;
    void SetUniformFloat (RShaderHandle sh, const char* name, float v) override;
    void SetUniformVec2  (RShaderHandle sh, const char* name, const float* v2) override;
    void SetUniformVec3  (RShaderHandle sh, const char* name, const float* v3) override;
    void SetUniformVec4  (RShaderHandle sh, const char* name, const float* v4) override;
    void SetUniformMat4  (RShaderHandle sh, const char* name, const float* mat4) override;

    void LinkUniformBlock(RShaderHandle sh, const char* blockName, uint32_t bindingPoint) override;

    [[nodiscard]] FBackendCoordDesc GetCoordConvention() const override;

    [[nodiscard]] FMatrix4 BuildProjectionMatrix(const FProjectionDesc &desc) const override;

    // Rendering
    void SubmitMesh(RMeshHandle mesh, RShaderHandle shader, const FMatrix4& transform) override;
    void UploadLights(const RLightData* lights, uint32_t count) override;

    void EnableScissorRegion(bool bEnable) override;
    void SetScissorRegion(int x, int y, int width, int height) override;

private:
    bool AttachSharedDepthStencil(FGLFramebuffer& framebuffer, const RFramebuffer& desc);
};
