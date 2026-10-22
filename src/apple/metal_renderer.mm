#include "apple/metal_renderer_state.hpp"
#import <Foundation/Foundation.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <cmath>
#include <cstdlib>
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
        descriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
        RoundedDepthPipeline = [Device newRenderPipelineStateWithDescriptor:descriptor error:&error];
        if (!RoundedDepthPipeline) throw std::runtime_error("Metal rounded depth-compatible pipeline: " + std::string([[error localizedDescription] UTF8String]));
        descriptor.fragmentFunction = [library newFunctionWithName:@"primitive_fragment"];
        PrimitiveDepthPipeline = [Device newRenderPipelineStateWithDescriptor:descriptor error:&error];
        if (!PrimitiveDepthPipeline) throw std::runtime_error("Metal primitive depth-compatible pipeline: " + std::string([[error localizedDescription] UTF8String]));
        descriptor.vertexFunction = [library newFunctionWithName:@"mesh_vertex"];
        MeshPipeline = [Device newRenderPipelineStateWithDescriptor:descriptor error:&error];
        if (!MeshPipeline) throw std::runtime_error("Metal indexed mesh pipeline: " + std::string([[error localizedDescription] UTF8String]));
        MTLDepthStencilDescriptor* depth = [MTLDepthStencilDescriptor new];
        depth.depthCompareFunction = MTLCompareFunctionLess;
        depth.depthWriteEnabled = YES;
        MeshDepthState = [Device newDepthStencilStateWithDescriptor:depth];
        depth.depthCompareFunction = MTLCompareFunctionAlways;
        depth.depthWriteEnabled = NO;
        OverlayDepthState = [Device newDepthStencilStateWithDescriptor:depth];
        if (!MeshDepthState || !OverlayDepthState) throw std::runtime_error("Metal depth state allocation failed");
        MTLSamplerDescriptor* sampler = [MTLSamplerDescriptor new];
        sampler.minFilter = MTLSamplerMinMagFilterLinear;
        sampler.magFilter = MTLSamplerMinMagFilterLinear;
        sampler.sAddressMode = MTLSamplerAddressModeClampToEdge;
        sampler.tAddressMode = MTLSamplerAddressModeClampToEdge;
        MaterialSampler = [Device newSamplerStateWithDescriptor:sampler];
        if (!MaterialSampler) throw std::runtime_error("Metal standard material sampler allocation failed");
    }

    id<MTLLibrary> MetalRenderer::State::LoadLibrary(std::span<const std::uint8_t> bytes) {
        if (bytes.size()<4 || bytes[0]!='M' || bytes[1]!='T' || bytes[2]!='L' || bytes[3]!='B') throw std::invalid_argument("Authored material requires cooked Metal library bytes");
        void* copy = std::malloc(bytes.size());
        if (!copy) throw std::bad_alloc();
        std::memcpy(copy,bytes.data(),bytes.size());
        dispatch_data_t data = dispatch_data_create(copy,bytes.size(),dispatch_get_global_queue(QOS_CLASS_DEFAULT,0),DISPATCH_DATA_DESTRUCTOR_FREE);
        NSError* error = nil;
        id<MTLLibrary> library = [Device newLibraryWithData:data error:&error];
        if (!library) throw std::runtime_error("Authored Metal library rejected: "+std::string(error ? [[error localizedDescription] UTF8String] : "SDK returned no diagnostic"));
        return library;
    }

    // Keep the iOS 15 reflection ABI available while adopting the current SDK for compilation.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    void MetalRenderer::State::ReflectArguments(MaterialResource& material,NSArray<MTLArgument*>* arguments,std::uint32_t stage) {
        for (MTLArgument* argument in arguments) {
            if (!argument.active) continue;
            if (argument.type == MTLArgumentTypeBuffer) {
                if (argument.index == 30 && stage == 1) continue;
                if (argument.index >= 30) throw std::runtime_error("Authored Metal constant buffer overlaps reserved mesh slot 30");
                material.Required.push_back({0,static_cast<std::uint32_t>(argument.index),stage,argument.bufferDataSize});
            } else if (argument.type == MTLArgumentTypeTexture) {
                if (argument.index >= 128 || argument.textureType != MTLTextureType2D) throw std::runtime_error("Authored Metal material currently requires separate 2D texture bindings");
                material.Required.push_back({1,static_cast<std::uint32_t>(argument.index),stage,0});
            } else if (argument.type == MTLArgumentTypeSampler) {
                if (argument.index >= 16) throw std::runtime_error("Authored Metal sampler exceeds the native slot limit");
                material.Required.push_back({2,static_cast<std::uint32_t>(argument.index),stage,0});
            } else throw std::runtime_error("Authored Metal material has an unsupported reflected resource kind");
        }
    }
#pragma clang diagnostic pop

    void MetalRenderer::State::Render(id<MTLTexture> target, id<CAMetalDrawable> drawable) {
        id<MTLCommandBuffer> command = [Queue commandBuffer];
        if (!command) throw std::runtime_error("Metal frame: command buffer allocation failed");
        MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = target;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
        const bool hasMesh = std::any_of(Draws.begin(),Draws.end(),[](const DrawPacket& draw) { return draw.Mesh != 0; });
        if (hasMesh) {
            MTLTextureDescriptor* depthDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:target.width height:target.height mipmapped:NO];
            depthDescriptor.storageMode = MTLStorageModePrivate;
            depthDescriptor.usage = MTLTextureUsageRenderTarget;
            id<MTLTexture> depthTarget = [Device newTextureWithDescriptor:depthDescriptor];
            if (!depthTarget) throw std::runtime_error("Metal depth attachment allocation failed");
            pass.depthAttachment.texture = depthTarget;
            pass.depthAttachment.loadAction = MTLLoadActionClear;
            pass.depthAttachment.storeAction = MTLStoreActionDontCare;
            pass.depthAttachment.clearDepth = 1.0;
        }
        id<MTLRenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
        if (!encoder) throw std::runtime_error("Metal frame: render encoder allocation failed");
        for (const DrawPacket& draw : Draws) {
            auto texture = Textures.find(draw.Texture);
            if (!draw.Rounded && draw.Material == 0 && texture == Textures.end()) {
                [encoder endEncoding];
                throw std::runtime_error("Metal frame: a queued texture was released before submission");
            }
            ScissorRect clip = draw.Clip.value_or(ScissorRect{0,0,static_cast<std::uint32_t>(target.width),static_cast<std::uint32_t>(target.height)});
            if (clip.Width == 0 || clip.Height == 0 || clip.X >= target.width || clip.Y >= target.height) continue;
            const MTLScissorRect scissor = {clip.X, clip.Y, std::min<std::size_t>(clip.Width, target.width-clip.X), std::min<std::size_t>(clip.Height, target.height-clip.Y)};
            [encoder setScissorRect:scissor];
            if (draw.Mesh != 0) {
                auto mesh = Meshes.find(draw.Mesh);
                if (mesh == Meshes.end()) {
                    [encoder endEncoding];
                    throw std::runtime_error("Metal frame: an indexed mesh was released before submission");
                }
                if (draw.Material != 0) {
                    auto material = Materials.find(draw.Material);
                    if (material == Materials.end()) {
                        [encoder endEncoding];
                        throw std::runtime_error("Metal frame: authored material was released before submission");
                    }
                    [encoder setRenderPipelineState:material->second.Pipeline];
                    [encoder setDepthStencilState:MeshDepthState];
                    [encoder setVertexBuffer:mesh->second.Vertices offset:0 atIndex:30];
                    for (const auto& binding : draw.MaterialBuffers) {
                        id<MTLBuffer> constants = [Device newBufferWithBytes:binding.Bytes.data() length:binding.Bytes.size() options:MTLResourceStorageModeShared];
                        if (!constants) { [encoder endEncoding]; throw std::runtime_error("Metal authored constant buffer allocation failed"); }
                        if (binding.Stages&1) [encoder setVertexBuffer:constants offset:0 atIndex:binding.Slot];
                        if (binding.Stages&2) [encoder setFragmentBuffer:constants offset:0 atIndex:binding.Slot];
                    }
                    for (const auto& binding : draw.MaterialTextures) {
                        auto native = Textures.find(binding.Texture);
                        if (native == Textures.end()) { [encoder endEncoding]; throw std::runtime_error("Metal frame: authored texture was released before submission"); }
                        if (binding.Stages&1) [encoder setVertexTexture:native->second atIndex:binding.Slot];
                        if (binding.Stages&2) [encoder setFragmentTexture:native->second atIndex:binding.Slot];
                    }
                    for (const auto& requirement : material->second.Required) {
                        if (requirement.Kind != 2) continue;
                        if (requirement.Stage&1) [encoder setVertexSamplerState:MaterialSampler atIndex:requirement.Slot];
                        if (requirement.Stage&2) [encoder setFragmentSamplerState:MaterialSampler atIndex:requirement.Slot];
                    }
                    [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:mesh->second.IndexCount indexType:MTLIndexTypeUInt32 indexBuffer:mesh->second.Indices indexBufferOffset:0];
                    continue;
                }
                [encoder setRenderPipelineState:MeshPipeline];
                [encoder setDepthStencilState:MeshDepthState];
                [encoder setVertexBuffer:mesh->second.Vertices offset:0 atIndex:0];
                [encoder setVertexBytes:&draw.MeshUniforms length:sizeof(draw.MeshUniforms) atIndex:1];
                [encoder setFragmentTexture:texture->second atIndex:0];
                [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:mesh->second.IndexCount indexType:MTLIndexTypeUInt32 indexBuffer:mesh->second.Indices indexBufferOffset:0];
                continue;
            }
            id<MTLBuffer> vertices = [Device newBufferWithBytes:draw.Vertices.data() length:draw.Vertices.size() * sizeof(Vertex) options:MTLResourceStorageModeShared];
            if (!vertices) {
                [encoder endEncoding];
                throw std::runtime_error("Metal frame: vertex buffer allocation failed");
            }
            [encoder setVertexBuffer:vertices offset:0 atIndex:0];
            [encoder setRenderPipelineState:draw.Rounded ? (hasMesh ? RoundedDepthPipeline : RoundedPipeline) : (hasMesh ? PrimitiveDepthPipeline : Pipeline)];
            [encoder setDepthStencilState:OverlayDepthState];
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
        if (std::any_of(Resources->Draws.begin(), Resources->Draws.end(), [texture](const DrawPacket& draw) { return draw.Texture == texture || std::any_of(draw.MaterialTextures.begin(),draw.MaterialTextures.end(),[texture](const MaterialTexture& binding) { return binding.Texture == texture; }); })) throw std::logic_error("Metal texture release: resource is still queued for drawing");
        if (Resources->Textures.erase(texture) != 1) throw std::invalid_argument("Metal texture release: unknown resource identity");
    }

    void MetalRenderer::UpdateTexture(std::uint64_t identity, std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height, std::span<const std::uint8_t> rgba, std::size_t rowPitch) {
        auto resource = Resources->Textures.find(identity);
        if (resource == Resources->Textures.end()) throw std::invalid_argument("Metal texture update: unknown resource identity");
        if (std::any_of(Resources->Draws.begin(), Resources->Draws.end(), [identity](const DrawPacket& draw) { return (!draw.Rounded && draw.Texture == identity) || std::any_of(draw.MaterialTextures.begin(),draw.MaterialTextures.end(),[identity](const MaterialTexture& binding) { return binding.Texture == identity; }); })) throw std::logic_error("Metal texture update: resource is still queued for drawing");
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

    std::uint64_t MetalRenderer::UploadMesh(std::span<const MeshVertex> vertices,std::span<const std::uint32_t> indices) {
        if (vertices.empty() || indices.empty() || indices.size()%3 != 0 || indices.size()>std::numeric_limits<std::uint32_t>::max() || vertices.size()>std::numeric_limits<std::size_t>::max()/sizeof(MeshVertex)) throw std::invalid_argument("Metal mesh requires nonempty indexed triangles within buffer limits");
        for (const auto index : indices) if (index>=vertices.size()) throw std::invalid_argument("Metal mesh index exceeds vertex count");
        for (const auto& vertex : vertices) {
            for (const float value : vertex.Position) if (!std::isfinite(value)) throw std::invalid_argument("Metal mesh position must be finite");
            for (const float value : vertex.Normal) if (!std::isfinite(value)) throw std::invalid_argument("Metal mesh normal must be finite");
            for (const float value : vertex.Uv) if (!std::isfinite(value)) throw std::invalid_argument("Metal mesh UV must be finite");
        }
        MeshResource mesh;
        mesh.Vertices = [Resources->Device newBufferWithBytes:vertices.data() length:vertices.size_bytes() options:MTLResourceStorageModeShared];
        mesh.Indices = [Resources->Device newBufferWithBytes:indices.data() length:indices.size_bytes() options:MTLResourceStorageModeShared];
        mesh.IndexCount = static_cast<std::uint32_t>(indices.size());
        if (!mesh.Vertices || !mesh.Indices) throw std::runtime_error("Metal indexed mesh buffer allocation failed");
        const auto identity = Resources->NextMesh++;
        Resources->Meshes.emplace(identity,mesh);
        return identity;
    }

    void MetalRenderer::SubmitMesh(std::uint64_t mesh,std::uint64_t texture,const MeshDrawUniform& uniforms) {
        if (!Resources->Meshes.contains(mesh)) throw std::invalid_argument("Metal indexed draw references an unknown mesh");
        if (!Resources->Textures.contains(texture)) throw std::invalid_argument("Metal indexed draw requires an explicitly uploaded texture");
        for (const auto value : uniforms.Transform) if (!std::isfinite(value)) throw std::invalid_argument("Metal mesh transform must be finite");
        for (const auto value : uniforms.Color) if (!std::isfinite(value)) throw std::invalid_argument("Metal mesh material color must be finite");
        DrawPacket packet;
        packet.Texture = texture;
        packet.Mesh = mesh;
        packet.MeshUniforms = uniforms;
        packet.Clip = Resources->Clip;
        Resources->Draws.push_back(std::move(packet));
    }

    void MetalRenderer::ReleaseMesh(std::uint64_t mesh) {
        if (std::any_of(Resources->Draws.begin(),Resources->Draws.end(),[mesh](const DrawPacket& draw) { return draw.Mesh == mesh; })) throw std::logic_error("Metal mesh release: resource is still queued for drawing");
        if (Resources->Meshes.erase(mesh)!=1) throw std::invalid_argument("Metal mesh release: unknown resource identity");
    }

    std::uint64_t MetalRenderer::UploadMaterial(std::span<const std::uint8_t> vertexLibrary,std::string_view vertexEntry,std::span<const std::uint8_t> fragmentLibrary,std::string_view fragmentEntry) {
        if (vertexEntry.empty() || fragmentEntry.empty()) throw std::invalid_argument("Authored material requires explicit vertex and fragment entry points");
        id<MTLLibrary> vertex = Resources->LoadLibrary(vertexLibrary);
        id<MTLLibrary> fragment = Resources->LoadLibrary(fragmentLibrary);
        MTLRenderPipelineDescriptor* descriptor = [MTLRenderPipelineDescriptor new];
        descriptor.vertexFunction = [vertex newFunctionWithName:[NSString stringWithUTF8String:std::string(vertexEntry).c_str()]];
        descriptor.fragmentFunction = [fragment newFunctionWithName:[NSString stringWithUTF8String:std::string(fragmentEntry).c_str()]];
        if (!descriptor.vertexFunction || !descriptor.fragmentFunction) throw std::runtime_error("Authored material library did not contain its declared shader entry point");
        descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        descriptor.colorAttachments[0].blendingEnabled = YES;
        descriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        descriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        descriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
        descriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        descriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
        MTLVertexDescriptor* layout = [MTLVertexDescriptor vertexDescriptor];
        layout.attributes[0].format = MTLVertexFormatFloat3;
        layout.attributes[0].offset = offsetof(MeshVertex,Position);
        layout.attributes[0].bufferIndex = 30;
        layout.attributes[1].format = MTLVertexFormatFloat3;
        layout.attributes[1].offset = offsetof(MeshVertex,Normal);
        layout.attributes[1].bufferIndex = 30;
        layout.attributes[2].format = MTLVertexFormatFloat2;
        layout.attributes[2].offset = offsetof(MeshVertex,Uv);
        layout.attributes[2].bufferIndex = 30;
        layout.layouts[30].stride = sizeof(MeshVertex);
        layout.layouts[30].stepFunction = MTLVertexStepFunctionPerVertex;
        descriptor.vertexDescriptor = layout;
        NSError* error = nil;
        MTLRenderPipelineReflection* reflection = nil;
        MaterialResource material;
        material.Pipeline = [Resources->Device newRenderPipelineStateWithDescriptor:descriptor options:MTLPipelineOptionBufferTypeInfo reflection:&reflection error:&error];
        if (!material.Pipeline) throw std::runtime_error("Authored Metal pipeline: "+std::string(error ? [[error localizedDescription] UTF8String] : "SDK returned no diagnostic"));
        if (!reflection) throw std::runtime_error("Authored Metal pipeline did not provide native resource reflection");
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        Resources->ReflectArguments(material,reflection.vertexArguments,1);
        Resources->ReflectArguments(material,reflection.fragmentArguments,2);
#pragma clang diagnostic pop
        const auto identity = Resources->NextMaterial++;
        Resources->Materials.emplace(identity,std::move(material));
        return identity;
    }

    void MetalRenderer::SubmitMaterialMesh(std::uint64_t mesh,std::uint64_t material,std::span<const MaterialBuffer> buffers,std::span<const MaterialTexture> textures) {
        if (!Resources->Meshes.contains(mesh)) throw std::invalid_argument("Authored Metal draw requires an uploaded indexed mesh");
        auto resource = Resources->Materials.find(material);
        if (resource == Resources->Materials.end()) throw std::invalid_argument("Authored Metal draw requires an uploaded material");
        for (std::size_t index=0; index<buffers.size(); ++index) {
            const auto& binding = buffers[index];
            if (binding.Bytes.empty() || binding.Slot>=30 || binding.Stages == 0 || binding.Stages>3) throw std::invalid_argument("Authored Metal constant binding has invalid bytes, stage or slot");
            for (std::size_t previous=0; previous<index; ++previous) if (buffers[previous].Slot == binding.Slot && (buffers[previous].Stages&binding.Stages)) throw std::invalid_argument("Authored Metal constant bindings overlap within a stage");
        }
        for (std::size_t index=0; index<textures.size(); ++index) {
            const auto& binding = textures[index];
            if (binding.Slot>=128 || binding.Stages == 0 || binding.Stages>3 || !Resources->Textures.contains(binding.Texture)) throw std::invalid_argument("Authored Metal texture binding has an invalid resource, stage or slot");
            for (std::size_t previous=0; previous<index; ++previous) if (textures[previous].Slot == binding.Slot && (textures[previous].Stages&binding.Stages)) throw std::invalid_argument("Authored Metal texture bindings overlap within a stage");
        }
        for (const auto& required : resource->second.Required) {
            if (required.Kind == 0 && std::none_of(buffers.begin(),buffers.end(),[&required](const MaterialBuffer& buffer) { return buffer.Slot == required.Slot && (buffer.Stages&required.Stage) && buffer.Bytes.size()>=required.Bytes; })) throw std::invalid_argument("Authored Metal material is missing a required reflected constant buffer or its payload is too short");
            if (required.Kind == 1 && std::none_of(textures.begin(),textures.end(),[&required](const MaterialTexture& texture) { return texture.Slot == required.Slot && (texture.Stages&required.Stage); })) throw std::invalid_argument("Authored Metal material is missing a required reflected texture");
        }
        DrawPacket packet;
        packet.Texture = 0;
        packet.Mesh = mesh;
        packet.Material = material;
        packet.Clip = Resources->Clip;
        packet.MaterialBuffers.assign(buffers.begin(),buffers.end());
        packet.MaterialTextures.assign(textures.begin(),textures.end());
        Resources->Draws.push_back(std::move(packet));
    }

    void MetalRenderer::ReleaseMaterial(std::uint64_t material) {
        if (std::any_of(Resources->Draws.begin(),Resources->Draws.end(),[material](const DrawPacket& draw) { return draw.Material == material; })) throw std::logic_error("Authored Metal material release: pipeline is still queued for drawing");
        if (Resources->Materials.erase(material)!=1) throw std::invalid_argument("Authored Metal material release: unknown pipeline identity");
    }

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
    bool MetalRenderer::HasPendingDraws() const { return !Resources->Draws.empty(); }

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
