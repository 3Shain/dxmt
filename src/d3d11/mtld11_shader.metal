#include <metal_lib>

[[kernel]] void clearTexture2D(
    texture2d<access:readwrite> tex [[texture(0)]],
    uint2 pos [[threadgroup_positioningrid]]
) {

}