#pragma once
#include "apple/metal_renderer.hpp"
#include "RenderManager2D.hpp"
#include "RenderCommandListBuilder2D.hpp"
#include "RenderList2D.hpp"
#include "SpriteQuad2D.hpp"
#include "byte4.hpp"
#include "float4.hpp"
#include <unordered_map>
#include <unordered_set>

class ICamera;
class RenderCommandList2D;

namespace helengine::apple {
    /// Adapts the real generated engine 2D command stream to the shared Metal renderer without changing generated code.
    class CoreRenderManager2D final : public ::RenderManager2D {
        /// Native renderer outlives the generated-core bridge.
        MetalRenderer& Renderer;
        /// Generated builder resolves clipping, sprite transforms and font glyph geometry.
        ::RenderCommandListBuilder2D Commands;
        /// Small generated render queue used when the core invokes a direct drawable method.
        ::RenderList2D ImmediateQueue;
        /// Associates each engine-owned runtime texture with its independently retained native texture.
        std::unordered_map<::RuntimeTexture*,std::uint64_t> Textures;
        /// Defers destruction until the current synchronous GPU frame has completed.
        std::unordered_set<::RuntimeTexture*> Released;
        /// Current target width in physical pixels, separate from SDL logical coordinates.
        std::uint32_t Width = 0;
        /// Current target height in physical pixels.
        std::uint32_t Height = 0;
        /// Current camera viewport intersected with the target.
        ScissorRect Viewport{};
        /// Detects repeated disposal and forbids uploads into a disposed bridge.
        bool Disposed = false;
        /// Converts a resolved sprite's four corners and normalized source rectangle to fixed GPU triangles.
        std::array<Vertex,6> BuildQuad(::SpriteQuad2D geometry, ::float4 source, ::byte4 tint) const;
        /// Converts window-pixel bounds into an unrotated resolved sprite geometry.
        ::SpriteQuad2D BuildBounds(::float4 bounds) const;
        /// Clamps an absolute engine clip to the current physical viewport and parent clip.
        ScissorRect Intersect(::float4 rectangle, ScissorRect parent) const;
        /// Resolves only textures built by this bridge; unknown and disposed resources are explicit errors.
        std::uint64_t ResolveTexture(::RuntimeTexture* texture) const;
        /// Builds one immediate drawable through the same generated command list path as camera queues.
        void DrawImmediate(::IDrawable2D* drawable);
        /// Converts all ordered generated commands and enforces balanced clip scopes.
        void DrawCommands(::RenderCommandList2D* commands);
    public:
        /// Binds a real Metal device to the generated engine's 2D API.
        explicit CoreRenderManager2D(MetalRenderer& renderer);
        /// Creates an RGBA32 runtime texture while preserving engine resource identity and ownership metadata.
        ::RuntimeTexture* BuildTextureFromRaw(::TextureAsset* data) override;
        /// Selects grayscale font atlases until independent RGB coverage has a validated native shader contract.
        bool get_SupportsRgbFontCoverage() override;
        /// Defers a texture's final native and generated-object destruction to a GPU-safe boundary.
        void ReleaseTexture(::RuntimeTexture* texture) override;
        /// Invalidates generated glyph reuse before releasing a font through the engine's ownership API.
        void ReleaseFont(::FontAsset* font) override;
        /// Destroys deferred resources after presentation/readback consumed the queued Metal packets.
        void FlushReleasedTextures() override;
        /// Releases generated command caches, fallback textures and every remaining owned native texture.
        void Dispose() override;
        /// Translates one real camera queue using the engine viewport convention and preserves command order.
        void RenderCamera(::ICamera* camera, std::uint32_t pixelWidth, std::uint32_t pixelHeight);
        /// Resolves a directly requested sprite through the generated command builder.
        void DrawSprite(::ISpriteDrawable2D* sprite) override;
        /// Resolves a directly requested text drawable through the generated font/glyph builder.
        void DrawText(::ITextDrawable2D* text) override;
        /// Resolves a directly requested rounded shape through the generated command builder.
        void DrawRoundedRect(::IRoundedRectDrawable2D* shape) override;
    protected:
        /// Updates validated runtime atlas regions through the renderer's exact RGBA8 upload contract.
        void UpdateTextureRegionCore(::RuntimeTexture* texture,int32_t x,int32_t y,int32_t width,int32_t height,Array<uint8_t>* rgba,int32_t pitch) override;
    };
}
