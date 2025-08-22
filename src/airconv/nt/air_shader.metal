#include <metal_stdlib>

using namespace metal;

#define ENCODE_SAMPLE_POS(x, y) ((y & 0xf) << 4) | (x & 0xf)

/*
 * According to D3D11.3 Spec 19.2.4 Specification of Sample Positions
 */

constant uchar sample_pos_2[] = {
    ENCODE_SAMPLE_POS(4, 4),
    ENCODE_SAMPLE_POS(-4, -4),
};
constant uchar sample_pos_4[] = {
    ENCODE_SAMPLE_POS(-2, -6),
    ENCODE_SAMPLE_POS(6, -2),
    ENCODE_SAMPLE_POS(-6, 2),
    ENCODE_SAMPLE_POS(2, 6),
};
constant uchar sample_pos_8[] = {
    ENCODE_SAMPLE_POS(1, -3),
    ENCODE_SAMPLE_POS(-1, 3),
    ENCODE_SAMPLE_POS(5, 1),
    ENCODE_SAMPLE_POS(-3, -5),
    ENCODE_SAMPLE_POS(-5, 5),
    ENCODE_SAMPLE_POS(-7, -1),
    ENCODE_SAMPLE_POS(3, 7),
    ENCODE_SAMPLE_POS(7, -7),
};

float2 sample_pos(uint sample_count, uint index) asm("dxmt.sample_pos.i32");

float2 sample_pos(uint sample_count, uint index) {
    char pos_packed = 0;
    if (sample_count == 2)
        pos_packed = sample_pos_2[index & 0b1];
    else if (sample_count == 4)
        pos_packed = sample_pos_4[index & 0b11]; 
    else if (sample_count == 8)
        pos_packed = sample_pos_8[index & 0b111];

    int x = (char)((char)(pos_packed & 0xf) << 4 ) >> 4;
    int y = ((char)(pos_packed & 0xf0)) >> 4;
    return float2(x / 16.0, y / 16.0);
}

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