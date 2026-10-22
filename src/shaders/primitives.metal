#include <metal_stdlib>
using namespace metal;

/// Fixed interleaved vertex format shared with apple/vertex.hpp.
struct PrimitiveVertex { float2 position; float2 uv; float4 color; };
/// Carries interpolated attributes from the primitive vertex stage.
struct RasterVertex { float4 position [[position]]; float2 uv; float4 color; };

/// Projects fixed GPU payloads which have already passed the engine's configurable-precision boundary.
vertex RasterVertex primitive_vertex(uint id [[vertex_id]], constant PrimitiveVertex* input [[buffer(0)]]) {
    return {float4(input[id].position, 0.0, 1.0), input[id].uv, input[id].color};
}

/// Samples the explicitly supplied texture and applies straight-alpha tint.
fragment float4 primitive_fragment(RasterVertex input [[stage_in]], texture2d<float> image [[texture(0)]]) {
    constexpr sampler nearest_sampler(coord::normalized, address::clamp_to_edge, filter::nearest);
    return image.sample(nearest_sampler, input.uv) * input.color;
}
