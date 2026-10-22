#include "apple/metal_renderer_state.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace helengine::apple {
std::uint64_t MetalRenderer::CreateShadowTexture(std::uint32_t width,std::uint32_t height,bool cube) {
    if (!width || !height || width>16384 || height>16384 || (cube && width!=height)) throw std::invalid_argument("Metal shadow target requires valid dimensions and square cube faces");
    if (HasPendingDraws()) throw std::logic_error("Metal shadow allocation requires a completed main frame");
    MTLTextureDescriptor* descriptor=[MTLTextureDescriptor new];
    descriptor.textureType=cube ? MTLTextureTypeCube : MTLTextureType2D;
    descriptor.pixelFormat=MTLPixelFormatR32Float; descriptor.width=width; descriptor.height=height;
    descriptor.storageMode=MTLStorageModeShared; descriptor.usage=MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget;
    id<MTLTexture> texture=[Resources->Device newTextureWithDescriptor:descriptor];
    if (!texture) throw std::runtime_error("Metal sampleable shadow texture allocation failed");
    const auto identity=Resources->NextTexture++;
    Resources->Textures.emplace(identity,texture);
    try { for (uint32_t face=0;face<(cube ? 6u : 1u);++face) RenderShadowMeshes(identity,face,{}); }
    catch (...) { Resources->Textures.erase(identity); throw; }
    return identity;
}

void MetalRenderer::RenderShadowMeshes(std::uint64_t identity,std::uint32_t face,std::span<const ShadowDraw> draws,std::optional<ScissorRect> atlasRegion) {
    if (HasPendingDraws()) throw std::logic_error("Metal shadow rendering must precede queued main-frame submissions");
    auto found=Resources->Textures.find(identity);
    if (found==Resources->Textures.end() || found->second.pixelFormat!=MTLPixelFormatR32Float) throw std::invalid_argument("Metal shadow rendering requires an owned R32Float target");
    id<MTLTexture> target=found->second;
    if (face>=(target.textureType==MTLTextureTypeCube ? 6u : 1u)) throw std::invalid_argument("Metal shadow face is outside target dimensions");
    if (atlasRegion && (target.textureType!=MTLTextureType2D || !atlasRegion->Width || !atlasRegion->Height || static_cast<uint64_t>(atlasRegion->X)+atlasRegion->Width>target.width || static_cast<uint64_t>(atlasRegion->Y)+atlasRegion->Height>target.height)) throw std::invalid_argument("Metal shadow atlas region must remain inside its 2D target");
    for (const auto& draw : draws) {
        if (!Resources->Meshes.contains(draw.Mesh)) throw std::invalid_argument("Metal shadow caster references an unavailable mesh");
        for (float value : draw.Uniforms.WorldViewProjection) if (!std::isfinite(value)) throw std::invalid_argument("Metal shadow clip matrix must be finite");
        for (float value : draw.Uniforms.World) if (!std::isfinite(value)) throw std::invalid_argument("Metal shadow world matrix must be finite");
        for (float value : draw.Uniforms.PointLight) if (!std::isfinite(value)) throw std::invalid_argument("Metal shadow point metadata must be finite");
        if (draw.Uniforms.PointLight[3]<0) throw std::invalid_argument("Metal shadow point range cannot be negative");
    }
    MTLTextureDescriptor* depthDescriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:target.width height:target.height mipmapped:NO];
    depthDescriptor.storageMode=MTLStorageModePrivate; depthDescriptor.usage=MTLTextureUsageRenderTarget;
    id<MTLTexture> depth=[Resources->Device newTextureWithDescriptor:depthDescriptor];
    if (!depth) throw std::runtime_error("Metal shadow depth attachment allocation failed");
    MTLRenderPassDescriptor* pass=[MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture=target; pass.colorAttachments[0].slice=face;
    pass.colorAttachments[0].loadAction=atlasRegion ? MTLLoadActionLoad : MTLLoadActionClear; pass.colorAttachments[0].storeAction=MTLStoreActionStore;
    pass.colorAttachments[0].clearColor=MTLClearColorMake(1,1,1,1);
    pass.depthAttachment.texture=depth; pass.depthAttachment.loadAction=MTLLoadActionClear; pass.depthAttachment.storeAction=MTLStoreActionDontCare; pass.depthAttachment.clearDepth=1;
    id<MTLCommandBuffer> command=[Resources->Queue commandBuffer];
    if (!command) throw std::runtime_error("Metal shadow command allocation failed");
    id<MTLRenderCommandEncoder> encoder=[command renderCommandEncoderWithDescriptor:pass];
    if (!encoder) throw std::runtime_error("Metal shadow encoder allocation failed");
    [encoder setRenderPipelineState:Resources->ShadowPipeline]; [encoder setDepthStencilState:Resources->MeshDepthState];
    [encoder setCullMode:MTLCullModeNone];
    if (atlasRegion) {
        const MTLViewport viewport={static_cast<double>(atlasRegion->X),static_cast<double>(atlasRegion->Y),static_cast<double>(atlasRegion->Width),static_cast<double>(atlasRegion->Height),0,1};
        const MTLScissorRect scissor={atlasRegion->X,atlasRegion->Y,atlasRegion->Width,atlasRegion->Height};
        [encoder setViewport:viewport]; [encoder setScissorRect:scissor];
    }
    for (const auto& draw : draws) {
        const auto& mesh=Resources->Meshes.at(draw.Mesh);
        [encoder setVertexBuffer:mesh.Vertices offset:0 atIndex:0];
        [encoder setVertexBytes:&draw.Uniforms length:sizeof(ShadowDrawUniform) atIndex:1];
        [encoder setFragmentBytes:&draw.Uniforms length:sizeof(ShadowDrawUniform) atIndex:1];
        [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:mesh.IndexCount indexType:MTLIndexTypeUInt32 indexBuffer:mesh.Indices indexBufferOffset:0];
    }
    [encoder endEncoding]; [command commit]; [command waitUntilCompleted];
    if (command.status!=MTLCommandBufferStatusCompleted) throw std::runtime_error("Metal shadow GPU execution failed: "+std::string(command.error ? command.error.localizedDescription.UTF8String : "no SDK diagnostic"));
}

std::vector<float> MetalRenderer::ReadShadowTexture(std::uint64_t identity,std::uint32_t face) const {
    auto found=Resources->Textures.find(identity);
    if (found==Resources->Textures.end() || found->second.pixelFormat!=MTLPixelFormatR32Float) throw std::invalid_argument("Metal shadow readback requires an owned float depth target");
    id<MTLTexture> target=found->second;
    if (face>=(target.textureType==MTLTextureTypeCube ? 6u : 1u)) throw std::invalid_argument("Metal shadow readback face is out of bounds");
    std::vector<float> pixels(target.width*target.height);
    [target getBytes:pixels.data() bytesPerRow:target.width*sizeof(float) bytesPerImage:pixels.size()*sizeof(float) fromRegion:MTLRegionMake2D(0,0,target.width,target.height) mipmapLevel:0 slice:face];
    return pixels;
}
}
