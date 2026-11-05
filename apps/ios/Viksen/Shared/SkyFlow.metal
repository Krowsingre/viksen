#include <metal_stdlib>
#include <SwiftUI/SwiftUI_Metal.h>
using namespace metal;

// Slow, flowing distortion for the Home sky (owner, 2026-11-05): the swirls waver and
// drift like smoke or water. Small offsets (a few points) layered at two scales.
[[ stitchable ]] float2 skyFlow(float2 position, float time, float amount) {
    float2 p = position;
    p.x += sin(p.y * 0.011 + time * 0.55) * amount + sin(p.y * 0.029 - time * 0.33) * amount * 0.45;
    p.y += cos(p.x * 0.009 + time * 0.41) * amount * 0.6 + sin((p.x + p.y) * 0.017 + time * 0.27) * amount * 0.3;
    return p;
}
