#include "apple/core_render_manager_2d.hpp"
#include "ICamera.hpp"
#include "RuntimeTexture.hpp"
#include "TextureAsset.hpp"
#include "TextureAssetColorFormat.hpp"
#include "FontAsset.hpp"
#include "RenderCommandList2D.hpp"
#include "RenderCommand2DType.hpp"
#include "ISpriteDrawable2D.hpp"
#include "ITextDrawable2D.hpp"
#include "IRoundedRectDrawable2D.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace helengine::apple {
    CoreRenderManager2D::CoreRenderManager2D(MetalRenderer& renderer) : Renderer(renderer), ImmediateQueue(1) {}

    RuntimeTexture* CoreRenderManager2D::BuildTextureFromRaw(TextureAsset* data) {
        if (Disposed) throw std::logic_error("Apple 2D bridge is disposed");
        if (!data || !data->Colors || !data->Colors->Data || data->Colors->Length <= 0) throw std::invalid_argument("Apple texture requires embedded RGBA32 data");
        if (data->ColorFormat != TextureAssetColorFormat::Rgba32) throw std::invalid_argument("Apple raw texture requires engine Rgba32 format");
        const auto resource = Renderer.UploadTexture(data->Width,data->Height,std::span<const std::uint8_t>(data->Colors->Data,static_cast<std::size_t>(data->Colors->Length)));
        auto texture = std::make_unique<RuntimeTexture>();
        texture->set_Id(data->get_Id());
        texture->set_Width(data->Width);
        texture->set_Height(data->Height);
        texture->set_IsEngineOwned(data->IsEngineOwned);
        texture->set_UsesRgbFontCoverage(false);
        Textures.emplace(texture.get(),resource);
        return texture.release();
    }

    bool CoreRenderManager2D::get_SupportsRgbFontCoverage() { return false; }

    std::uint64_t CoreRenderManager2D::ResolveTexture(RuntimeTexture* texture) const {
        auto resource = Textures.find(texture);
        if (!texture || resource == Textures.end() || Released.contains(texture) || texture->get_IsDisposed()) throw std::invalid_argument("Apple draw references an absent, foreign or disposed runtime texture");
        return resource->second;
    }

    void CoreRenderManager2D::UpdateTextureRegionCore(RuntimeTexture* texture,int32_t x,int32_t y,int32_t width,int32_t height,Array<uint8_t>* rgba,int32_t pitch) {
        if (!rgba || !rgba->Data || rgba->Length < 0 || x < 0 || y < 0 || width <= 0 || height <= 0 || pitch <= 0) throw std::invalid_argument("Apple texture region requires valid source bytes and positive pixel dimensions");
        Renderer.UpdateTexture(ResolveTexture(texture),x,y,width,height,std::span<const std::uint8_t>(rgba->Data,static_cast<std::size_t>(rgba->Length)),pitch);
    }

    void CoreRenderManager2D::ReleaseTexture(RuntimeTexture* texture) {
        if (!texture || !Textures.contains(texture)) throw std::invalid_argument("Apple texture release references a foreign resource");
        if (!Released.insert(texture).second) throw std::logic_error("Apple runtime texture was released twice");
        Commands.InvalidateTextCommandReuse();
        texture->Dispose();
    }

    void CoreRenderManager2D::ReleaseFont(FontAsset* font) {
        Commands.InvalidateTextCommandReuse();
        RenderManager2D::ReleaseFont(font);
    }

    void CoreRenderManager2D::FlushReleasedTextures() {
        if (Renderer.HasPendingDraws()) return;
        for (auto* texture : Released) {
            Renderer.ReleaseTexture(Textures.at(texture));
            Textures.erase(texture);
            delete texture;
        }
        Released.clear();
    }

    void CoreRenderManager2D::Dispose() {
        if (Disposed) return;
        DisposeDefaultTextures();
        Renderer.DiscardFrame();
        for (const auto& resource : Textures) {
            Renderer.ReleaseTexture(resource.second);
            resource.first->Dispose();
            delete resource.first;
        }
        Textures.clear();
        Released.clear();
        Commands.Dispose();
        ImmediateQueue.Dispose();
        Disposed = true;
    }

    SpriteQuad2D CoreRenderManager2D::BuildBounds(float4 bounds) const {
        SpriteQuad2D quad;
        quad.TopLeft = float2(bounds.X,bounds.Y);
        quad.TopRight = float2(bounds.X+bounds.Z,bounds.Y);
        quad.BottomRight = float2(bounds.X+bounds.Z,bounds.Y+bounds.W);
        quad.BottomLeft = float2(bounds.X,bounds.Y+bounds.W);
        return quad;
    }

    std::array<Vertex,6> CoreRenderManager2D::BuildQuad(SpriteQuad2D quad,float4 source,byte4 tint) const {
        if (Width == 0 || Height == 0) throw std::logic_error("Apple 2D draw requires a nonzero physical target extent");
        const std::array<float2,6> positions = {quad.TopLeft,quad.BottomLeft,quad.BottomRight,quad.TopLeft,quad.BottomRight,quad.TopRight};
        const float u = source.X.ToSingle();
        const float v = source.Y.ToSingle();
        const float right = (source.X+source.Z).ToSingle();
        const float bottom = (source.Y+source.W).ToSingle();
        const std::array<std::array<float,2>,6> uv = {{{u,v},{u,bottom},{right,bottom},{u,v},{right,bottom},{right,v}}};
        std::array<Vertex,6> vertices{};
        for (std::size_t index=0; index<vertices.size(); ++index) {
            vertices[index] = {{positions[index].X.ToSingle()/Width*2.0f-1.0f,1.0f-positions[index].Y.ToSingle()/Height*2.0f}, {uv[index][0],uv[index][1]}, {tint.X/255.0f,tint.Y/255.0f,tint.Z/255.0f,tint.W/255.0f}};
        }
        return vertices;
    }

    ScissorRect CoreRenderManager2D::Intersect(float4 rectangle,ScissorRect parent) const {
        const float x = rectangle.X.ToSingle();
        const float y = rectangle.Y.ToSingle();
        const float width = rectangle.Z.ToSingle();
        const float height = rectangle.W.ToSingle();
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) || !std::isfinite(height)) throw std::invalid_argument("Apple clip requires finite pixel coordinates");
        const float left = std::max<float>(parent.X,std::clamp(std::floor(x),0.0f,static_cast<float>(Width)));
        const float top = std::max<float>(parent.Y,std::clamp(std::floor(y),0.0f,static_cast<float>(Height)));
        const float right = std::min<float>(parent.X+parent.Width,std::clamp(std::ceil(x+std::max(0.0f,width)),0.0f,static_cast<float>(Width)));
        const float bottom = std::min<float>(parent.Y+parent.Height,std::clamp(std::ceil(y+std::max(0.0f,height)),0.0f,static_cast<float>(Height)));
        return {static_cast<std::uint32_t>(left),static_cast<std::uint32_t>(top),static_cast<std::uint32_t>(std::max(0.0f,right-left)),static_cast<std::uint32_t>(std::max(0.0f,bottom-top))};
    }

    void CoreRenderManager2D::RenderCamera(ICamera* camera,std::uint32_t pixelWidth,std::uint32_t pixelHeight) {
        if (Disposed) throw std::logic_error("Apple 2D bridge is disposed");
        if (!camera) throw std::invalid_argument("Apple 2D camera is required");
        Width = pixelWidth;
        Height = pixelHeight;
        if (Width == 0 || Height == 0) return;
        float4 viewport = camera->get_Viewport();
        if (viewport.Z > 0 && viewport.W > 0 && viewport.Z <= 1 && viewport.W <= 1) viewport = float4(viewport.X*Width,viewport.Y*Height,viewport.Z*Width,viewport.W*Height);
        if (viewport.Z <= 0 || viewport.W <= 0) viewport = float4(0,0,Width,Height);
        Viewport = Intersect(viewport,ScissorRect{0,0,Width,Height});
        if (!camera->get_RenderQueue2D()) return;
        DrawCommands(Commands.Build(camera->get_RenderQueue2D()));
    }

    void CoreRenderManager2D::DrawCommands(RenderCommandList2D* commands) {
        if (!commands) throw std::runtime_error("Generated 2D command builder returned no command list");
        std::vector<ScissorRect> clips{Viewport};
        Renderer.SetClip(Viewport);
        for (int32_t index=0; index<commands->get_Count(); ++index) {
            const auto kind = commands->GetCommandType(index);
            if (kind == RenderCommand2DType::ClipPush) {
                clips.push_back(Intersect(commands->GetClipPushRect(commands->GetClipPushPayloadIndex(index)),clips.back()));
                Renderer.SetClip(clips.back());
            } else if (kind == RenderCommand2DType::ClipPop) {
                if (clips.size() == 1) throw std::runtime_error("Generated 2D clip stack underflow");
                clips.pop_back();
                Renderer.SetClip(clips.back());
            } else if (kind == RenderCommand2DType::TexturedQuad) {
                const auto payload = commands->GetTexturedQuadPayloadIndex(index);
                Renderer.Submit(BuildQuad(commands->GetTexturedQuadGeometry(payload),commands->GetTexturedQuadSourceRect(payload),commands->GetTexturedQuadColor(payload)),ResolveTexture(commands->GetTexturedQuadTexture(payload)));
            } else if (kind == RenderCommand2DType::GlyphQuad) {
                const auto payload = commands->GetGlyphQuadPayloadIndex(index);
                Renderer.Submit(BuildQuad(BuildBounds(commands->GetGlyphQuadBounds(payload)),commands->GetGlyphQuadSourceRect(payload),commands->GetGlyphQuadColor(payload)),ResolveTexture(commands->GetGlyphQuadTexture(payload)));
            } else if (kind == RenderCommand2DType::RoundedRect) {
                const auto payload = commands->GetRoundedRectPayloadIndex(index);
                const auto bounds = commands->GetRoundedRectBounds(payload);
                if (bounds.Z <= 0 || bounds.W <= 0) continue;
                const auto border = commands->GetRoundedRectBorderColor(payload);
                Renderer.SubmitRoundedRect(BuildQuad(BuildBounds(bounds),float4(0,0,1,1),commands->GetRoundedRectFillColor(payload)),bounds.Z.ToSingle(),bounds.W.ToSingle(),static_cast<float>(commands->GetRoundedRectRadius(payload)),static_cast<float>(commands->GetRoundedRectBorderThickness(payload)),{border.X/255.0f,border.Y/255.0f,border.Z/255.0f,border.W/255.0f},static_cast<std::uint32_t>(commands->GetRoundedRectCorners(payload)));
            } else throw std::runtime_error("Unknown generated 2D command kind");
        }
        if (clips.size() != 1) throw std::runtime_error("Generated 2D command list did not close its clip scopes");
        Renderer.SetClip(std::nullopt);
    }

    void CoreRenderManager2D::DrawImmediate(IDrawable2D* drawable) {
        if (!drawable) throw std::invalid_argument("Apple immediate drawable is required");
        if (Width == 0 || Height == 0) throw std::logic_error("Apple immediate drawing requires a selected camera viewport");
        ImmediateQueue.Clear();
        ImmediateQueue.Add(drawable);
        DrawCommands(Commands.Build(&ImmediateQueue));
        ImmediateQueue.Clear();
    }

    void CoreRenderManager2D::DrawSprite(ISpriteDrawable2D* sprite) { DrawImmediate(sprite); }
    void CoreRenderManager2D::DrawText(ITextDrawable2D* text) { DrawImmediate(text); }
    void CoreRenderManager2D::DrawRoundedRect(IRoundedRectDrawable2D* shape) { DrawImmediate(shape); }
}
