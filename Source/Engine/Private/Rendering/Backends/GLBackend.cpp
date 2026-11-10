//  Copyright 2025-2026 JesseTheCatLover. All Rights Reserved.

#include "GLBackend.h"
#include "Rendering/IPlatformSurface.h"
#include "Core/Math/FMatrix4.h"
#include <iostream>
#include <string>
#include <cmath>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <unordered_set>

#include "Framework/DebugDrawFramework.h"
#include "Rendering/RCommandQueue.h"
#include "Rendering/RObjects.h"

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

std::unordered_map<RMeshHandle, GLBackend::FGLMesh> GLBackend::m_Meshes{};
std::unordered_map<RShaderHandle, GLBackend::FGLShader> GLBackend::m_Shaders{};
std::unordered_map<RTextureHandle, GLBackend::FGLTexture> GLBackend::m_Textures{};
std::unordered_map<RFramebufferHandle, GLBackend::FGLFramebuffer> GLBackend::m_Framebuffers{};

// Helpers
static inline uint32_t NextId(uint32_t &next) { return next++; }

struct FScopedGLFramebufferBindings
{
    GLint drawFramebuffer = 0;
    GLint readFramebuffer = 0;

    FScopedGLFramebufferBindings()
    {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
    }

    ~FScopedGLFramebufferBindings()
    {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawFramebuffer));
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(readFramebuffer));
    }
};

static bool CheckShader(GLuint sh, const char* label)
{
    GLint ok = GL_FALSE; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len=0; glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
        std::vector<GLchar> log(len); glGetShaderInfoLog(sh, len, nullptr, log.data());
        std::cerr << "[GLBackend]: Shader compile failed (" << label << "): " << log.data() << "\n";
        return false;
    }
    return true;
}
static bool CheckProgram(GLuint prog)
{
    GLint ok = GL_FALSE; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len=0; glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::vector<GLchar> log(len); glGetProgramInfoLog(prog, len, nullptr, log.data());
        std::cerr << "[GLBackend]: Program link failed: " << log.data() << "\n";
        return false;
    }
    return true;
}

static GLuint CompileShaderChecked(GLenum type, const std::string& source, const char* label)
{
    GLuint s = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    CheckShader(s, label);
    return s;
}

static inline void GLFormatFrom(ETexFormat fmt, GLint& internal, GLenum& external)
{
    switch (fmt) {
        case ETexFormat::RGBA8:    internal = GL_RGBA8;      external = GL_RGBA; break;
        case ETexFormat::RGB8:     internal = GL_RGB8;       external = GL_RGB;  break;
        case ETexFormat::SRGB8_A8: internal = GL_SRGB8_ALPHA8; external = GL_RGBA; break;
        case ETexFormat::SRGB8:    internal = GL_SRGB8;      external = GL_RGB;  break;
        case ETexFormat::R8:       internal = GL_R8;         external = GL_RED;  break;
        case ETexFormat::RG16F:    internal = GL_RG16F;      external = GL_RG;   break;
        case ETexFormat::RGBA16F:  internal = GL_RGBA16F;    external = GL_RGBA; break;
        default:                   internal = GL_RGBA8;      external = GL_RGBA; break;
    }
}

static inline GLuint GetProgramFromHandle(const std::unordered_map<RShaderHandle, GLBackend::FGLShader>& map,
                                          RShaderHandle h)
{
    auto it = map.find(h);
    return (it != map.end()) ? it->second.program : 0u;
}

static inline GLint GetUniformLoc(GLuint prog, const char* name)
{
    return (prog && name) ? glGetUniformLocation(prog, name) : -1;
}

static inline GLbitfield ToGLMask(IRenderBackend::EResolveMask m)
{
    using M = IRenderBackend::EResolveMask;
    GLbitfield bits = 0;
    if ((static_cast<uint8_t>(m) & static_cast<uint8_t>(M::Color))   != 0) bits |= GL_COLOR_BUFFER_BIT;
    if ((static_cast<uint8_t>(m) & static_cast<uint8_t>(M::Depth))   != 0) bits |= GL_DEPTH_BUFFER_BIT;
    if ((static_cast<uint8_t>(m) & static_cast<uint8_t>(M::Stencil)) != 0) bits |= GL_STENCIL_BUFFER_BIT;
    return bits;
}
static inline GLenum ToGLFilter(IRenderBackend::EResolveFilter f)
{
    using F = IRenderBackend::EResolveFilter;
    return (f == F::Linear) ? GL_LINEAR : GL_NEAREST;
}

struct GLColorDesc { GLint internal; GLenum external; GLenum type; };
struct GLDepthDesc { GLint internal; GLenum external; GLenum type; bool hasStencil; };

static GLColorDesc ToGL(const RFramebuffer& req)
{
    switch (req.colorMode) {
        case EColorMode::LDR8:     return { GL_RGBA8,          GL_RGBA, GL_UNSIGNED_BYTE };
        case EColorMode::HDR16F:   return { GL_RGBA16F,        GL_RGBA, GL_HALF_FLOAT    };
        case EColorMode::sRGB8_A8: return { GL_SRGB8_ALPHA8,   GL_RGBA, GL_UNSIGNED_BYTE };
    }
    return { GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE };
}

static GLenum ToGL(IRenderBackend::ECompareFunc f)
{
    using F = IRenderBackend::ECompareFunc;
    switch (f)
    {
        case F::Never:        return GL_NEVER;
        case F::Less:         return GL_LESS;
        case F::LessEqual:    return GL_LEQUAL;
        case F::Equal:        return GL_EQUAL;
        case F::Greater:      return GL_GREATER;
        case F::GreaterEqual: return GL_GEQUAL;
        case F::NotEqual:     return GL_NOTEQUAL;
        case F::Always:       return GL_ALWAYS;
    }
    return GL_LESS; // sensible default
}

static GLenum ToGL(IRenderBackend::EStencilOp op)
{
    using O = IRenderBackend::EStencilOp;
    switch (op)
    {
        case O::Keep:     return GL_KEEP;
        case O::Zero:     return GL_ZERO;
        case O::Replace:  return GL_REPLACE;
        case O::Incr:     return GL_INCR;
        case O::IncrWrap: return GL_INCR_WRAP;
        case O::Decr:     return GL_DECR;
        case O::DecrWrap: return GL_DECR_WRAP;
        case O::Invert:   return GL_INVERT;
    }
    return GL_KEEP;
}

static GLDepthDesc ToGLDepth(const RFramebuffer& req)
{
    switch (req.depthMode) {
        case EDepthMode::None:        return { 0, 0, 0, false };
        case EDepthMode::D24S8:       return { GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, true  };
        case EDepthMode::D32F:        return { GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, false };
        case EDepthMode::DepthOnly16: return { GL_DEPTH_COMPONENT16,  GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, false };
    }
    return { 0, 0, 0, false };
}

static GLenum ToGL(ETexWrap w)
{
    return (w == ETexWrap::ClampEdge) ? GL_CLAMP_TO_EDGE : GL_REPEAT;
}

static GLenum ToGL(ETexFilter f)
{
    switch (f) {
        case ETexFilter::Nearest:            return GL_NEAREST;
        case ETexFilter::Linear:             return GL_LINEAR;
        case ETexFilter::LinearMipmapLinear: return GL_LINEAR_MIPMAP_LINEAR;
    }
    return GL_LINEAR;
}

bool GLBackend::Initialize(IPlatformSurface* surface)
{
    if (!surface)
    {
        std::cerr << "[GLBackend]: No valid surface provided!" << std::endl;
        return false;
    }

    m_Surface = surface;

    // Get the platform-specific loader function
    const auto loader = surface->GetProcAddressFunction();
    if (!loader)
    {
        std::cerr << "[GLBackend]: Surface does not provide GetProcAddressFunction!" << std::endl;
        return false;
    }

    // Load GL using that function
    if (!gladLoadGL((GLADloadfunc)loader))
    {
        std::cerr << "[GLBackend]: Failed to load OpenGL functions (GLAD)" << std::endl;
        return false;
    }

    std::cout << "[GLBackend]: OpenGL version" << glGetString(GL_VERSION) << " with renderer "
    << glGetString(GL_RENDERER) << " loaded successfully" << std::endl;

    return true;
}

void GLBackend::Shutdown()
{
    m_FramebufferStack.clear();
    m_RenderStateStack.clear();

    // Clean up meshes
    for (auto &[_, mesh] : m_Meshes)
    {
        glDeleteVertexArrays(1, &mesh.vao);
        glDeleteBuffers(1, &mesh.vbo);
        glDeleteBuffers(1, &mesh.ebo);
    }
    m_Meshes.clear();

    // Clean up shaders
    for (auto &[_, shader] : m_Shaders)
        glDeleteProgram(shader.program);
    m_Shaders.clear();

    // Clean up Framebuffers first (this also erases any registered engine handles)
    for (auto &[_, fb] : m_Framebuffers) {
        if (fb.fbo)
            glDeleteFramebuffers(1, &fb.fbo);

        if (fb.colorAttachment)
        {
            if (fb.samples == 1 && fb.bColorIsTexture)
                glDeleteTextures(1, &fb.colorAttachment);
            else
                glDeleteRenderbuffers(1, &fb.colorAttachment);
        }

        if (fb.colorAttachmentMS)
        {
            if (fb.samples > 1 && fb.bColorIsTexture)
                glDeleteTextures(1, &fb.colorAttachmentMS);
            else
                glDeleteRenderbuffers(1, &fb.colorAttachmentMS);
        }

        // Clean up depth-stencil storage.
        if (fb.depthStencil)
        {
            if (fb.bDepthIsTexture && fb.samples == 1)
                glDeleteTextures(1, &fb.depthStencil);
            else if (!fb.depthStencilStorage)
                glDeleteRenderbuffers(1, &fb.depthStencil);
        }
    }
    m_Framebuffers.clear();

    // Now delete standalone textures only
    for (auto it = m_Textures.begin(); it != m_Textures.end(); ) {
        if (!it->second.ownedByFBO && it->second.handle)
            glDeleteTextures(1, &it->second.handle);
        it = m_Textures.erase(it);
    }

    std::cout << "[GLBackend]: Shutdown completed" << std::endl;
}

void GLBackend::BeginFrame()
{
    //glEnable(GL_DEPTH_TEST);
    //glEnable(GL_BLEND);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void GLBackend::EndFrame()
{
    //glFlush();
    glDisable(GL_DEPTH_TEST);
    //glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    //glDisable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_STENCIL_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glStencilMask(0xFF);
}

void GLBackend::SetViewport(int x, int y, int width, int height)
{
    glViewport(x, y, width, height);
}

void GLBackend::ClearColorDepth(float r, float g, float b, float a, bool bClearDepth)
{
    glClearColor(r, g, b, a);
    GLbitfield mask = GL_COLOR_BUFFER_BIT;
    if (bClearDepth) mask |= GL_DEPTH_BUFFER_BIT;
    glClear(mask);
}

void GLBackend::ClearDepthOnly(bool bClearDepth)
{
    GLbitfield mask = 0;
    if (bClearDepth) mask |= GL_DEPTH_BUFFER_BIT;
    glClear(mask);
}

void GLBackend::ClearStencilBuffer(int value)
{
    GLint previousWriteMask = 0;
    GLint previousClearValue = 0;

    glGetIntegerv(GL_STENCIL_WRITEMASK, &previousWriteMask);
    glGetIntegerv(GL_STENCIL_CLEAR_VALUE, &previousClearValue);

    glStencilMask(0xFF);
    glClearStencil(std::clamp(value, 0, 255));
    glClear(GL_STENCIL_BUFFER_BIT);

    glClearStencil(previousClearValue);
    glStencilMask(static_cast<GLuint>(previousWriteMask));
}

void GLBackend::ClearDepthStencil(float depth, int stencil)
{
    // Clamp to sensible ranges
    depth = std::clamp(depth, 0.0f, 1.0f);
    stencil = std::clamp(stencil, 0, 255);

    // Make sure clears actually write
    glDepthMask(GL_TRUE);

    // Stencil clears are affected by stencil write mask.
    // Make sure we can clear all bits.
    glStencilMask(0xFF);

    GLint drawFbo = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);

    if (drawFbo != 0)
    {
        // FBO path (fast, explicit)
        glClearBufferfi(GL_DEPTH_STENCIL, 0, depth, stencil);
    }
    else
    {
        // Default framebuffer path (widely compatible)
        glClearDepth(depth);
        glClearStencil(stencil);
        glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }
}

void GLBackend::ClearColorDepthStencil(float r, float g, float b, float a, float depth, int stencil)
{
    const GLfloat color[4] = { r, g, b, a };

    // Color attachment 0
    glClearBufferfv(GL_COLOR, 0, color);

    // Depth + stencil attachment
    glClearBufferfi(GL_DEPTH_STENCIL, 0, depth, stencil);
}

void* GLBackend::GetNativeTextureHandle(RTextureHandle handle) const
{
    auto it = m_Textures.find(handle);
    if (it == m_Textures.end())
        return nullptr;

    const FGLTexture& tex = it->second;
    GLuint glTex = tex.handle;
    if (glTex == 0)
        return nullptr;

    return reinterpret_cast<void*>(static_cast<uintptr_t>(glTex));
}

void GLBackend::SetDepthState(bool bTestEnable, bool bWriteEnable, ECompareFunc func)
{
    if (bTestEnable) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    glDepthMask(bWriteEnable ? GL_TRUE : GL_FALSE);

    glDepthFunc(ToGL(func));
}

void GLBackend::SetBlendState(bool bEnable, EBlendFactor src, EBlendFactor dst)
{
    if (bEnable) glEnable(GL_BLEND); else glDisable(GL_BLEND);

    auto toGL = [](EBlendFactor b)->GLenum {
        switch (b)
        {
            case EBlendFactor::Zero:             return GL_ZERO;
            case EBlendFactor::One:              return GL_ONE;
            case EBlendFactor::SrcColor:         return GL_SRC_COLOR;
            case EBlendFactor::OneMinusSrcColor: return GL_ONE_MINUS_SRC_COLOR;
            case EBlendFactor::DstColor:         return GL_DST_COLOR;
            case EBlendFactor::OneMinusDstColor: return GL_ONE_MINUS_DST_COLOR;
            case EBlendFactor::SrcAlpha:         return GL_SRC_ALPHA;
            case EBlendFactor::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
            case EBlendFactor::DstAlpha:         return GL_DST_ALPHA;
            case EBlendFactor::OneMinusDstAlpha: return GL_ONE_MINUS_DST_ALPHA;
        }
        return GL_ONE;
    };
    glBlendFunc(toGL(src), toGL(dst));
}

void GLBackend::SetCullMode(ECullMode mode)
{
    if (mode == ECullMode::None)
        {
        glDisable(GL_CULL_FACE);
        return;
    }
    glEnable(GL_CULL_FACE);
    glCullFace(mode == ECullMode::Back ? GL_BACK : GL_FRONT);
}

RMeshHandle GLBackend::CreateMesh(const RMesh& meshData)
{
    FGLMesh mesh{};
    glGenVertexArrays(1, &mesh.vao);
    glBindVertexArray(mesh.vao);

    glGenBuffers(1, &mesh.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    if (!meshData.vertices.empty())
    {
        glBufferData(GL_ARRAY_BUFFER,
                     meshData.vertices.size() * sizeof(float),
                     meshData.vertices.data(),
                     GL_STATIC_DRAW);
    }

    if (!meshData.indices.empty())
    {
        glGenBuffers(1, &mesh.ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     meshData.indices.size() * sizeof(uint32_t),
                     meshData.indices.data(),
                     GL_STATIC_DRAW);
        mesh.indexCount = static_cast<uint32_t>(meshData.indices.size());
    }
    else
    {
        if (meshData.vertexStride > 0)
            mesh.indexCount = static_cast<uint32_t>((meshData.vertices.size() * sizeof(float)) / meshData.vertexStride);
        else
            mesh.indexCount = static_cast<uint32_t>(meshData.vertices.size() / 3);
    }

    const GLsizei stride = (meshData.vertexStride > 0)
    ? static_cast<GLsizei>(meshData.vertexStride)
    : static_cast<GLsizei>(sizeof(float) * 3);

    // Attribute layout
    if (!meshData.attributes.empty())
    {
        for (const FVertexAttribute& attribute : meshData.attributes)
        {
            const GLuint location = static_cast<GLuint>(attribute.location);

            glEnableVertexAttribArray(location);
            glVertexAttribPointer(
                location,
                attribute.size,
                GL_FLOAT,
                GL_FALSE,
                stride,
                reinterpret_cast<const void*>(static_cast<std::uintptr_t>(attribute.offset))
            );
        }
    }
    else
    {
        // Attribute layout (0=pos, 1=normal, 2=uv, 3=tangent, etc.)
        const GLsizei stride = (meshData.vertexStride > 0)
        ? static_cast<GLsizei>(meshData.vertexStride)
        : static_cast<GLsizei>(sizeof(float) * 3); // fallback
    }

    // Position
    size_t offset = 0;
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offset));
    offset += sizeof(float) * 3;

    if (meshData.bHasNormals)
    {
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offset));
        offset += sizeof(float) * 3;
    }

    if (meshData.bHasUVs)
    {
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offset));
        offset += sizeof(float) * 2;
    }

    if (meshData.bHasTangents)
    {
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offset));
        offset += sizeof(float) * 3;
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    uint32_t id = NextId(m_NextID);
    RMeshHandle handle{ id };
    m_Meshes[handle] = mesh;

    return handle;
}

void GLBackend::DestroyMesh(RMeshHandle handle)
{
    if (auto it = m_Meshes.find(handle); it != m_Meshes.end())
    {
        auto &mesh = it->second;
        if (mesh.vao) glDeleteVertexArrays(1, &mesh.vao);
        if (mesh.vbo) glDeleteBuffers(1, &mesh.vbo);
        if (mesh.ebo) glDeleteBuffers(1, &mesh.ebo);
        m_Meshes.erase(it);
    }
}

GLBackend::FGLMesh* GLBackend::FindMesh(RMeshHandle handle)
{
    if (auto it = m_Meshes.find(handle); it != m_Meshes.end())
        return &it->second;
    return nullptr;
}

RTextureHandle GLBackend::CreateTexture(const RTexture &textureData)
{
    GLuint tex = 0;
    glGenTextures(1, &tex);

    const GLenum target = (textureData.type == ETexType::Tex2D) ? GL_TEXTURE_2D : GL_TEXTURE_CUBE_MAP;
    glBindTexture(target, tex);

    glTexParameteri(target, GL_TEXTURE_WRAP_S, ToGL(textureData.wrapS));
    glTexParameteri(target, GL_TEXTURE_WRAP_T, ToGL(textureData.wrapT));
    if (target == GL_TEXTURE_CUBE_MAP)
    {
        glTexParameteri(target, GL_TEXTURE_WRAP_R, ToGL(textureData.wrapR));
    }
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, ToGL(textureData.minFilter));
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, ToGL(textureData.magFilter));
    if (textureData.bMaxLevel > 0)
    {
        glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, textureData.bMaxLevel);
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    GLint internal = 0; GLenum external = 0;
    if (textureData.bSRGB && (textureData.format == ETexFormat::RGBA8 || textureData.format == ETexFormat::RGB8))
    {
        GLFormatFrom(textureData.channels == 4 ? ETexFormat::SRGB8_A8 : ETexFormat::SRGB8, internal, external);
    }
    else
    {
        GLFormatFrom(textureData.format, internal, external);
    }

    if (textureData.bUseAnisotropy)
    {
        GLfloat maxAniso = 0.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
        if (maxAniso >= 1.0f)
            glTexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAniso);
    }

    // Choose pixel type based on format
    GLenum pixelType = GL_UNSIGNED_BYTE;
    switch (textureData.format)
    {
        case ETexFormat::RG16F:
        case ETexFormat::RGBA16F: pixelType = GL_HALF_FLOAT; break;
        default: ;
            // add others as needed (R16F, R32F, etc.)
    }

    if (textureData.type == ETexType::Tex2D)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, internal,
                     textureData.width, textureData.height, 0,
                     external, pixelType, textureData.data);

        if (textureData.bGenerateMipmaps)
            glGenerateMipmap(GL_TEXTURE_2D);
    }
    else // CubeMap
    {
        for (int i = 0; i < 6; ++i)
        {
            const void* face = textureData.faces[i];
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internal,
                         textureData.width, textureData.height, 0,
                         external, pixelType, face);
        }
        if (textureData.bGenerateMipmaps)
            glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    }

    glBindTexture(target, 0);

    RTextureHandle handle{ NextId(m_NextID) };
    m_Textures[handle] = { tex, target };
    return handle;
}

void GLBackend::DestroyTexture(RTextureHandle handle)
{
    auto it = m_Textures.find(handle);
    if (it == m_Textures.end()) return;
    if (!it->second.ownedByFBO)
    {
        if (it->second.handle) glDeleteTextures(1, &it->second.handle);
    }
    m_Textures.erase(it);
}

void GLBackend::BindTexture(RTextureHandle texture, uint32_t slot)
{
    auto it = m_Textures.find(texture);
    if (it == m_Textures.end()) return;

    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(it->second.target, it->second.handle);
}

RTextureHandle GLBackend::CreateTextureFromFramebufferRegion(RFramebufferHandle source, int x, int y, int width, int height)
{
    if (!source.IsValid() || x < 0 || y < 0 || width <= 0 || height <= 0 || m_Framebuffers.empty())
        return {};

    const auto sourceIt = m_Framebuffers.find(source);

    if (sourceIt == m_Framebuffers.end())
        return {};

    const FGLFramebuffer& sourceFramebuffer = sourceIt->second;

    if (!sourceFramebuffer.fbo ||
        (!sourceFramebuffer.colorAttachment && !sourceFramebuffer.colorAttachmentMS) ||
        x > sourceFramebuffer.width ||
        y > sourceFramebuffer.height ||
        width > sourceFramebuffer.width - x ||
        height > sourceFramebuffer.height - y)
        return {};

    FScopedGLFramebufferBindings previousBindings;

    GLint previousTextureBinding = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTextureBinding);

    const GLboolean bScissorWasEnabled = glIsEnabled(GL_SCISSOR_TEST);

    GLuint texture = 0;
    glGenTextures(1, &texture);

    if (!texture)
        return {};

    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLuint copyFramebuffer = 0;
    glGenFramebuffers(1, &copyFramebuffer);

    if (!copyFramebuffer)
    {
        glDeleteTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTextureBinding));
        return {};
    }

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, copyFramebuffer);

    glFramebufferTexture2D(
        GL_DRAW_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        texture,
        0);

    glDrawBuffer(GL_COLOR_ATTACHMENT0);

    if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        glDeleteFramebuffers(1, &copyFramebuffer);
        glDeleteTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTextureBinding));
        return {};
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFramebuffer.fbo);
    glReadBuffer(GL_COLOR_ATTACHMENT0);

    // The active UI scissor must not restrict the destination texture.
    if (bScissorWasEnabled)
        glDisable(GL_SCISSOR_TEST);

    // Flip vertically to match the texture-coordinate convention used by RmlUi.
    glBlitFramebuffer(
        x, y,
        x + width, y + height,
        0, height,
        width, 0,
        GL_COLOR_BUFFER_BIT,
        GL_NEAREST);

    if (bScissorWasEnabled)
        glEnable(GL_SCISSOR_TEST);

    glDeleteFramebuffers(1, &copyFramebuffer);

    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTextureBinding));

    return RegisterTextureFromGL(texture, GL_TEXTURE_2D, false);
}

RTextureHandle GLBackend::RegisterTextureFromGL(GLuint gltex, GLenum target, bool ownedByFBO)
{
    if (gltex == 0) return RTextureHandle::Invalid();

    RTextureHandle handle{ NextId(m_NextID) };

    FGLTexture rec{};
    rec.handle = gltex;
    rec.target = target;
    rec.ownedByFBO = ownedByFBO;

    m_Textures[handle] = rec;
    return handle;
}

RShaderHandle GLBackend::CreateShader(const RShader &shaderData)
{
    GLuint vs = CompileShaderChecked(GL_VERTEX_SHADER,   shaderData.vertexSource,   "VS");
    GLuint fs = CompileShaderChecked(GL_FRAGMENT_SHADER, shaderData.fragmentSource, "FS");

    GLuint gs = 0;
    if (!shaderData.geometrySource.empty()) {
        gs = CompileShaderChecked(GL_GEOMETRY_SHADER, shaderData.geometrySource, "GS");
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    if (gs) glAttachShader(prog, gs);

    glLinkProgram(prog);
    CheckProgram(prog);

    glDeleteShader(vs);
    glDeleteShader(fs);
    if (gs) glDeleteShader(gs);

    RShaderHandle h{ NextId(m_NextID) };
    m_Shaders[h] = { prog };
    return h;
}

void GLBackend::DestroyShader(RShaderHandle handle)
{
    if (auto it = m_Shaders.find(handle); it != m_Shaders.end()) {
        if (it->second.program) glDeleteProgram(it->second.program);
        m_Shaders.erase(it);
    }
}

void GLBackend::BindShader(RShaderHandle shader)
{
    auto it = m_Shaders.find(shader);
    glUseProgram(it != m_Shaders.end() ? it->second.program : 0);
}


RFramebufferHandle GLBackend::CreateFramebuffer(const RFramebuffer& framebufferData)
{
    FScopedGLFramebufferBindings previousBindings;

    if (framebufferData.width <= 0 || framebufferData.height <= 0)
        return {};

    FGLFramebuffer fb{};

    fb.width = framebufferData.width;
    fb.height = framebufferData.height;
    fb.samples = std::max(1, framebufferData.samples);

    const bool bWantColor = framebufferData.bHasColor;

    if (framebufferData.bDepthAsTexture && fb.samples > 1)
        return {};

    fb.bColorIsTexture = bWantColor ? framebufferData.bColorAsTexture : false;
    fb.bDepthIsTexture = framebufferData.bDepthAsTexture;

    const GLColorDesc col = ToGL(framebufferData);
    const GLDepthDesc dep = ToGLDepth(framebufferData);

    fb.colorInternalFormat = col.internal;
    fb.depthInternalFormat = dep.internal;

    // Resolve shared depth-stencil storage.

    TSharedPtr<FGLDepthStencilStorage> sharedDepthStencilStorage{};

    if (framebufferData.sharedDepthStencilFramebuffer.IsValid())
    {
        if (framebufferData.depthMode == EDepthMode::None ||
            framebufferData.bDepthAsTexture)
            return {};

        const auto sharedIt = m_Framebuffers.find(
            framebufferData.sharedDepthStencilFramebuffer
        );

        if (sharedIt == m_Framebuffers.end())
            return {};

        const FGLFramebuffer& shared = sharedIt->second;
        const auto& storage = shared.depthStencilStorage;

        if (!storage ||
            !storage->renderbuffer ||
            shared.depthStencil != storage->renderbuffer ||
            shared.bDepthIsTexture ||
            shared.width != fb.width ||
            shared.height != fb.height ||
            shared.samples != fb.samples ||
            shared.depthInternalFormat != dep.internal ||
            storage->width != fb.width ||
            storage->height != fb.height ||
            storage->samples != fb.samples ||
            storage->internalFormat != dep.internal ||
            storage->depthMode != framebufferData.depthMode ||
            storage->bHasStencil != dep.hasStencil)
        {
            return {};
        }

        sharedDepthStencilStorage = storage;
    }

    // Create framebuffer.

    glGenFramebuffers(1, &fb.fbo);

    if (!fb.fbo)
        return {};

    glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);

    // Cleanup helper.

    const auto cleanup = [&]()
    {
        if (fb.colorTexHandle.IsValid())
            m_Textures.erase(fb.colorTexHandle);

        if (fb.depthTexHandle.IsValid())
            m_Textures.erase(fb.depthTexHandle);

        if (fb.fbo)
            glDeleteFramebuffers(1, &fb.fbo);

        // Color storage.
        if (fb.colorAttachment)
        {
            if (fb.samples == 1 && fb.bColorIsTexture)
                glDeleteTextures(1, &fb.colorAttachment);
            else if (!fb.bColorIsTexture)
                glDeleteRenderbuffers(1, &fb.colorAttachment);
        }

        if (fb.colorAttachmentMS)
        {
            if (fb.samples > 1 && fb.bColorIsTexture)
                glDeleteTextures(1, &fb.colorAttachmentMS);
            else
                glDeleteRenderbuffers(1, &fb.colorAttachmentMS);
        }

        // Depth storage.
        if (fb.depthStencil)
        {
            if (fb.bDepthIsTexture)
                glDeleteTextures(1, &fb.depthStencil);
            else if (!fb.depthStencilStorage)
                glDeleteRenderbuffers(1, &fb.depthStencil);
        }

        // Releases owned or shared renderbuffer storage.
        fb.depthStencilStorage.reset();
    };

    // Color attachment.

    if (!bWantColor)
    {
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        fb.colorAttachment = 0;
        fb.colorAttachmentMS = 0;
        fb.colorTarget = GL_TEXTURE_2D;
        fb.colorTexHandle = RTextureHandle::Invalid();
    }
    else
    {
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glReadBuffer(GL_COLOR_ATTACHMENT0);

        if (fb.samples > 1)
        {
            // MSAA color.
            if (fb.bColorIsTexture)
            {
                glGenTextures(1, &fb.colorAttachmentMS);
                glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, fb.colorAttachmentMS);

                glTexImage2DMultisample(
                    GL_TEXTURE_2D_MULTISAMPLE,
                    fb.samples,
                    col.internal,
                    fb.width,
                    fb.height,
                    GL_TRUE
                );

                glFramebufferTexture2D(
                    GL_FRAMEBUFFER,
                    GL_COLOR_ATTACHMENT0,
                    GL_TEXTURE_2D_MULTISAMPLE,
                    fb.colorAttachmentMS,
                    0
                );

                fb.colorTarget = GL_TEXTURE_2D_MULTISAMPLE;

                glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
            }
            else
            {
                glGenRenderbuffers(1, &fb.colorAttachmentMS);
                glBindRenderbuffer(GL_RENDERBUFFER, fb.colorAttachmentMS);

                glRenderbufferStorageMultisample(
                    GL_RENDERBUFFER,
                    fb.samples,
                    col.internal,
                    fb.width,
                    fb.height
                );

                glFramebufferRenderbuffer(
                    GL_FRAMEBUFFER,
                    GL_COLOR_ATTACHMENT0,
                    GL_RENDERBUFFER,
                    fb.colorAttachmentMS
                );

                glBindRenderbuffer(GL_RENDERBUFFER, 0);
            }
        }
        else
        {
            // Single-sample color.
            if (fb.bColorIsTexture)
            {
                glGenTextures(1, &fb.colorAttachment);
                glBindTexture(GL_TEXTURE_2D, fb.colorAttachment);

                glTexImage2D(
                    GL_TEXTURE_2D,
                    0,
                    col.internal,
                    fb.width,
                    fb.height,
                    0,
                    col.external,
                    col.type,
                    nullptr
                );

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

                glFramebufferTexture2D(
                    GL_FRAMEBUFFER,
                    GL_COLOR_ATTACHMENT0,
                    GL_TEXTURE_2D,
                    fb.colorAttachment,
                    0
                );

                fb.colorTarget = GL_TEXTURE_2D;

                fb.colorTexHandle = RegisterTextureFromGL(
                    fb.colorAttachment,
                    GL_TEXTURE_2D,
                    true
                );

                glBindTexture(GL_TEXTURE_2D, 0);
            }
            else
            {
                glGenRenderbuffers(1, &fb.colorAttachment);
                glBindRenderbuffer(GL_RENDERBUFFER, fb.colorAttachment);

                glRenderbufferStorage(
                    GL_RENDERBUFFER,
                    col.internal,
                    fb.width,
                    fb.height
                );

                glFramebufferRenderbuffer(
                    GL_FRAMEBUFFER,
                    GL_COLOR_ATTACHMENT0,
                    GL_RENDERBUFFER,
                    fb.colorAttachment
                );

                glBindRenderbuffer(GL_RENDERBUFFER, 0);
            }
        }
    }

    // Depth-stencil attachment.

    if (framebufferData.depthMode != EDepthMode::None)
    {
        const GLenum depthAttachment = dep.hasStencil
            ? GL_DEPTH_STENCIL_ATTACHMENT
            : GL_DEPTH_ATTACHMENT;

        if (sharedDepthStencilStorage)
        {
            // Attach existing shared depth-stencil storage.
            fb.depthStencilStorage = sharedDepthStencilStorage;
            fb.depthStencil = sharedDepthStencilStorage->renderbuffer;

            glFramebufferRenderbuffer(
                GL_FRAMEBUFFER,
                depthAttachment,
                GL_RENDERBUFFER,
                fb.depthStencil
            );
        }
        else if (fb.bDepthIsTexture)
        {
            // Single-sample depth texture.
            GLenum depthFormat = GL_DEPTH_COMPONENT;
            GLenum depthType = GL_UNSIGNED_SHORT;

            switch (framebufferData.depthMode)
            {
                case EDepthMode::D24S8:
                    depthFormat = GL_DEPTH_STENCIL;
                    depthType = GL_UNSIGNED_INT_24_8;
                    break;

                case EDepthMode::D32F:
                    depthFormat = GL_DEPTH_COMPONENT;
                    depthType = GL_FLOAT;
                    break;

                case EDepthMode::DepthOnly16:
                    depthFormat = GL_DEPTH_COMPONENT;
                    depthType = GL_UNSIGNED_SHORT;
                    break;

                default:
                    cleanup();
                    return {};
            }

            glGenTextures(1, &fb.depthStencil);
            glBindTexture(GL_TEXTURE_2D, fb.depthStencil);

            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                dep.internal,
                fb.width,
                fb.height,
                0,
                depthFormat,
                depthType,
                nullptr
            );

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);

            glFramebufferTexture2D(
                GL_FRAMEBUFFER,
                depthAttachment,
                GL_TEXTURE_2D,
                fb.depthStencil,
                0
            );

            fb.depthTexHandle = RegisterTextureFromGL(
                fb.depthStencil,
                GL_TEXTURE_2D,
                true
            );

            glBindTexture(GL_TEXTURE_2D, 0);
        }
        else
        {
            // Create independently owned depth renderbuffer.
            glGenRenderbuffers(1, &fb.depthStencil);

            if (!fb.depthStencil)
            {
                cleanup();
                return {};
            }

            glBindRenderbuffer(GL_RENDERBUFFER, fb.depthStencil);

            if (fb.samples > 1)
            {
                glRenderbufferStorageMultisample(
                    GL_RENDERBUFFER,
                    fb.samples,
                    dep.internal,
                    fb.width,
                    fb.height
                );
            }
            else
            {
                glRenderbufferStorage(
                    GL_RENDERBUFFER,
                    dep.internal,
                    fb.width,
                    fb.height
                );
            }

            glFramebufferRenderbuffer(
                GL_FRAMEBUFFER,
                depthAttachment,
                GL_RENDERBUFFER,
                fb.depthStencil
            );

            glBindRenderbuffer(GL_RENDERBUFFER, 0);

            // Transfer native renderbuffer ownership to shared storage.
            auto storage = MakeShared<FGLDepthStencilStorage>();

            storage->renderbuffer = fb.depthStencil;
            storage->width = fb.width;
            storage->height = fb.height;
            storage->samples = fb.samples;
            storage->internalFormat = dep.internal;
            storage->depthMode = framebufferData.depthMode;
            storage->bHasStencil = dep.hasStencil;

            fb.depthStencilStorage = std::move(storage);
        }
    }

    // Validate framebuffer.

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr
            << "[GLBackend]: FBO incomplete: 0x"
            << std::hex << status << std::dec
            << "\n";

        cleanup();
        return {};
    }

    // Register framebuffer.

    RFramebufferHandle handle{ NextId(m_NextID) };

    m_Framebuffers.emplace(handle, std::move(fb));

    return handle;
}


void GLBackend::DestroyFramebuffer(RFramebufferHandle handle)
{
    auto it = m_Framebuffers.find(handle);
    if (it == m_Framebuffers.end())
        return;

    FGLFramebuffer& fb = it->second;

    // A later pop must never restore a framebuffer that has been deleted.
    for (FFramebufferState& state : m_FramebufferStack)
    {
        if (state.drawFramebuffer == static_cast<GLint>(fb.fbo))
            state.drawFramebuffer = 0;

        if (state.readFramebuffer == static_cast<GLint>(fb.fbo))
            state.readFramebuffer = 0;
    }

    // Remove engine-visible handles from the registry.
    if (fb.colorTexHandle.IsValid())
        m_Textures.erase(fb.colorTexHandle);

    if (fb.depthTexHandle.IsValid())
        m_Textures.erase(fb.depthTexHandle);

    // Delete the framebuffer before releasing its attachments.
    if (fb.fbo)
        glDeleteFramebuffers(1, &fb.fbo);

    // Delete color storage.
    if (fb.colorAttachment)
    {
        if (fb.samples == 1 && fb.bColorIsTexture)
            glDeleteTextures(1, &fb.colorAttachment);
        else if (!fb.bColorIsTexture)
            glDeleteRenderbuffers(1, &fb.colorAttachment);
    }

    if (fb.colorAttachmentMS)
    {
        if (fb.samples > 1 && fb.bColorIsTexture)
            glDeleteTextures(1, &fb.colorAttachmentMS);
        else
            glDeleteRenderbuffers(1, &fb.colorAttachmentMS);
    }

    // Delete depth texture, or unmanaged depth renderbuffer.
    // Managed renderbuffers are released through shared ownership.
    if (fb.depthStencil)
    {
        if (fb.bDepthIsTexture && fb.samples == 1)
            glDeleteTextures(1, &fb.depthStencil);
        else if (!fb.depthStencilStorage)
            glDeleteRenderbuffers(1, &fb.depthStencil);
    }

    m_Framebuffers.erase(it);
}

void GLBackend::BindFramebuffer(RFramebufferHandle handle)
{
    auto it = m_Framebuffers.find(handle);
    if (it == m_Framebuffers.end())
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    const FGLFramebuffer& fb = it->second;
    glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);
    glViewport(0, 0, fb.width, fb.height);
}

void GLBackend::UnbindFramebuffer()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLBackend::ResolveFramebuffer(RFramebufferHandle src, RFramebufferHandle dst,
                                   EResolveMask mask, EResolveFilter filter)
{
    auto itS = m_Framebuffers.find(src);
    auto itD = m_Framebuffers.find(dst);
    if (itS == m_Framebuffers.end() || itD == m_Framebuffers.end()) return;

    FScopedGLFramebufferBindings previousBindings;

    const auto& A = itS->second;
    const auto& B = itD->second;

    glBindFramebuffer(GL_READ_FRAMEBUFFER, A.fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, B.fbo);

    const bool hasDepth = (static_cast<uint8_t>(mask) & static_cast<uint8_t>(EResolveMask::Depth)) != 0
                       || (static_cast<uint8_t>(mask) & static_cast<uint8_t>(EResolveMask::Stencil)) != 0;
    const GLenum glFilter = hasDepth ? GL_NEAREST : ToGLFilter(filter);

    glBlitFramebuffer(
        0, 0, A.width, A.height,
        0, 0, B.width, B.height,
        ToGLMask(mask),
        glFilter
    );

}

bool GLBackend::PushFramebuffer(RFramebufferHandle handle)
{
    auto it = m_Framebuffers.end();

    if (handle.IsValid())
    {
        it = m_Framebuffers.find(handle);
        if (it == m_Framebuffers.end())
            return false;
    }

    FFramebufferState previousState{};
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousState.drawFramebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousState.readFramebuffer);
    glGetIntegerv(GL_VIEWPORT, previousState.viewport);

    m_FramebufferStack.push_back(previousState);

    if (it == m_Framebuffers.end())
    {
        // An invalid/default handle represents the platform backbuffer.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return true;
    }

    const FGLFramebuffer& framebuffer = it->second;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.fbo);
    glViewport(0, 0, framebuffer.width, framebuffer.height);

    return true;
}

bool GLBackend::PopFramebuffer()
{
    if (m_FramebufferStack.empty())
        return false;

    const FFramebufferState previousState = m_FramebufferStack.back();
    m_FramebufferStack.pop_back();

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(previousState.drawFramebuffer));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previousState.readFramebuffer));
    glViewport(
        previousState.viewport[0],
        previousState.viewport[1],
        previousState.viewport[2],
        previousState.viewport[3]
    );

    return true;
}

void GLBackend::ClearFramebuffer(const FRenderClearDesc& desc)
{
    if (!desc.IsValid())
        return;

    GLboolean previousColorMask[4]{};
    GLboolean previousDepthMask = GL_TRUE;
    GLboolean previousScissor = GL_FALSE;

    GLint previousStencilFrontMask = 0;
    GLint previousStencilBackMask = 0;

    if (desc.bClearColor)
        glGetBooleanv(GL_COLOR_WRITEMASK, previousColorMask);

    if (desc.bClearDepth)
        glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);

    if (desc.bClearStencil)
    {
        glGetIntegerv(GL_STENCIL_WRITEMASK, &previousStencilFrontMask);
        glGetIntegerv(GL_STENCIL_BACK_WRITEMASK, &previousStencilBackMask);
    }

    previousScissor = glIsEnabled(GL_SCISSOR_TEST);

    const bool bDisableScissor = previousScissor && !desc.bRespectScissor;

    if (bDisableScissor)
        glDisable(GL_SCISSOR_TEST);

    if (desc.bClearColor)
    {
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glClearBufferfv(GL_COLOR, 0, desc.color);
    }

    if (desc.bClearDepth)
    {
        const GLfloat depth = std::clamp(desc.depth, 0.f, 1.f);

        glDepthMask(GL_TRUE);
        glClearBufferfv(GL_DEPTH, 0, &depth);
    }

    if (desc.bClearStencil)
    {
        const GLint stencil = static_cast<GLint>(desc.stencil);

        glStencilMaskSeparate(GL_FRONT, 0xFF);
        glStencilMaskSeparate(GL_BACK, 0xFF);

        glClearBufferiv(GL_STENCIL, 0, &stencil);
    }

    // Restore the graphics state we temporarily changed.
    if (desc.bClearColor)
        glColorMask(
            previousColorMask[0],
            previousColorMask[1],
            previousColorMask[2],
            previousColorMask[3]);

    if (desc.bClearDepth)
        glDepthMask(previousDepthMask);

    if (desc.bClearStencil)
    {
        glStencilMaskSeparate(GL_FRONT, static_cast<GLuint>(previousStencilFrontMask));
        glStencilMaskSeparate(GL_BACK, static_cast<GLuint>(previousStencilBackMask));
    }

    if (previousScissor)
        glEnable(GL_SCISSOR_TEST);
}


bool GLBackend::PushRenderState()
{
    FRenderState state{};

    // Blend
    state.bBlendEnabled = glIsEnabled(GL_BLEND);

    glGetIntegerv(GL_BLEND_SRC_RGB, &state.blendSrcRGB);
    glGetIntegerv(GL_BLEND_DST_RGB, &state.blendDstRGB);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &state.blendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &state.blendDstAlpha);

    // Depth
    state.bDepthTestEnabled = glIsEnabled(GL_DEPTH_TEST);

    glGetBooleanv(GL_DEPTH_WRITEMASK, &state.bDepthWriteEnabled);
    glGetIntegerv(GL_DEPTH_FUNC, &state.depthFunction);

    // Culling
    state.bCullEnabled = glIsEnabled(GL_CULL_FACE);

    glGetIntegerv(GL_CULL_FACE_MODE, &state.cullFaceMode);

    // Color write mask
    glGetBooleanv(GL_COLOR_WRITEMASK, state.colorWriteMask);

    // Scissor
    state.bScissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    glGetIntegerv(GL_SCISSOR_BOX, state.scissorBox);

    // Stencil
    state.bStencilEnabled = glIsEnabled(GL_STENCIL_TEST);

    // Front face
    glGetIntegerv(GL_STENCIL_FUNC, &state.stencilFront.function);
    glGetIntegerv(GL_STENCIL_REF, &state.stencilFront.reference);
    glGetIntegerv(GL_STENCIL_VALUE_MASK, &state.stencilFront.valueMask);
    glGetIntegerv(GL_STENCIL_WRITEMASK, &state.stencilFront.writeMask);

    glGetIntegerv(GL_STENCIL_FAIL, &state.stencilFront.stencilFail);
    glGetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, &state.stencilFront.depthFail);
    glGetIntegerv(GL_STENCIL_PASS_DEPTH_PASS, &state.stencilFront.depthPass);

    // Back face
    glGetIntegerv(GL_STENCIL_BACK_FUNC, &state.stencilBack.function);
    glGetIntegerv(GL_STENCIL_BACK_REF, &state.stencilBack.reference);
    glGetIntegerv(GL_STENCIL_BACK_VALUE_MASK, &state.stencilBack.valueMask);
    glGetIntegerv(GL_STENCIL_BACK_WRITEMASK, &state.stencilBack.writeMask);

    glGetIntegerv(GL_STENCIL_BACK_FAIL, &state.stencilBack.stencilFail);
    glGetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_FAIL, &state.stencilBack.depthFail);
    glGetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_PASS, &state.stencilBack.depthPass);

    // Shader and vertex array
    glGetIntegerv(GL_CURRENT_PROGRAM, &state.currentProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &state.vertexArray);

    // Texture unit 0
    glGetIntegerv(GL_ACTIVE_TEXTURE, &state.activeTexture);

    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &state.texture2DBindingUnit0);
    glActiveTexture(static_cast<GLenum>(state.activeTexture));

    m_RenderStateStack.push_back(state);
    return true;
}


bool GLBackend::PopRenderState()
{
    if (m_RenderStateStack.empty())
        return false;

    const FRenderState state = m_RenderStateStack.back();
    m_RenderStateStack.pop_back();

    // Blend
    if (state.bBlendEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    glBlendFuncSeparate(
        static_cast<GLenum>(state.blendSrcRGB),
        static_cast<GLenum>(state.blendDstRGB),
        static_cast<GLenum>(state.blendSrcAlpha),
        static_cast<GLenum>(state.blendDstAlpha));

    // Depth
    if (state.bDepthTestEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    glDepthMask(state.bDepthWriteEnabled);
    glDepthFunc(static_cast<GLenum>(state.depthFunction));

    // Culling
    if (state.bCullEnabled)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);

    glCullFace(static_cast<GLenum>(state.cullFaceMode));

    // Color write mask
    glColorMask(
        state.colorWriteMask[0],
        state.colorWriteMask[1],
        state.colorWriteMask[2],
        state.colorWriteMask[3]);

    // Scissor
    glScissor(
        state.scissorBox[0],
        state.scissorBox[1],
        state.scissorBox[2],
        state.scissorBox[3]);

    if (state.bScissorEnabled)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);

    // Stencil: front face
    glStencilFuncSeparate(
        GL_FRONT,
        static_cast<GLenum>(state.stencilFront.function),
        state.stencilFront.reference,
        static_cast<GLuint>(state.stencilFront.valueMask));

    glStencilMaskSeparate(
        GL_FRONT,
        static_cast<GLuint>(state.stencilFront.writeMask));

    glStencilOpSeparate(
        GL_FRONT,
        static_cast<GLenum>(state.stencilFront.stencilFail),
        static_cast<GLenum>(state.stencilFront.depthFail),
        static_cast<GLenum>(state.stencilFront.depthPass));

    // Stencil: back face
    glStencilFuncSeparate(
        GL_BACK,
        static_cast<GLenum>(state.stencilBack.function),
        state.stencilBack.reference,
        static_cast<GLuint>(state.stencilBack.valueMask));

    glStencilMaskSeparate(
        GL_BACK,
        static_cast<GLuint>(state.stencilBack.writeMask));

    glStencilOpSeparate(
        GL_BACK,
        static_cast<GLenum>(state.stencilBack.stencilFail),
        static_cast<GLenum>(state.stencilBack.depthFail),
        static_cast<GLenum>(state.stencilBack.depthPass));

    if (state.bStencilEnabled)
        glEnable(GL_STENCIL_TEST);
    else
        glDisable(GL_STENCIL_TEST);

    // Texture unit 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(
        GL_TEXTURE_2D,
        static_cast<GLuint>(state.texture2DBindingUnit0));

    glActiveTexture(static_cast<GLenum>(state.activeTexture));

    // Shader and vertex array
    glBindVertexArray(static_cast<GLuint>(state.vertexArray));
    glUseProgram(static_cast<GLuint>(state.currentProgram));

    return true;
}

void GLBackend::SetColorWriteMask(bool r, bool g, bool b, bool a)
{
    glColorMask(r ? GL_TRUE : GL_FALSE,
                g ? GL_TRUE : GL_FALSE,
                b ? GL_TRUE : GL_FALSE,
                a ? GL_TRUE : GL_FALSE);
}

void GLBackend::SetStencilState(const FStencilState& s)
{
    if (!s.bEnable)
    {
        glDisable(GL_STENCIL_TEST);
        glStencilMask(0x00); // Prevents accidental stencil writes
        return;
    }

    glEnable(GL_STENCIL_TEST);
    glStencilFunc(ToGL(s.func), (GLint)s.ref, (GLuint)s.readMask);
    glStencilMask((GLuint)s.writeMask);
    glStencilOp(ToGL(s.sfail), ToGL(s.zfail), ToGL(s.zpass));
}

RTextureHandle GLBackend::GetFramebufferColorTexture(RFramebufferHandle h)
{
    auto it = m_Framebuffers.find(h);
    if (it == m_Framebuffers.end()) return RTextureHandle::Invalid();
    return it->second.colorTexHandle;
}

RTextureHandle GLBackend::GetFramebufferDepthTexture(RFramebufferHandle h)
{
    auto it = m_Framebuffers.find(h);
    if (it == m_Framebuffers.end()) return RTextureHandle::Invalid();
    return it->second.depthTexHandle;
}

void GLBackend::EnsureDebugLineStream()
{
    if (m_DebugLineVAO != 0) return;

    glGenVertexArrays(1, &m_DebugLineVAO);
    glGenBuffers(1, &m_DebugLineVBO);

    glBindVertexArray(m_DebugLineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_DebugLineVBO);

    m_DebugLineVBBytes = 0;

    // FDebugVertex layout:
    // location 0: vec3 position (x,y,z)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE,
        (GLsizei)sizeof(FDebugVertex),
        (void*)offsetof(FDebugVertex, x));

    // location 1: vec4 color (r,g,b,a)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 4, GL_FLOAT, GL_FALSE,
        (GLsizei)sizeof(FDebugVertex),
        (void*)offsetof(FDebugVertex, r));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GLBackend::SubmitDebugLineList(RShaderHandle shader, const FDebugVertex* verts, uint32_t vertCount)
{
    if (!verts || vertCount < 2) return;
    if ((vertCount & 1u) != 0u) return; // line list must be even

    auto sit = m_Shaders.find(shader);
    if (sit == m_Shaders.end() || sit->second.program == 0) return;

    EnsureDebugLineStream();

    const size_t bytes = size_t(vertCount) * sizeof(FDebugVertex);

    glBindVertexArray(m_DebugLineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_DebugLineVBO);

    if (bytes > m_DebugLineVBBytes)
    {
        m_DebugLineVBBytes = std::max(bytes, m_DebugLineVBBytes ? m_DebugLineVBBytes * 2 : bytes);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)m_DebugLineVBBytes, nullptr, GL_STREAM_DRAW);
    }

    // orphan + upload
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)bytes, nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)bytes, verts);

    glUseProgram(sit->second.program);
    glDrawArrays(GL_LINES, 0, (GLsizei)vertCount);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GLBackend::EnsureDebugClipTriStream()
{
    if (m_DebugClipTriVAO != 0) return;

    glGenVertexArrays(1, &m_DebugClipTriVAO);
    glGenBuffers(1, &m_DebugClipTriVBO);

    glBindVertexArray(m_DebugClipTriVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_DebugClipTriVBO);

    // start small; will grow
    m_DebugClipTriVBBytes = 0;

    // layout:
    // location 0: vec4 clip position (x,y,z,w)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 4, GL_FLOAT, GL_FALSE,
        (GLsizei)sizeof(FDebugClipVertex),
        (void*)offsetof(FDebugClipVertex, x));

    // location 1: vec4 color (r,g,b,a)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 4, GL_FLOAT, GL_FALSE,
        (GLsizei)sizeof(FDebugClipVertex),
        (void*)offsetof(FDebugClipVertex, r));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GLBackend::SubmitDebugClipTriList(RShaderHandle shader, const FDebugClipVertex* verts, uint32_t vertCount)
{
    if (!verts || vertCount < 3) return;
    if ((vertCount % 3u) != 0u) return; // must be multiple of 3 for triangles

    // validate shader handle
    auto sit = m_Shaders.find(shader);
    if (sit == m_Shaders.end() || sit->second.program == 0) return;

    EnsureDebugClipTriStream();

    const size_t bytes = size_t(vertCount) * sizeof(FDebugClipVertex);

    glBindVertexArray(m_DebugClipTriVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_DebugClipTriVBO);

    // grow if needed
    if (bytes > m_DebugClipTriVBBytes)
    {
        m_DebugClipTriVBBytes = std::max(bytes, m_DebugClipTriVBBytes * 2);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)m_DebugClipTriVBBytes, nullptr, GL_STREAM_DRAW);
    }

    // orphan to avoid GPU stalls, then upload
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)bytes, nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)bytes, verts);

    glUseProgram(sit->second.program);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertCount);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GLBackend::EnsureDebugWorldTriStream()
{
    if (m_DebugWorldTriVAO != 0) return;

    glGenVertexArrays(1, &m_DebugWorldTriVAO);
    glGenBuffers(1, &m_DebugWorldTriVBO);

    glBindVertexArray(m_DebugWorldTriVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_DebugWorldTriVBO);

    m_DebugWorldTriVBBytes = 0;

    // location 0: vec3 position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FDebugWorldVertex),
                          (void*)offsetof(FDebugWorldVertex, x));

    // location 1: vec3 normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(FDebugWorldVertex),
                          (void*)offsetof(FDebugWorldVertex, nx));

    // location 2: vec4 color
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(FDebugWorldVertex),
                          (void*)offsetof(FDebugWorldVertex, r));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GLBackend::SubmitDebugWorldTriList(RShaderHandle shader, const FDebugWorldVertex *verts, uint32_t vertCount)
{
    if (!verts || vertCount < 3) return;
    if ((vertCount % 3u) != 0u) return;

    auto sit = m_Shaders.find(shader);
    if (sit == m_Shaders.end() || sit->second.program == 0) return;

    EnsureDebugWorldTriStream();

    const size_t bytes = size_t(vertCount) * sizeof(FDebugWorldVertex);

    glBindVertexArray(m_DebugWorldTriVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_DebugWorldTriVBO);

    if (bytes > m_DebugWorldTriVBBytes)
    {
        m_DebugWorldTriVBBytes = std::max(bytes, m_DebugWorldTriVBBytes ? m_DebugWorldTriVBBytes * 2 : bytes);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)m_DebugWorldTriVBBytes, nullptr, GL_STREAM_DRAW);
    }

    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)bytes, nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)bytes, verts);

    glUseProgram(sit->second.program);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertCount);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GLBackend::SetUniformInt(RShaderHandle sh, const char* name, int v)
{
    GLuint p = GetProgramFromHandle(m_Shaders, sh); if (!p) return;
    glUseProgram(p);
    GLint loc = GetUniformLoc(p, name); if (loc < 0) return;
    glUniform1i(loc, v);
}

void GLBackend::SetUniformFloat(RShaderHandle sh, const char* name, float v)
{
    GLuint p = GetProgramFromHandle(m_Shaders, sh); if (!p) return;
    glUseProgram(p);
    GLint loc = GetUniformLoc(p, name); if (loc < 0) return;
    glUniform1f(loc, v);
}

void GLBackend::SetUniformVec2(RShaderHandle sh, const char* name, const float* v2)
{
    if (!v2) return;
    GLuint p = GetProgramFromHandle(m_Shaders, sh); if (!p) return;
    glUseProgram(p);
    GLint loc = GetUniformLoc(p, name); if (loc < 0) return;
    glUniform2fv(loc, 1, v2);
}

void GLBackend::SetUniformVec3(RShaderHandle sh, const char* name, const float* v3)
{
    if (!v3) return;
    GLuint p = GetProgramFromHandle(m_Shaders, sh); if (!p) return;
    glUseProgram(p);
    GLint loc = GetUniformLoc(p, name); if (loc < 0) return;
    glUniform3fv(loc, 1, v3);
}

void GLBackend::SetUniformVec4(RShaderHandle sh, const char* name, const float* v4)
{
    if (!v4) return;
    GLuint p = GetProgramFromHandle(m_Shaders, sh); if (!p) return;
    glUseProgram(p);
    GLint loc = GetUniformLoc(p, name); if (loc < 0) return;
    glUniform4fv(loc, 1, v4);
}

void GLBackend::SetUniformMat4(RShaderHandle sh, const char* name, const float* mat4)
{
    if (!mat4) return;
    GLuint p = GetProgramFromHandle(m_Shaders, sh); if (!p) return;
    glUseProgram(p);
    GLint loc = GetUniformLoc(p, name); if (loc < 0) return;
    glUniformMatrix4fv(loc, 1, GL_FALSE, mat4);
}

void GLBackend::LinkUniformBlock(RShaderHandle sh, const char* blockName, uint32_t bindingPoint)
{
    GLuint p = GetProgramFromHandle(m_Shaders, sh); if (!p || !blockName) return;
    GLuint idx = glGetUniformBlockIndex(p, blockName);
    if (idx != GL_INVALID_INDEX) {
        glUniformBlockBinding(p, idx, bindingPoint);
    }
}

FBackendCoordDesc GLBackend::GetCoordConvention() const
{
    FBackendCoordDesc d;
    d.X = FVector3(0, 1, 0); // +X = Right in engine
    d.Y = FVector3(0, 0, 1); // +Y = Up in engine
    d.Z = FVector3(-1, 0, 0); // +Z = Forward in engine

    d.depthZeroToOne = false; // OpenGL [-1,1]
    d.clipSpaceYUp = true;  // assume standard GL
    return d;
}

FMatrix4 GLBackend::BuildProjectionMatrix(const FProjectionDesc& d) const
{
    const float nearP = std::max(d.nearP, 1e-6f);
    const float farP  = std::max(d.farP, nearP + 1e-6f);

    if (d.type == EProjectionType::Perspective)
    {
        return FMatrix4(glm::perspective(glm::radians(d.fovYDeg), d.aspect, nearP, farP));
    }
    else
    {
        const float halfH = d.orthoHalfHeight;
        const float halfW = halfH * d.aspect;
        return FMatrix4(glm::ortho(-halfW, +halfW, -halfH, +halfH, nearP, farP));
    }
}

void GLBackend::UploadLights(const RLightData *lights, uint32_t count)
{
    if (!m_LightUBO) {
        glGenBuffers(1, &m_LightUBO);
        m_LightUBOSize = GLsizeiptr(kMaxLights) * GLsizeiptr(sizeof(FGLLight));
        glBindBuffer(GL_UNIFORM_BUFFER, m_LightUBO);
        glBufferData(GL_UNIFORM_BUFFER, m_LightUBOSize, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, LIGHTS_BINDING, m_LightUBO);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        m_LightStaging.resize(kMaxLights);
    }

    const uint32_t n = std::min<uint32_t>(count, kMaxLights);
    for (uint32_t i = 0; i < n; ++i) {
        m_LightStaging[i].pos_intensity[0] = lights[i].position.x;
        m_LightStaging[i].pos_intensity[1] = lights[i].position.y;
        m_LightStaging[i].pos_intensity[2] = lights[i].position.z;
        m_LightStaging[i].pos_intensity[3] = lights[i].intensity;
        m_LightStaging[i].color_pad[0] = lights[i].color.x;
        m_LightStaging[i].color_pad[1] = lights[i].color.y;
        m_LightStaging[i].color_pad[2] = lights[i].color.z;
        m_LightStaging[i].color_pad[3] = 0.0f;
    }

    glBindBuffer(GL_UNIFORM_BUFFER, m_LightUBO);
    if (n > 0) {
        glBufferSubData(GL_UNIFORM_BUFFER, 0, n * sizeof(FGLLight), m_LightStaging.data());
    }
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    m_LastLightCount = (int)n;
}

void GLBackend::EnableScissorRegion(bool bEnable)
{
    if (bEnable)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);
}

void GLBackend::SetScissorRegion(int x, int y, int width, int height)
{
    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);

    width = std::max(width, 0);
    height = std::max(height, 0);

    const GLint scissorX = viewport[0] + x;
    const GLint scissorY = viewport[1] + viewport[3] - y - height;

    glScissor(scissorX, scissorY, width, height);
}

bool GLBackend::AttachSharedDepthStencil(FGLFramebuffer& framebuffer, const RFramebuffer& desc)
{
    if (!desc.sharedDepthStencilFramebuffer.IsValid())
        return false;

    if (desc.bDepthAsTexture || desc.depthMode != EDepthMode::D24S8)
        return false;

    const auto sourceIt = m_Framebuffers.find(desc.sharedDepthStencilFramebuffer);

    if (sourceIt == m_Framebuffers.end())
        return false;

    const FGLFramebuffer& source = sourceIt->second;
    const TSharedPtr<FGLDepthStencilStorage>& storage = source.depthStencilStorage;

    if (!storage || !storage->renderbuffer)
        return false;

    if (!storage->bHasStencil || storage->depthMode != desc.depthMode)
        return false;

    if (storage->width != desc.width || storage->height != desc.height)
        return false;

    if (storage->samples != std::max(1, desc.samples))
        return false;

    glFramebufferRenderbuffer(
        GL_FRAMEBUFFER,
        GL_DEPTH_STENCIL_ATTACHMENT,
        GL_RENDERBUFFER,
        storage->renderbuffer
    );

    framebuffer.depthStencil = storage->renderbuffer;
    framebuffer.depthStencilStorage = storage;

    return true;
}

void GLBackend::SubmitMesh(RMeshHandle mesh, RShaderHandle shader, const FMatrix4 &transform)
{
    // Lookup mesh
    auto mit = m_Meshes.find(mesh);
    if (mit == m_Meshes.end()) return;
    const FGLMesh& m = mit->second;

    if (m.indexCount == 0 || m.vao == 0) return;

    // Lookup shader
    auto sit = m_Shaders.find(shader);
    if (sit == m_Shaders.end() || sit->second.program == 0) return;
    GLuint prog = sit->second.program;

    // Bind and set model matrix
    glUseProgram(prog);
    if (GLint loc = glGetUniformLocation(prog, "u_Model"); loc >= 0) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, transform.GetValue());
    }

    // Draw
    glBindVertexArray(m.vao);
    if (m.ebo) {
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m.indexCount), GL_UNSIGNED_INT, nullptr);
    } else {
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m.indexCount));
    }
    glBindVertexArray(0);
}

