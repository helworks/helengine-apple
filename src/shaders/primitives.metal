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

/// Fixed-size SDF payload corresponding to apple/rounded_rect_uniform.hpp.
struct RoundedRectUniform { float4 parameters; float4 borderColor; uint4 flags; };

/// Evaluates per-corner rounded rectangle coverage and blends the authored interior and border without a CPU tessellation approximation.
fragment float4 rounded_fragment(RasterVertex input [[stage_in]], constant RoundedRectUniform& shape [[buffer(0)]]) {
    float2 size = shape.parameters.xy;
    uint corner = input.uv.y < 0.5 ? (input.uv.x < 0.5 ? 1u : 2u) : (input.uv.x < 0.5 ? 4u : 8u);
    float radius = (shape.flags.x & corner) ? clamp(shape.parameters.z, 0.0, min(size.x, size.y) * 0.5) : 0.0;
    float2 q = abs(input.uv * size - size * 0.5) - (size * 0.5 - radius);
    float distance = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
    float aa = max(fwidth(distance), 0.5);
    float coverage = 1.0 - smoothstep(-aa * 0.5, aa * 0.5, distance);
    float interior = shape.parameters.w > 0.0 ? 1.0 - smoothstep(-aa * 0.5, aa * 0.5, distance + shape.parameters.w) : 1.0;
    float4 color = mix(shape.borderColor, input.color, interior);
    color.a *= coverage;
    return color;
}
