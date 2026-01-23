#include <metal_stdlib>

using namespace metal;

template<typename T>
T msad_impl(T ref, T src, T accum) {

    {
        T ref_byte = (ref >> 0) & T(0xFF);
        T src_byte = (src >> 0) & T(0xFF);
        accum += select(0u, absdiff(ref_byte, src_byte), ref_byte != 0);
    }
    {
        T ref_byte = (ref >> 8) & T(0xFF);
        T src_byte = (src >> 8) & T(0xFF);
        accum += select(0u, absdiff(ref_byte, src_byte), ref_byte != 0);
    }
    {
        T ref_byte = (ref >> 16) & T(0xFF);
        T src_byte = (src >> 16) & T(0xFF);
        accum += select(0u, absdiff(ref_byte, src_byte), ref_byte != 0);
    }
    {
        T ref_byte = (ref >> 24) & T(0xFF);
        T src_byte = (src >> 24) & T(0xFF);
        accum += select(0u, absdiff(ref_byte, src_byte), ref_byte != 0);
    }

    return accum;
}

uint msad(uint ref, uint src, uint accum) asm("dxmt.msad.i32");
uint2 msad(uint2 ref, uint2 src, uint2 accum) asm("dxmt.msad.v2i32");
uint3 msad(uint3 ref, uint3 src, uint3 accum) asm("dxmt.msad.v3i32");
uint4 msad(uint4 ref, uint4 src, uint4 accum) asm("dxmt.msad.v4i32");

uint msad(uint ref, uint src, uint accum) {
    return msad_impl(ref, src, accum);
}
uint2 msad(uint2 ref, uint2 src, uint2 accum) {
    return msad_impl(ref, src, accum);
}
uint3 msad(uint3 ref, uint3 src, uint3 accum) {
    return msad_impl(ref, src, accum);
}
uint4 msad(uint4 ref, uint4 src, uint4 accum) {
    return msad_impl(ref, src, accum);
}
