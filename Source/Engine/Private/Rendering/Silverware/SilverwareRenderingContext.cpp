// Copyright 2026 JesseTheCatLover. All Rights Reserved.

#include "SilverwareRenderingContext.h"

#include "Rendering/RObjects.h"

#include <cstddef>
#include <limits>
#include <vector>

SilverwareRenderingContext::SilverwareRenderingContext(
    IRenderDevice& renderDevice,
    IRenderTargetProcessor& renderTargetProcessor)
    : m_RenderDevice(renderDevice),
      m_RenderTargetProcessor(renderTargetProcessor)
{
}
SilverwareRenderingContext::~SilverwareRenderingContext()
{
    while (PopRenderTarget())
    {
    }

    for (const auto& [_, target] : m_RenderTargets)
        m_RenderDevice.DestroyRenderTarget(target.resource);

    m_RenderTargets.clear();

    for (const auto& [_, mesh] : m_Geometries)
        m_RenderDevice.DestroyMesh(mesh);

    for (const auto& [_, texture] : m_Textures)
        if (texture.bOwned && texture.resource.IsValid())
            m_RenderDevice.DestroyTexture(texture.resource);
}

void SilverwareRenderingContext::SetViewportSize(int width, int height)
{
    m_ViewportWidth = width;
    m_ViewportHeight = height;
}

Silverware::FGeometryHandle SilverwareRenderingContext::CreateGeometry(
    std::span<const Silverware::FRenderVertex> vertices,
    std::span<const std::uint32_t> indices)
{
    if (vertices.empty() || m_NextGeometryHandle == 0)
        return {};

    if (indices.empty() && vertices.size() % 3 != 0)
        return {};

    if (!indices.empty() && indices.size() % 3 != 0)
        return {};

    for (const std::uint32_t index : indices)
        if (index >= vertices.size())
            return {};

    RMesh mesh{};
    mesh.vertexStride = static_cast<Rint>(sizeof(float) * 8);

    mesh.attributes =
    {
        { 0, 0,                              2 },
        { 1, static_cast<Rint>(sizeof(float) * 2), 2 },
        { 2, static_cast<Rint>(sizeof(float) * 4), 4 }
    };

    mesh.vertices.reserve(vertices.size() * 8);

    for (const Silverware::FRenderVertex& vertex : vertices)
    {
        mesh.vertices.insert(mesh.vertices.end(),
        {
            vertex.x, vertex.y,
            vertex.u, vertex.v,
            vertex.r, vertex.g, vertex.b, vertex.a
        });
    }

    mesh.indices.assign(indices.begin(), indices.end());

    const RMeshHandle meshHandle = m_RenderDevice.CreateMesh(mesh);

    if (!meshHandle.IsValid())
        return {};

    const std::uint64_t id = m_NextGeometryHandle++;
    m_Geometries.emplace(id, meshHandle);

    return { id };
}

void SilverwareRenderingContext::DestroyGeometry(
    Silverware::FGeometryHandle geometry)
{
    if (!geometry.IsValid())
        return;

    const auto it = m_Geometries.find(geometry.Value);

    if (it == m_Geometries.end())
        return;

    m_RenderDevice.DestroyMesh(it->second);
    m_Geometries.erase(it);
}

Silverware::FTextureHandle SilverwareRenderingContext::CreateTexture(
    int width,
    int height,
    std::span<const std::byte> rgbaPixels)
{
    if (width <= 0 || height <= 0 || m_NextTextureHandle == 0)
        return {};

    const std::size_t w = static_cast<std::size_t>(width);
    const std::size_t h = static_cast<std::size_t>(height);
    constexpr std::size_t BytesPerPixel = 4;

    if (w > std::numeric_limits<std::size_t>::max() / BytesPerPixel / h)
        return {};

    if (rgbaPixels.size() != w * h * BytesPerPixel)
        return {};

    RTexture textureData{};
    textureData.type = ETexType::Tex2D;
    textureData.format = ETexFormat::RGBA8;
    textureData.width = width;
    textureData.height = height;
    textureData.channels = 4;
    textureData.data = rgbaPixels.data();

    textureData.bGenerateMipmaps = false;
    textureData.bUseAnisotropy = false;
    textureData.bSRGB = false;

    textureData.wrapS = ETexWrap::ClampEdge;
    textureData.wrapT = ETexWrap::ClampEdge;
    textureData.minFilter = ETexFilter::Linear;
    textureData.magFilter = ETexFilter::Linear;

    const RTextureHandle textureHandle = m_RenderDevice.CreateTexture(textureData);

    if (!textureHandle.IsValid())
        return {};

    const std::uint64_t id = m_NextTextureHandle++;
    m_Textures.emplace(id, FTextureEntry{textureHandle, true});

    return { id };
}

void SilverwareRenderingContext::DestroyTexture(Silverware::FTextureHandle texture)
{
    if (!texture.IsValid())
        return;

    const auto it = m_Textures.find(texture.Value);

    if (it == m_Textures.end())
        return;

    if (it->second.renderTargetOwner != 0)
    {
        const auto targetIt = m_RenderTargets.find(it->second.renderTargetOwner);

        if (targetIt != m_RenderTargets.end() &&
            targetIt->second.texture.Value == texture.Value)
            targetIt->second.texture = {};
    }

    if (it->second.bOwned && it->second.resource.IsValid())
        m_RenderDevice.DestroyTexture(it->second.resource);

    m_Textures.erase(it);
}

Silverware::FTextureHandle SilverwareRenderingContext::CreateTextureFromRenderTargetRegion(
    Silverware::FRenderTargetHandle source, int x, int y, int width, int height)
{
    if (!source.IsValid() || x < 0 || y < 0 ||
        width <= 0 || height <= 0 || m_NextTextureHandle == 0)
        return {};

    const auto sourceIt = m_RenderTargets.find(source.Value);

    if (sourceIt == m_RenderTargets.end())
        return {};

    const FRenderTargetEntry& target = sourceIt->second;

    if (x > target.width || y > target.height ||
        width > target.width - x ||
        height > target.height - y)
        return {};

    const RTextureHandle resource =
        m_RenderDevice.CreateTextureFromRenderTargetRegion(
            target.resource, x, y, width, height);

    if (!resource.IsValid())
        return {};

    const std::uint64_t id = m_NextTextureHandle++;

    m_Textures.emplace(id, FTextureEntry{resource, true});

    return {id};
}

Silverware::FTextureHandle SilverwareRenderingContext::CreateExternalTexture(
    Silverware::FExternalTextureReference texture)
{
    if (m_NextTextureHandle == 0)
        return {};

    RTextureHandle resource{};

    if (texture.IsValid())
    {
        if (texture.Value > static_cast<std::uint64_t>(std::numeric_limits<Rint>::max()))
            return {};

        resource = RTextureHandle{static_cast<Rint>(texture.Value)};

        if (!resource.IsValid())
            return {};
    }

    const std::uint64_t id = m_NextTextureHandle++;
    m_Textures.emplace(id, FTextureEntry{resource, false});

    return {id};
}

bool SilverwareRenderingContext::UpdateExternalTexture(
    Silverware::FTextureHandle texture,
    Silverware::FExternalTextureReference externalTexture)
{
    if (!texture.IsValid())
        return false;

    auto it = m_Textures.find(texture.Value);

    if (it == m_Textures.end() || it->second.bOwned)
        return false;

    RTextureHandle resource{};

    if (externalTexture.IsValid())
    {
        if (externalTexture.Value > static_cast<std::uint64_t>(std::numeric_limits<Rint>::max()))
            return false;

        resource = RTextureHandle{static_cast<Rint>(externalTexture.Value)};

        if (!resource.IsValid())
            return false;
    }

    it->second.resource = resource;
    return true;
}

void SilverwareRenderingContext::RenderGeometry(
    Silverware::FGeometryHandle geometry,
    Silverware::FTextureHandle texture,
    float translationX,
    float translationY,
    const Silverware::FRenderTransform& transform,
    bool bWriteColor)
{
    if (!geometry.IsValid() ||
        m_ViewportWidth <= 0 ||
        m_ViewportHeight <= 0)
        return;

    const auto geometryIt = m_Geometries.find(geometry.Value);

    if (geometryIt == m_Geometries.end())
        return;

    RTextureHandle textureHandle{};

    if (texture.IsValid())
    {
        const auto textureIt = m_Textures.find(texture.Value);

        if (textureIt == m_Textures.end() || !textureIt->second.resource.IsValid())
            return;

        textureHandle = textureIt->second.resource;
    }

    m_RenderDevice.SubmitScreenSpaceGeometry(
        geometryIt->second,
        textureHandle,
        translationX,
        translationY,
        m_ViewportWidth,
        m_ViewportHeight,
        bWriteColor,
        transform.values.data());
}

void SilverwareRenderingContext::EnableScissorRegion(bool bEnable)
{
    m_RenderDevice.EnableScissorRegion(bEnable);
}

void SilverwareRenderingContext::SetScissorRegion(int x, int y, int width, int height)
{
    m_RenderDevice.SetScissorRegion(x, y, width, height);
}

Silverware::FRenderTargetHandle SilverwareRenderingContext::CreateRenderTarget(const Silverware::FRenderTargetDesc& desc)
{
    if (!desc.IsValid() || m_NextRenderTargetHandle == 0)
        return {};

    FRenderTargetDesc renderTargetDesc{};
    renderTargetDesc.width = desc.width;
    renderTargetDesc.height = desc.height;
    renderTargetDesc.bHasDepthStencil = desc.bHasDepthStencil;

    const RRenderTargetHandle resource = m_RenderDevice.CreateRenderTarget(renderTargetDesc);

    if (!resource.IsValid())
        return {};

    const std::uint64_t id = m_NextRenderTargetHandle++;

    m_RenderTargets.emplace(id, FRenderTargetEntry{
        resource,
        {},
        desc.width,
        desc.height
    });

    return {id};
}

void SilverwareRenderingContext::DestroyRenderTarget(Silverware::FRenderTargetHandle target)
{
    if (!target.IsValid())
        return;

    const auto it = m_RenderTargets.find(target.Value);

    if (it == m_RenderTargets.end())
        return;

    // A target must not be destroyed while it is in the active
    // render-target nesting hierarchy.
    for (const FRenderTargetStackEntry& entry : m_RenderTargetStack)
        if (entry.target == target.Value)
            return;

    if (it->second.texture.IsValid())
        m_Textures.erase(it->second.texture.Value);

    m_RenderDevice.DestroyRenderTarget(it->second.resource);
    m_RenderTargets.erase(it);
}

Silverware::FTextureHandle SilverwareRenderingContext::GetRenderTargetTexture(
    Silverware::FRenderTargetHandle target)
{
    if (!target.IsValid())
        return {};

    auto it = m_RenderTargets.find(target.Value);

    if (it == m_RenderTargets.end())
        return {};

    // Reuse the existing Silverware alias.
    if (it->second.texture.IsValid())
        return it->second.texture;

    if (m_NextTextureHandle == 0)
        return {};

    const RTextureHandle resource =
        m_RenderDevice.GetRenderTargetColorTexture(it->second.resource);

    if (!resource.IsValid())
        return {};

    const std::uint64_t id = m_NextTextureHandle++;

    const Silverware::FTextureHandle texture{id};

    m_Textures.emplace(id, FTextureEntry{
        resource,
        false,
        target.Value
    });

    it->second.texture = texture;
    return texture;
}

bool SilverwareRenderingContext::PushRenderTarget(Silverware::FRenderTargetHandle target)
{
    if (!target.IsValid())
        return false;

    const auto it = m_RenderTargets.find(target.Value);

    if (it == m_RenderTargets.end())
        return false;

    m_RenderTargetStack.push_back({
        target.Value,
        m_ViewportWidth,
        m_ViewportHeight
    });

    if (!m_RenderDevice.PushRenderTarget(it->second.resource))
    {
        m_RenderTargetStack.pop_back();
        return false;
    }

    m_ViewportWidth = it->second.width;
    m_ViewportHeight = it->second.height;

    return true;
}

bool SilverwareRenderingContext::PopRenderTarget()
{
    if (m_RenderTargetStack.empty())
        return false;

    if (!m_RenderDevice.PopRenderTarget())
        return false;

    const FRenderTargetStackEntry previous = m_RenderTargetStack.back();

    m_RenderTargetStack.pop_back();

    m_ViewportWidth = previous.previousViewportWidth;
    m_ViewportHeight = previous.previousViewportHeight;

    return true;
}

bool SilverwareRenderingContext::ClearRenderTarget(Silverware::FRenderTargetHandle target,
    const Silverware::FRenderTargetClearDesc& desc)
{
    if (!target.IsValid() || !desc.IsValid())
        return false;

    const auto it = m_RenderTargets.find(target.Value);

    if (it == m_RenderTargets.end())
        return false;

    FRenderClearDesc renderClearDesc{};
    renderClearDesc.bClearColor = desc.bClearColor;
    renderClearDesc.bRespectScissor = desc.bRespectScissor;

    for (int i = 0; i < 4; ++i)
        renderClearDesc.color[i] = desc.color[i];

    renderClearDesc.bClearDepth = desc.bClearDepth;
    renderClearDesc.depth = desc.depth;

    renderClearDesc.bClearStencil = desc.bClearStencil;
    renderClearDesc.stencil = desc.stencil;

    return m_RenderDevice.ClearRenderTarget(it->second.resource, renderClearDesc);
}

bool SilverwareRenderingContext::CompositeRenderTargets(
    Silverware::FRenderTargetHandle source,
    Silverware::FRenderTargetHandle destination,
    Silverware::ECompositeBlendMode blendMode)
{
    if (!source.IsValid())
        return false;

    const auto sourceIt = m_RenderTargets.find(source.Value);

    if (sourceIt == m_RenderTargets.end())
        return false;

    RRenderTargetHandle destinationResource{};

    if (destination.IsValid())
    {
        const auto destinationIt = m_RenderTargets.find(destination.Value);

        if (destinationIt == m_RenderTargets.end())
            return false;

        destinationResource = destinationIt->second.resource;
    }

    ERenderTargetBlendMode redleafBlendMode{};

    switch (blendMode)
    {
        case Silverware::ECompositeBlendMode::Blend:
            redleafBlendMode = ERenderTargetBlendMode::Blend;
            break;

        case Silverware::ECompositeBlendMode::Replace:
            redleafBlendMode = ERenderTargetBlendMode::Replace;
            break;

        default:
            return false;
    }

    return m_RenderDevice.CompositeRenderTargets(
        sourceIt->second.resource,
        destinationResource,
        redleafBlendMode);
}

bool SilverwareRenderingContext::ApplyFilter(
    Silverware::FRenderTargetHandle source,
    Silverware::FRenderTargetHandle output,
    const Silverware::FRenderTargetFilterDesc& filter,
    const Silverware::FRenderTargetRegion& region)
{
    if (!source.IsValid() ||
        !output.IsValid() ||
        source.Value == output.Value ||
        !region.IsValid())
        return false;

    const auto sourceIt = m_RenderTargets.find(source.Value);
    const auto outputIt = m_RenderTargets.find(output.Value);

    if (sourceIt == m_RenderTargets.end() ||
        outputIt == m_RenderTargets.end())
        return false;

    const FRenderTargetEntry& sourceTarget = sourceIt->second;
    const FRenderTargetEntry& outputTarget = outputIt->second;

    if (sourceTarget.width != outputTarget.width ||
        sourceTarget.height != outputTarget.height)
        return false;

    if (region.x > sourceTarget.width ||
        region.y > sourceTarget.height ||
        region.width > sourceTarget.width - region.x ||
        region.height > sourceTarget.height - region.y)
        return false;

    FRenderTargetFilterDesc renderFilter{};

    switch (filter.type)
    {
        case Silverware::ERenderTargetFilterType::GaussianBlur:
            renderFilter.type = ::ERenderTargetFilterType::GaussianBlur;
            break;

        default:
            return false;
    }

    renderFilter.sigma = filter.sigma;

    const FRenderTargetRegion renderRegion{
        region.x,
        region.y,
        region.width,
        region.height
    };

    return m_RenderTargetProcessor.ApplyFilter(
        sourceTarget.resource,
        outputTarget.resource,
        renderFilter,
        renderRegion);
}

void SilverwareRenderingContext::ClearStencilBuffer(std::uint8_t value)
{
    m_RenderDevice.ClearStencilBuffer(value);
}

void SilverwareRenderingContext::SetClipMaskStencilState(const Silverware::FClipMaskStencilState& state)
{
    ::FClipMaskStencilState renderState{};
    renderState.bEnable = state.bEnable;
    renderState.bWrite = state.bWrite;
    renderState.reference = state.reference;

    switch (state.operation)
    {
        case Silverware::EClipMaskStencilOperation::Keep:
            renderState.operation = ::EClipMaskStencilOperation::Keep;
            break;

        case Silverware::EClipMaskStencilOperation::Replace:
            renderState.operation = ::EClipMaskStencilOperation::Replace;
            break;

        case Silverware::EClipMaskStencilOperation::Increment:
            renderState.operation = ::EClipMaskStencilOperation::Increment;
            break;
    }

    m_RenderDevice.SetClipMaskStencilState(renderState);
}
