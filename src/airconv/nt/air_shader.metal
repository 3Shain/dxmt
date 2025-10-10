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

/* Tessellation */

enum class partitioning {
  integer,
  pow2,
  fractional_odd,
  fractional_even,
};

template <partitioning partition>
char
get_int_factor(half factor) {
  return ceil(factor);
}

template <>
char
get_int_factor<partitioning::fractional_odd>(half factor) {
  char c = ceil(factor);
  return c & 1 ? c : c + 1;
}

template <>
char
get_int_factor<partitioning::fractional_even>(half factor) {
  short c = ceil(factor);
  return c & 1 ? c + 1 : c;
}

template <partitioning partition>
half
place_in_1d(short index, half factor) {
  return (half)index / factor;
}

template <>
half
place_in_1d<partitioning::fractional_odd>(short index, half factor) {
  if (factor < 1) { // it really can only be 0 or 1
    return (half)index;
  }
  half maxh = (1.0 - 1.0 / factor) / 2;
  return min((half)index / factor, maxh);
}

uint
f(uint a) {
  uint x = a - 1u;
  if (x == 0u)
    return 1u;
  return (x << 1) - (1u << (32 - clz(x))) + 1u;
}

template <>
half
place_in_1d<partitioning::fractional_even>(short index, half factor) {
  if (factor < 2) { // it really can only be 0 or 1
    return 0.5h * (half)index;
  }
  return min((half)index / factor, 0.5h);

  // if (factor < 2) { // it really can only be 0 or 1
  //   return 0.5h * (half)index;
  // }
  // short halfFloorFactor = floor(factor / 2.0h);
  // short splitPoint = f(halfFloorFactor);
  // half ret = (half)index / factor;
  // short next_f = get_int_factor<partitioning::fractional_even>(factor);
  // if (index > splitPoint) {
  //   ret = ret - (half((half)next_f - factor) / (2 * factor));
  // }
  // return ret;
}

template <partitioning partition>
half
regularize_factor(half factor) {
  // For integer partitioning, TessFactor range is [1 ... 64] (fractions rounded up).
  return clamp(ceil(factor), 1.0h, 64.0h);
}

template <>
half
regularize_factor<partitioning::pow2>(half factor) {
  // For pow2 partitioning, TessFactor range is [1,2,4,8,16,32,64].
  uint clamped = clamp(ceil(factor), 1.0h, 64.0h);
  return clamped == 1 ? 1 : 1 << (32 - clz(clamped - 1));
}

template <>
half
regularize_factor<partitioning::fractional_odd>(half factor) {
  // For fractional odd partitioning, TessFactor range is [1 ... 63].
  return clamp(factor, 1.0h, 63.0h);
}

template <>
half
regularize_factor<partitioning::fractional_even>(half factor) {
  // For fractional even tessellation, TessFactor range is [2 ... 64].
  return clamp(factor, 2.0h, 64.0h);
}

struct TessMeshWorkload {
  half2 inner0;
  half2 inner1;
  half2 outer0;
  half2 outer1;
  half inner_factor;
  half outer_factor;
  char inner_factor_i;
  char outer_factor_i;
  bool has_complement;
  bool padding;
  half2 inner0_c;
  half2 inner1_c;
  half2 outer0_c;
  half2 outer1_c;
  half inner_factor_c;
  half outer_factor_c;
  char inner_factor_c_i;
  char outer_factor_c_i;
  short patch_index;
};

int
get_next_index(threadgroup int *out_count) {
  return __metal_atomic_fetch_add_explicit(out_count, 1, int(memory_order_relaxed), __METAL_MEMORY_SCOPE_THREADGROUP__);
}

template <partitioning partition>
void
emit_tess_mesh_workload(
    object_data TessMeshWorkload *workloads, threadgroup int *out_count, 
    half2 in0, half2 in1, half2 out0, half2 out1, half inner_factor, half outer_factor,
    half2 in0_c, half2 in1_c, half2 out0_c, half2 out1_c, half inner_factor_c, half outer_factor_c, 
    short patch_index, bool has_complement
) {
  workloads[get_next_index(out_count)] = {
      in0,
      in1,
      out0,
      out1,
      inner_factor,
      outer_factor,
      get_int_factor<partition>(inner_factor),
      get_int_factor<partition>(outer_factor),
      has_complement,
      0,
      in0_c,
      in1_c,
      out0_c,
      out1_c,
      inner_factor_c,
      outer_factor_c,
      has_complement ? get_int_factor<partition>(inner_factor_c) : (char)0,
      has_complement ? get_int_factor<partition>(outer_factor_c) : (char)0,
      patch_index
  };
}

template <partitioning partition>
void
gen_workload_triangle_impl(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in, float out0, float out1,
    float out2
) {
  if (isnan(out0) || out0 <= 0) return;
  if (isnan(out1) || out1 <= 0) return;
  if (isnan(out2) || out2 <= 0) return;

  object_data TessMeshWorkload *workloads = (object_data TessMeshWorkload *)out_buffer;
  half in0f = regularize_factor<partition>(in);
  half out0f = regularize_factor<partition>(out0);
  half out1f = regularize_factor<partition>(out1);
  half out2f = regularize_factor<partition>(out2);
  char in0 = get_int_factor<partition>(in0f);

  short rings = in0 / 2;
  half2 _100{1.0h, 0.0h};
  half2 _010{0.0h, 1.0h};
  half2 _001{0.0h, 0.0h};
  half2 _C = half2(1.0 / 3.0);

  short u = rings;

  for (short r = 0; r < u; r++, u--) {

    half outer = 2.0h * place_in_1d<partition>(r, in0f);
    half inner = 2.0h * place_in_1d<partition>(r + 1, in0f);

    half2 _001x = mix(_001, _C, outer);
    half2 _001i = mix(_001, _C, inner);
    half2 _100x = mix(_100, _C, outer);
    half2 _100i = mix(_100, _C, inner);
    half2 _010x = mix(_010, _C, outer);
    half2 _010i = mix(_010, _C, inner);


    if (r + 1 == u) {
      emit_tess_mesh_workload<partition>(
        workloads, out_count,
        _001i, _100i, _001x, _100x, half(in0f - 2.0h * (r + 1)), half(r == 0 ? out1f : (in0f - 2.0h * (r))), 
        _100i, _010i, _100x, _010x, half(in0f - 2.0h * (r + 1)), half(r == 0 ? out1f : (in0f - 2.0h * (r))), 
        patch_index, true
      );
      if (in0 & 1) {
        half outer = 2.0h * place_in_1d<partition>(rings, in0f);
        half2 _001xp = mix(_001, _C, outer);
        half2 _100xp = mix(_100, _C, outer);
        half2 _010xp = mix(_010, _C, outer);

        emit_tess_mesh_workload<partition>(
          workloads, out_count,
          _010i, _001i, _010x, _001x, half(in0f - 2.0h * (r + 1)), half(r == 0 ? out1f : (in0f - 2.0h * (r))), 
          _010xp.xy, _010xp.xy, _001xp.xy, _100xp.xy, 0.0h, 1.0h,
          patch_index, true
        );
      } else {
        emit_tess_mesh_workload<partition>(
          workloads, out_count,
          _010i, _001i, _010x, _001x, half(in0f - 2.0h * (r + 1)), half(r == 0 ? out1f : (in0f - 2.0h * (r))), 
          _C, _C, _C, _C, 0, 0,
          patch_index, false
        );
      }
      return;
    }

    half outer_c = 2.0h * place_in_1d<partition>(u - 1, in0f);
    half inner_c = 2.0h * place_in_1d<partition>(u, in0f);

    half2 _001x_c = mix(_001, _C, outer_c);
    half2 _001i_c = mix(_001, _C, inner_c);
    half2 _100x_c = mix(_100, _C, outer_c);
    half2 _100i_c = mix(_100, _C, inner_c);
    half2 _010x_c = mix(_010, _C, outer_c);
    half2 _010i_c = mix(_010, _C, inner_c);


    emit_tess_mesh_workload<partition>(
        workloads, out_count, _001i, _100i, _001x, _100x, half(in0f - 2.0h * (r + 1)),
        half(r == 0 ? out1f : (in0f - 2.0h * (r))), 
        _001i_c, _100i_c, _001x_c, _100x_c, half(in0f - 2.0h * (u)), half(in0f - 2.0h * (u - 1)), 
        patch_index, true
    );
    emit_tess_mesh_workload<partition>(
        workloads, out_count, _100i, _010i, _100x, _010x, half(in0f - 2.0h * (r + 1)),
        half(r == 0 ? out2f : (in0f - 2.0h * (r))),
        _100i_c, _010i_c, _100x_c, _010x_c, half(in0f - 2.0h * (u)), half(in0f - 2.0h * (u - 1)), 
        patch_index, true
    );
    emit_tess_mesh_workload<partition>(
        workloads, out_count, _010i, _001i, _010x, _001x, half(in0f - 2.0h * (r + 1)),
        half(r == 0 ? out0f : (in0f - 2.0h * (r))),
        _010i_c, _001i_c, _010x_c, _001x_c, half(in0f - 2.0h * (u)), half(in0f - 2.0h * (u - 1)), 
        patch_index, true
    );
  }

  if (in0 & 1) {
    half outer = 2.0h * place_in_1d<partition>(rings, in0f);
    half2 _001x = mix(_001, _C, outer);
    half2 _100x = mix(_100, _C, outer);
    half2 _010x = mix(_010, _C, outer);
    emit_tess_mesh_workload<partition>(workloads, out_count,
      _010x.xy, _010x.xy, _001x.xy, _100x.xy, 0.0h, 1.0h,
      _C, _C, _C, _C, 0, 0, patch_index, false
    );
  }

  return;
}

void gen_workload_triangle_integer(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in, float out0, float out1,
    float out2
) asm("dxmt.generate_workload.triangle.integer");
void
gen_workload_triangle_integer(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in, float out0, float out1,
    float out2
) {
  gen_workload_triangle_impl<partitioning::integer>(
      patch_index, out_count, out_buffer, in, out0, out1, out2
  );
}

void gen_workload_triangle_pow2(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in, float out0, float out1,
    float out2
) asm("dxmt.generate_workload.triangle.pow2");
void
gen_workload_triangle_pow2(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in, float out0, float out1,
    float out2
) {
  gen_workload_triangle_impl<partitioning::pow2>(patch_index, out_count, out_buffer, in, out0, out1, out2);
}

void gen_workload_triangle_odd(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in, float out0, float out1,
    float out2
) asm("dxmt.generate_workload.triangle.odd");
void
gen_workload_triangle_odd(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in, float out0, float out1,
    float out2
) {
  gen_workload_triangle_impl<partitioning::fractional_odd>(
      patch_index, out_count, out_buffer, in, out0, out1, out2
  );
}

void gen_workload_triangle_even(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in, float out0, float out1,
    float out2
) asm("dxmt.generate_workload.triangle.even");
void
gen_workload_triangle_even(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in, float out0, float out1,
    float out2
) {
  gen_workload_triangle_impl<partitioning::fractional_even>(
      patch_index, out_count, out_buffer, in, out0, out1, out2
  );
}

template <partitioning partition>
void
gen_workload_quad_impl(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in0, float in1, float out0,
    float out1, float out2, float out3
) {
  if (isnan(out0) || out0 <= 0) return;
  if (isnan(out1) || out1 <= 0) return;
  if (isnan(out2) || out2 <= 0) return;
  if (isnan(out3) || out3 <= 0) return;

  object_data TessMeshWorkload *workloads = (object_data TessMeshWorkload *)out_buffer;
  half in0f = regularize_factor<partition>(in0);
  half in1f = regularize_factor<partition>(in1);
  half out0f = regularize_factor<partition>(out0);
  half out1f = regularize_factor<partition>(out1);
  half out2f = regularize_factor<partition>(out2);
  half out3f = regularize_factor<partition>(out3);

  // workaround: outside edges exist if any of outside factors is greater than 1
  if (partition != partitioning::fractional_even) {
    if (out0f > 1.0h || out1f > 1.0h || out2f > 1.0h || out3f > 1.0h) {
      if (in0f == 1.0h)
        in0f = regularize_factor<partition>(1.001h); // 1.00097656, FIXME: eventually use fixed point number
      if (in1f == 1.0h)
        in1f = regularize_factor<partition>(1.001h);
    }
  }

  char in0i = get_int_factor<partition>(in0f);
  char in1i = get_int_factor<partition>(in1f);

  short min_in = min(in0i, in1i);
  short rings = min_in / 2;
  half2 _01{0.0f, 1.0f};
  half2 _11{1.0f, 1.0f};
  half2 _00{0.0f, 0.0f};
  half2 _10{1.0f, 0.0f};

  short u = max(in0i, in1i) / 2;

  for (short r = 0; r < u && r < rings; r++, u--) {

    half x_coord = place_in_1d<partition>(r, in0f);
    half y_coord = place_in_1d<partition>(r, in1f);
    half x_coordi = place_in_1d<partition>(r + 1, in0f);
    half y_coordi = place_in_1d<partition>(r + 1, in1f);

    half2 _01x = _01 + half2{x_coord, -y_coord};
    half2 _11x = _11 + half2{-x_coord, -y_coord};
    half2 _00x = _00 + half2{x_coord, y_coord};
    half2 _10x = _10 + half2{-x_coord, y_coord};
    half2 _01i = _01 + half2{x_coordi, -y_coordi};
    half2 _11i = _11 + half2{-x_coordi, -y_coordi};
    half2 _00i = _00 + half2{x_coordi, y_coordi};
    half2 _10i = _10 + half2{-x_coordi, y_coordi};

    if (r + 1 == u) {
      emit_tess_mesh_workload<partition>(
          workloads, out_count,
          _11i, _01i, _11x, _01x, half(in0f - 2.0h * (r + 1)), half(r == 0 ? out3f : (in0f - 2.0h * (r))),
          _10i, _11i, _10x, _11x, half(in1f - 2.0h * (r + 1)), half(r == 0 ? out2f : (in1f - 2.0h * (r))),
          patch_index, true
      );
      emit_tess_mesh_workload<partition>(
          workloads, out_count,
          _00i, _10i, _00x, _10x, half(in0f - 2.0h * (r + 1)), half(r == 0 ? out1f : (in0f - 2.0h * (r))),
          _01i, _00i, _01x, _00x, half(in1f - 2.0h * (r + 1)), half(r == 0 ? out0f : (in1f - 2.0h * (r))),
         patch_index, true
      );
      break;
    }

    half x_coord_c = place_in_1d<partition>(u - 1, in0f);
    half y_coord_c = place_in_1d<partition>(u - 1, in1f);
    half x_coordi_c = place_in_1d<partition>(u, in0f);
    half y_coordi_c = place_in_1d<partition>(u, in1f);

    half2 _01x_c = _01 + half2{x_coord_c, -y_coord_c};
    half2 _11x_c = _11 + half2{-x_coord_c, -y_coord_c};
    half2 _00x_c = _00 + half2{x_coord_c, y_coord_c};
    half2 _10x_c = _10 + half2{-x_coord_c, y_coord_c};
    half2 _01i_c = _01 + half2{x_coordi_c, -y_coordi_c};
    half2 _11i_c = _11 + half2{-x_coordi_c, -y_coordi_c};
    half2 _00i_c = _00 + half2{x_coordi_c, y_coordi_c};
    half2 _10i_c = _10 + half2{-x_coordi_c, y_coordi_c};
  
    emit_tess_mesh_workload<partition>(
        workloads, out_count,
        _11i, _01i, _11x, _01x, half(in0f - 2.0h * (r + 1)), half(r == 0 ? out3f : (in0f - 2.0h * (r))),
        _11i_c, _01i_c, _11x_c, _01x_c, half(in0f - 2.0h * u), half(in0f - 2.0h * (u - 1)),
        patch_index, u - 1 < rings
    );
    emit_tess_mesh_workload<partition>(
        workloads, out_count,
        _10i, _11i, _10x, _11x, half(in1f - 2.0h * (r + 1)), half(r == 0 ? out2f : (in1f - 2.0h * (r))),
        _10i_c, _11i_c, _10x_c, _11x_c, half(in1f - 2.0h * u), half(in1f - 2.0h * (u - 1)),
        patch_index, u - 1 < rings
    );

    emit_tess_mesh_workload<partition>(
        workloads, out_count,
        _00i, _10i, _00x, _10x, half(in0f - 2.0h * (r + 1)), half(r == 0 ? out1f : (in0f - 2.0h * (r))),
        _00i_c, _10i_c, _00x_c, _10x_c, half(in0f - 2.0h * u), half(in0f - 2.0h * (u - 1)),
        patch_index, u - 1 < rings
    );
    emit_tess_mesh_workload<partition>(
        workloads, out_count,
        _01i, _00i, _01x, _00x, half(in1f - 2.0h * (r + 1)), half(r == 0 ? out0f : (in1f - 2.0h * (r))),
        _01i_c, _00i_c, _01x_c,_00x_c,  half(in1f - 2.0h * u), half(in1f - 2.0h * (u - 1)),
        patch_index, u - 1 < rings
    );
  }

  if (min_in & 1) {
    half x_coord = place_in_1d<partition>(rings, in0f);
    half y_coord = place_in_1d<partition>(rings, in1f);

    half2 _01x = _01 + half2{x_coord, -y_coord};
    half2 _11x = _11 + half2{-x_coord, -y_coord};
    half2 _00x = _00 + half2{x_coord, y_coord};
    half2 _10x = _10 + half2{-x_coord, y_coord};

    if (in0 > in1) {
      emit_tess_mesh_workload<partition>(
          workloads, out_count, _01x,_11x, _00x,  _10x, half(in0f - 2.0h * rings), half(in0f - 2.0h * rings),
          0, 0, 0, 0, 0, 0,
          patch_index, false
      );
    } else {
      emit_tess_mesh_workload<partition>(
          workloads, out_count, _11x, _10x, _01x, _00x, half(in1f - 2.0h * rings), half(in1f - 2.0h * rings),
          0, 0, 0, 0, 0, 0,
          patch_index, false
      );
    }
  }

  return;
}

void gen_workload_quad_integer(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in0, float in1, float out0,
    float out1, float out2, float out3
) asm("dxmt.generate_workload.quad.integer");
void
gen_workload_quad_integer(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in0, float in1, float out0,
    float out1, float out2, float out3
) {
  gen_workload_quad_impl<partitioning::integer>(
      patch_index, out_count, out_buffer, in0, in1, out0, out1, out2, out3
  );
}

void gen_workload_quad_pow2(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in0, float in1, float out0,
    float out1, float out2, float out3
) asm("dxmt.generate_workload.quad.pow2");
void
gen_workload_quad_pow2(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in0, float in1, float out0,
    float out1, float out2, float out3
) {
  gen_workload_quad_impl<partitioning::pow2>(
      patch_index, out_count, out_buffer, in0, in1, out0, out1, out2, out3
  );
}

void gen_workload_quad_odd(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in0, float in1, float out0,
    float out1, float out2, float out3
) asm("dxmt.generate_workload.quad.odd");
void
gen_workload_quad_odd(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in0, float in1, float out0,
    float out1, float out2, float out3
) {
  gen_workload_quad_impl<partitioning::fractional_odd>(
      patch_index, out_count, out_buffer, in0, in1, out0, out1, out2, out3
  );
}

void gen_workload_quad_even(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in0, float in1, float out0,
    float out1, float out2, float out3
) asm("dxmt.generate_workload.quad.even");
void
gen_workload_quad_even(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in0, float in1, float out0,
    float out1, float out2, float out3
) {
  gen_workload_quad_impl<partitioning::fractional_even>(
      patch_index, out_count, out_buffer, in0, in1, out0, out1, out2, out3
  );
}

struct domain_location {
  float2 uv;
  bool active;
  bool iterate;
};

template <partitioning partition>
half2
get_domain_point(object_data TessMeshWorkload &workload, ushort tid) {
  bool point_at_out = tid > workload.inner_factor_i;
  bool point_at_in_c = tid > workload.inner_factor_i + workload.outer_factor_i + 1;
  bool point_at_out_c = tid > workload.inner_factor_i + workload.outer_factor_i + workload.inner_factor_c_i + 2;

  char real_factor = workload.inner_factor_i;
  real_factor = select(real_factor, workload.outer_factor_i, point_at_out);
  real_factor = select(real_factor, workload.inner_factor_c_i, point_at_in_c);
  real_factor = select(real_factor, workload.outer_factor_c_i, point_at_out_c);

  tid = select(tid, (ushort)(tid - (workload.inner_factor_i + 1)), point_at_out);
  tid = select(tid, (ushort)(tid - (workload.outer_factor_i + 1)), point_at_in_c);
  tid = select(tid, (ushort)(tid - (workload.inner_factor_c_i + 1)), point_at_out_c);

  half2 left = workload.inner0;
  left = select(left, workload.outer0, point_at_out);
  left = select(left, workload.inner0_c, point_at_in_c);
  left = select(left, workload.outer0_c, point_at_out_c);

  half2 right = workload.inner1;
  right = select(right, workload.outer1, point_at_out);
  right = select(right, workload.inner1_c, point_at_in_c);
  right = select(right, workload.outer1_c, point_at_out_c);

  half factor = workload.inner_factor;
  factor = select(factor, workload.outer_factor, point_at_out);
  factor = select(factor, workload.inner_factor_c, point_at_in_c);
  factor = select(factor, workload.outer_factor_c, point_at_out_c);

  if (real_factor == 0)
    return left; // very edge case...
  ushort tid_ref = real_factor - tid;
  half ratio = place_in_1d<partition>(min(tid, tid_ref), factor);
  ratio = select(ratio, 1.0h - ratio, tid_ref < tid);
  return mix(left, right, ratio);
}

int get_domain_patch_index(int workload_index, object_data int *data) asm("dxmt.get_domain_patch_index");

int get_domain_patch_index(int workload_index, object_data int *data) {
  object_data TessMeshWorkload &workload = ((object_data TessMeshWorkload *)data)[workload_index];
  return workload.patch_index;  
}

template <partitioning partition>
domain_location
get_domain_location_impl(int workload_index, int thread_index, object_data int *data) {
  simdgroup_barrier(mem_flags::mem_none);

  domain_location ret{float2(0), false, false};
  object_data TessMeshWorkload &workload = ((object_data TessMeshWorkload *)data)[workload_index];

  int count = workload.inner_factor_i + workload.outer_factor_i + 2;
  if (workload.has_complement)
    count += workload.inner_factor_c_i + workload.outer_factor_c_i + 2;

  if (thread_index < count) {
    ret.active = true;
    ret.uv = (float2)get_domain_point<partition>(workload, thread_index);
  }

  simdgroup_barrier(mem_flags::mem_none);
  ret.iterate = simd_all(ret.active);

  return ret;
};

domain_location get_domain_location_integer(int workload_index, int thread_index, object_data int *data) asm(
    "dxmt.get_domain_location.integer"
);

domain_location
get_domain_location_integer(int workload_index, int thread_index, object_data int *data) {
  return get_domain_location_impl<partitioning::integer>(workload_index, thread_index, data);
};

domain_location get_domain_location_pow2(int workload_index, int thread_index, object_data int *data) asm(
    "dxmt.get_domain_location.pow2"
);

domain_location
get_domain_location_pow2(int workload_index, int thread_index, object_data int *data) {
  return get_domain_location_impl<partitioning::pow2>(workload_index, thread_index, data);
};

domain_location
get_domain_location_odd(int workload_index, int thread_index, object_data int *data) asm("dxmt.get_domain_location.odd"
);

domain_location
get_domain_location_odd(int workload_index, int thread_index, object_data int *data) {
  return get_domain_location_impl<partitioning::fractional_odd>(workload_index, thread_index, data);
};

domain_location get_domain_location_even(int workload_index, int thread_index, object_data int *data) asm(
    "dxmt.get_domain_location.even"
);

domain_location
get_domain_location_even(int workload_index, int thread_index, object_data int *data) {
  return get_domain_location_impl<partitioning::fractional_even>(workload_index, thread_index, data);
};

struct dummy_vertex {
  float4 pos [[position]];
};

using MeshTri = metal::mesh<dummy_vertex, void, 130, 128, topology::triangle>;

void generatePrimitiveTriangle(int workload_index, object_data int *data, MeshTri mesh) asm(
    "dxmt.domain_generate_primitives.triangle"
);

void
generate_triangle_for_edges(MeshTri mesh, short base_inner, short base_outer, short inner, short outer, short provoke_offset, short ccw_xor) {
  for (short i = 0; i <= inner; i++) {
    half frac_left = half(i) / half(inner + 1);
    half frac_right = half(i + 1) / half(inner + 1);
    short left = frac_left <= 0.5 ? floor(frac_left * half(outer)) : ceil(frac_left * half(outer));
    short right = frac_right <= 0.5 ? floor(frac_right * half(outer)) : ceil(frac_right * half(outer));
    short count = right - left;
    if (count) {
      for (short j = 0; j < count; j++) {
        short primitive_index = base_outer + left + j - provoke_offset - 1;
        mesh.set_index(primitive_index * 3, base_outer + left + j);
        mesh.set_index(primitive_index * 3 + (1 ^ ccw_xor), base_outer + left + j + 1);
        mesh.set_index(primitive_index * 3 + (2 ^ ccw_xor), base_inner + i);
      }
    }
    if (i == inner) break;
    short primitive_index = base_inner + i - provoke_offset;
    mesh.set_index(primitive_index * 3, base_inner + i);
    mesh.set_index(primitive_index * 3 + (1 ^ ccw_xor), base_outer + right);
    mesh.set_index(primitive_index * 3 + (2 ^ ccw_xor), base_inner + i + 1);
  }
}

void
generatePrimitiveTriangle(int workload_index, object_data int *data, MeshTri mesh) {
  object_data TessMeshWorkload &workload = ((object_data TessMeshWorkload *)data)[workload_index];

  short primitive_count = 0;
  {
    short max_in = workload.inner_factor_i;
    short max_out = workload.outer_factor_i;
    short base_outer = max_in + 1;

    primitive_count += max_in + max_out;
    generate_triangle_for_edges(mesh, 0, base_outer, max_in, max_out, 0, 0);
  }

  if (workload.has_complement) {
    short max_in = workload.inner_factor_c_i;
    short max_out = workload.outer_factor_c_i;
    short base_inner = 2 + workload.inner_factor_i + workload.outer_factor_i;
    short base_outer = base_inner + max_in + 1;

    primitive_count += max_in + max_out;
    generate_triangle_for_edges(mesh, base_inner, base_outer, max_in, max_out, 2, 0);
  }

  mesh.set_primitive_count(primitive_count);
}

void generatePrimitiveTriangleCCW(int workload_index, object_data int *data, MeshTri mesh) asm(
    "dxmt.domain_generate_primitives.triangle_ccw"
);

void
generatePrimitiveTriangleCCW(int workload_index, object_data int *data, MeshTri mesh) {
  object_data TessMeshWorkload &workload = ((object_data TessMeshWorkload *)data)[workload_index];

  short primitive_count = 0;
  {
    short max_in = workload.inner_factor_i;
    short max_out = workload.outer_factor_i;
    short base_outer = max_in + 1;

    primitive_count += max_in + max_out;
    generate_triangle_for_edges(mesh, 0, base_outer, max_in, max_out, 0, 3);
  }

  if (workload.has_complement) {
    short max_in = workload.inner_factor_c_i;
    short max_out = workload.outer_factor_c_i;
    short base_inner = 2 + workload.inner_factor_i + workload.outer_factor_i;
    short base_outer = base_inner + max_in + 1;

    primitive_count += max_in + max_out;
    generate_triangle_for_edges(mesh, base_inner, base_outer, max_in, max_out, 2, 3);
  }

  mesh.set_primitive_count(primitive_count);
}

void generatePrimitivePoint(int workload_index, object_data int *data, MeshTri mesh) asm(
    "dxmt.domain_generate_primitives.point"
);

void
generatePrimitivePoint(int workload_index, object_data int *data, MeshTri mesh) {
  object_data TessMeshWorkload &workload = ((object_data TessMeshWorkload *)data)[workload_index];

  short index_count = 0;

  {
    short max_in = workload.inner_factor_i;
    short max_out = workload.outer_factor_i;
    short base_outer = max_in + 1;

    for (short i = 0; i < max_in + 1; i++) {
      mesh.set_index(index_count++, i);
    }
    for (short i = 0; i < max_out + 1; i++) {
      mesh.set_index(index_count++, i + base_outer);
    }
  }

  if (workload.has_complement) {
    short max_in = workload.inner_factor_c_i;
    short max_out = workload.outer_factor_c_i;
    short base_inner = 2 + workload.inner_factor_i + workload.outer_factor_i;
    short base_outer = base_inner + max_in + 1;

    for (short i = 0; i < max_in + 1; i++) {
      mesh.set_index(index_count++, i + base_inner);
    }
    for (short i = 0; i < max_out + 1; i++) {
      mesh.set_index(index_count++, i + base_outer);
    }
  }

  mesh.set_primitive_count(index_count);
}