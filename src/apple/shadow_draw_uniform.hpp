#pragma once

namespace helengine::apple {
/// Fixed GPU shadow payload; configured engine matrices are converted only at this upload boundary.
struct ShadowDrawUniform final {
    /// Row-vector world-to-light-clip matrix bytes, consumed through Metal's storage transpose.
    float WorldViewProjection[16];
    /// Row-vector local-to-world matrix bytes used to calculate actual point-light distance.
    float World[16];
    /// XYZ point-light world position and positive range; zero range selects projected-depth output.
    float PointLight[4];
};
static_assert(sizeof(ShadowDrawUniform)==144);
}
