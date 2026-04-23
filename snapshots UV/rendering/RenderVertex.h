#pragma once

#include <cstddef>

// Vertex format matching GraphicsPipeline input layout:
//   POSITION: R32G32B32_FLOAT    (offset 0,  12 bytes)
//   COLOR:    R32G32B32A32_FLOAT (offset 12, 16 bytes)
//   NORMAL:   R32G32B32_FLOAT    (offset 28, 12 bytes)
//   Total stride: 40 bytes
struct RenderVertex {
    float position[3];
    float color[4];
    float normal[3];
};

static_assert(sizeof(RenderVertex) == 40, "RenderVertex layout must stay aligned with GraphicsPipeline input layout");
