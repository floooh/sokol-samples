#include <metal_stdlib>
using namespace metal;
fragment float4 _main(float4 color [[stage_in]]) {
    return color;
}

