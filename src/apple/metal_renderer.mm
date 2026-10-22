#include "apple/metal_renderer_state.hpp"
#import <Foundation/Foundation.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <cmath>
#include <stdexcept>

namespace helengine::apple {
    MetalRenderer::State::State() {
        Device = MTLCreateSystemDefaultDevice();
        if (!Device) throw std::runtime_error("Metal initialization: no hardware device available");
        Queue = [Device newCommandQueue];
        if (!Queue) throw std::runtime_error("Metal initialization: command queue allocation failed");
        NSURL* executable = [[NSBundle mainBundle] executableURL];
        NSURL* libraryUrl = [[[NSBundle mainBundle] resourceURL] URLByAppendingPathComponent:@"helengine.metallib"];
        if (![[NSFileManager defaultManager] fileExistsAtPath:[libraryUrl path]]) {
            libraryUrl = [[executable URLByDeletingLastPathComponent] URLByAppendingPathComponent:@"helengine.metallib"];
        }
        NSError* error = nil;
        id<MTLLibrary> library = [Device newLibraryWithURL:libraryUrl error:&error];
        if (!library) throw std::runtime_error("Metal shader library: " + std::string([[error localizedDescription] UTF8String]));
        MTLRenderPipelineDescriptor* descriptor = [MTLRenderPipelineDescriptor new];
        descriptor.vertexFunction = [library newFunctionWithName:@"primitive_vertex"];
        descriptor.fragmentFunction = [library newFunctionWithName:@"primitive_fragment"];
        descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        descriptor.colorAttachments[0].blendingEnabled = YES;
        descriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        descriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        descriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
        descriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        Pipeline = [Device newRenderPipelineStateWithDescriptor:descriptor error:&error];
        if (!Pipeline) throw std::runtime_error("Metal primitive pipeline: " + std::string([[error localizedDescription] UTF8String]));
        descriptor.fragmentFunction = [library newFunctionWithName:@"rounded_fragment"];
        RoundedPipeline = [Device newRenderPipelineStateWithDescriptor:descriptor error:&error];
        if (!RoundedPipeline) throw std::runtime_error("Metal rounded rectangle pipeline: " + std::string([[error localizedDescription] UTF8String]));
    }

    void MetalRenderer::State::Render(id<MTLTexture> target, id<CAMetalDrawable> drawable) {
        id<MTLCommandBuffer> command = [Queue commandBuffer];
        if (!command) throw std::runtime_error("Metal frame: command buffer allocation failed");
        MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = target;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
        id<MTLRenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
        if (!encoder) throw std::runtime_error("Metal frame: render encoder allocation failed");
        [encoder setRenderPipelineState:Pipeline];
        for (const DrawPacket& draw : Draws) {
            auto texture = Textures.find(draw.Texture);
            if (!draw.Rounded && texture == Textures.end()) {
                [encoder endEncoding];
                throw std::runtime_error("Metal frame: a queued texture was released before submission");
            }
            ScissorRect clip = draw.Clip.value_or(ScissorRect{0,0,static_cast<std::uint32_t>(target.width),static_cast<std::uint32_t>(target.height)});
            if (clip.Width == 0 || clip.Height == 0 || clip.X >= target.width || clip.Y >= target.height) continue;
            const MTLScissorRect scissor = {clip.X, clip.Y, std::min<std::size_t>(clip.Width, target.width-clip.X), std::min<std::size_t>(clip.Height, target.height-clip.Y)};
            [encoder setScissorRect:scissor];
            id<MTLBuffer> vertices = [Device newBufferWithBytes:draw.Vertices.data() length:draw.Vertices.size() * sizeof(Vertex) options:MTLResourceStorageModeShared];
            if (!vertices) {
                [encoder endEncoding];
                throw std::runtime_error("Metal frame: vertex buffer allocation failed");
            }
            [encoder setVertexBuffer:vertices offset:0 atIndex:0];
            [encoder setRenderPipelineState:draw.Rounded ? RoundedPipeline : Pipeline];
            if (draw.Rounded) [encoder setFragmentBytes:&draw.Shape length:sizeof(draw.Shape) atIndex:0];
            else [encoder setFragmentTexture:texture->second atIndex:0];
            [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:draw.Vertices.size()];
        }
        [encoder endEncoding];
        if (drawable) [command presentDrawable:drawable];
        [command commit];
        [command waitUntilCompleted];
        if (command.status == MTLCommandBufferStatusError) throw std::runtime_error("Metal GPU submission: " + std::string([[command.error localizedDescription] UTF8String]));
        Draws.clear();
    }

    MetalRenderer::MetalRenderer() : Resources(std::make_unique<State>()) {}
    MetalRenderer::~MetalRenderer() = default;
    std::string MetalRenderer::DeviceName() const { return [[Resources->Device name] UTF8String]; }

    std::uint64_t MetalRenderer::UploadTexture(std::uint32_t width, std::uint32_t height, std::span<const std::uint8_t> rgba) {
        if (width == 0 || height == 0 || static_cast<std::uint64_t>(width) * height > std::numeric_limits<std::size_t>::max() / 4 || rgba.size() != static_cast<std::size_t>(width) * height * 4) {
            throw std::invalid_argument("Metal texture upload requires nonzero dimensions and exact RGBA8 payload");
        }
        MTLTextureDescriptor* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:width height:height mipmapped:NO];
        descriptor.storageMode = MTLStorageModeShared;
        descriptor.usage = MTLTextureUsageShaderRead;
        id<MTLTexture> texture = [Resources->Device newTextureWithDescriptor:descriptor];
        if (!texture) throw std::runtime_error("Metal texture allocation failed");
        [texture replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0 withBytes:rgba.data() bytesPerRow:width * 4];
        const std::uint64_t identity = Resources->NextTexture++;
        Resources->Textures.emplace(identity, texture);
        return identity;
    }

    void MetalRenderer::ReleaseTexture(std::uint64_t texture) {
        if (std::any_of(Resources->Draws.begin(), Resources->Draws.end(), [texture](const DrawPacket& draw) { return draw.Texture == texture; })) throw std::logic_error("Metal texture release: resource is still queued for drawing");
        if (Resources->Textures.erase(texture) != 1) throw std::invalid_argument("Metal texture release: unknown resource identity");
    }

    void MetalRenderer::UpdateTexture(std::uint64_t identity, std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height, std::span<const std::uint8_t> rgba, std::size_t rowPitch) {
        auto resource = Resources->Textures.find(identity);
        if (resource == Resources->Textures.end()) throw std::invalid_argument("Metal texture update: unknown resource identity");
        if (std::any_of(Resources->Draws.begin(), Resources->Draws.end(), [identity](const DrawPacket& draw) { return !draw.Rounded && draw.Texture == identity; })) throw std::logic_error("Metal texture update: resource is still queued for drawing");
        id<MTLTexture> texture = resource->second;
        if (width == 0 || height == 0 || static_cast<std::uint64_t>(x)+width > texture.width || static_cast<std::uint64_t>(y)+height > texture.height || rowPitch < static_cast<std::size_t>(width)*4 || rowPitch%4 != 0 || height-1 > (std::numeric_limits<std::size_t>::max()-static_cast<std::size_t>(width)*4)/rowPitch || rgba.size() < rowPitch*(height-1)+static_cast<std::size_t>(width)*4) throw std::invalid_argument("Metal texture update: invalid region, pitch or RGBA8 payload");
        [texture replaceRegion:MTLRegionMake2D(x,y,width,height) mipmapLevel:0 withBytes:rgba.data() bytesPerRow:rowPitch];
    }

    void MetalRenderer::Submit(std::span<const Vertex> vertices, std::uint64_t texture) {
        if (vertices.empty() || vertices.size() % 3 != 0) throw std::invalid_argument("Metal triangles require a nonempty multiple of three vertices");
        if (!Resources->Textures.contains(texture)) throw std::invalid_argument("Metal draw: texture resource is unavailable");
        DrawPacket packet;
        packet.Vertices.assign(vertices.begin(),vertices.end());
        packet.Texture = texture;
        packet.Clip = Resources->Clip;
        Resources->Draws.push_back(std::move(packet));
    }

    void MetalRenderer::SetClip(std::optional<ScissorRect> rectangle) { Resources->Clip = rectangle; }

    void MetalRenderer::SubmitRoundedRect(std::span<const Vertex> vertices, float width, float height, float radius, float border, std::array<float,4> borderColor, std::uint32_t corners) {
        if (vertices.empty() || vertices.size()%3 != 0 || !std::isfinite(width) || !std::isfinite(height) || !std::isfinite(radius) || !std::isfinite(border) || width <= 0 || height <= 0 || radius < 0 || border < 0 || corners > 15) throw std::invalid_argument("Metal rounded rectangle requires finite dimensions, nonnegative radii and a valid corner mask");
        DrawPacket packet;
        packet.Vertices.assign(vertices.begin(),vertices.end());
        packet.Texture = 0;
        packet.Clip = Resources->Clip;
        packet.Rounded = true;
        packet.Shape.Parameters[0] = width;
        packet.Shape.Parameters[1] = height;
        packet.Shape.Parameters[2] = radius;
        packet.Shape.Parameters[3] = border;
        std::copy(borderColor.begin(),borderColor.end(),packet.Shape.BorderColor);
        packet.Shape.Flags[0] = corners;
        Resources->Draws.push_back(std::move(packet));
    }

    std::vector<std::uint8_t> MetalRenderer::RenderOffscreen(std::uint32_t width, std::uint32_t height) {
        if (width == 0 || height == 0 || static_cast<std::uint64_t>(width)*height > std::numeric_limits<std::size_t>::max()/4) throw std::invalid_argument("Metal offscreen target requires nonzero dimensions within addressable memory");
        MTLTextureDescriptor* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:width height:height mipmapped:NO];
        descriptor.storageMode = MTLStorageModeShared;
        descriptor.usage = MTLTextureUsageRenderTarget;
        id<MTLTexture> target = [Resources->Device newTextureWithDescriptor:descriptor];
        if (!target) throw std::runtime_error("Metal offscreen target allocation failed");
        Resources->Render(target, nil);
        std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width)*height*4);
        [target getBytes:rgba.data() bytesPerRow:static_cast<std::size_t>(width)*4 fromRegion:MTLRegionMake2D(0,0,width,height) mipmapLevel:0];
        for (std::size_t pixel=0; pixel<rgba.size(); pixel+=4) std::swap(rgba[pixel],rgba[pixel+2]);
        return rgba;
    }

    bool MetalRenderer::Present(void* nativeLayer, std::uint32_t width, std::uint32_t height) {
        if (!nativeLayer) throw std::invalid_argument("Metal presentation requires an SDL CAMetalLayer");
        if (width == 0 || height == 0) { DiscardFrame(); return false; }
        CAMetalLayer* layer = (__bridge CAMetalLayer*)nativeLayer;
        layer.device = Resources->Device;
        layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        layer.drawableSize = CGSizeMake(width, height);
        id<CAMetalDrawable> drawable = [layer nextDrawable];
        if (!drawable) { DiscardFrame(); return false; }
        Resources->Render(drawable.texture, drawable);
        return true;
    }

    void MetalRenderer::DiscardFrame() { Resources->Draws.clear(); }

    std::vector<std::uint8_t> MetalRenderer::ValidateOffscreen() {
        if (!Resources->Draws.empty()) throw std::logic_error("Metal offscreen validation requires an empty frame");
        const std::array<std::uint8_t, 4> white = {255,255,255,255};
        const auto texture = UploadTexture(1, 1, white);
        const std::array<Vertex, 6> red = {{{{-1,-1},{0,0},{1,0,0,1}},{{1,-1},{1,0},{1,0,0,1}},{{1,1},{1,1},{1,0,0,1}},{{-1,-1},{0,0},{1,0,0,1}},{{1,1},{1,1},{1,0,0,1}},{{-1,1},{0,1},{1,0,0,1}}}};
        auto blue = red;
        for (auto& vertex : blue) { vertex.Position[0] *= 0.5f; vertex.Position[1] *= 0.5f; vertex.Color[0] = 0; vertex.Color[2] = 1; vertex.Color[3] = 0.5f; }
        Submit(red, texture);
        Submit(blue, texture);
        bool retained = false;
        try { ReleaseTexture(texture); } catch (const std::logic_error&) { retained = true; }
        if (!retained) throw std::runtime_error("queued texture ownership was not enforced");
        auto rgba = RenderOffscreen(64,64);
        ReleaseTexture(texture);
        const std::size_t center = (32 * 64 + 32) * 4;
        if (rgba[0] != 255 || rgba[1] != 0 || rgba[2] != 0 || rgba[3] != 255) throw std::runtime_error("Metal texture/color readback mismatch");
        if (rgba[center] < 126 || rgba[center] > 129 || rgba[center+2] < 126 || rgba[center+2] > 129 || rgba[center+1] != 0 || rgba[center+3] != 255) throw std::runtime_error("Metal ordered alpha blend readback mismatch");
        return rgba;
    }
}
