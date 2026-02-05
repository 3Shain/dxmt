#include <metal_stdlib>

using namespace metal;

enum class partitioning {
  integer,
  pow2,
  fractional_odd,
  fractional_even,
};

using fxp32 = uint;

constexpr constant fxp32 fxp32_0 = 0x0u;
constexpr constant fxp32 fxp32_1 = 0x10000u;
constexpr constant fxp32 fxp32_2 = 0x20000u;
constexpr constant fxp32 fxp32_half = 0x8000u;

uint fxp32_ceil(fxp32 f) {
  return (f >> 16) + ((f & 0xffff) > 0 ? 1 : 0);
}

fxp32 fxp32_invb(uint n, fxp32 v) {
  return (fxp32)(((ulong)n << 32) / v);
}

using fxp16 = ushort;
using fxp16v2 = ushort2;

constexpr constant fxp16 fxp16_0 = ushort{0x0000};
constexpr constant fxp16 fxp16_one_third = ushort{0x2aab};
constexpr constant fxp16 fxp16_1 = ushort{0x8000};

constexpr constant float fxp16_1_f = 32768.0;

using fxp16v2 = ushort2; // 0x0000 -> 0.0 ; 0x8000 -> 1.0

fxp16v2 mix(fxp16v2 a, fxp16v2 b, fxp32 factor) {

  uint2 ai = uint2(a);
  uint2 bi = uint2(b);

  uint f = min(factor >> 1, (uint)fxp16_1);
  uint invf = fxp16_1 - f;

  uint2 ri = ((ai * invf + bi * f)) / fxp16_1; // effective right shift 15
  ri = min(ri, uint2(fxp16_1));
  return fxp16v2(ri);
};

float2 to_float(fxp16v2 unorm) {
  return (float2)unorm / float2(fxp16_1_f);
}

template <partitioning partition>
char
get_int_factor(fxp32 factor) {
  return fxp32_ceil(factor);
}

template <>
char
get_int_factor<partitioning::fractional_odd>(fxp32 factor) {
  char c = fxp32_ceil(factor);
  return c & 1 ? c : c + 1;
}

template <>
char
get_int_factor<partitioning::fractional_even>(fxp32 factor) {
  short c = fxp32_ceil(factor);
  return c & 1 ? c + 1 : c;
}

template <partitioning partition>
fxp32
place_in_1d(short index, fxp32 factor) {
  return fxp32_invb(index, factor);
}

template <>
fxp32
place_in_1d<partitioning::fractional_odd>(short index, fxp32 factor) {
  if (factor < fxp32_1) { // it really can only be 0 or 1
    return index ? fxp32_1 : fxp32_0;
  }
  fxp32 maxh = (fxp32_1 - fxp32_invb(1, factor)) >> 1;
  return min(fxp32_invb(index, factor), maxh);
}

template <>
fxp32
place_in_1d<partitioning::fractional_even>(short index, fxp32 factor) {
  if (factor < fxp32_2) { // it really can only be 0 or 1
    return index * fxp32_half;
  }
  return min(fxp32_invb(index, factor), fxp32_half);
}

template <partitioning partition>
fxp32
regularize_factor(float factor) {
  // For integer partitioning, TessFactor range is [1 ... 64] (fractions rounded up).
  return ((uint)clamp(ceil(factor), 1.0f, 64.0f)) << 16;
}

template <>
fxp32
regularize_factor<partitioning::pow2>(float factor) {
  // For pow2 partitioning, TessFactor range is [1,2,4,8,16,32,64].
  uint clamped = clamp(ceil(factor), 1.0f, 64.0f);
  return (clamped == 1 ? 1 : 1 << (32 - clz(clamped - 1))) << 16;
}

template <>
fxp32
regularize_factor<partitioning::fractional_odd>(float factor) {
  // For fractional odd partitioning, TessFactor range is [1 ... 63].
  return clamp(factor, 1.0f, 63.0f) * 65536.0;
}

template <>
fxp32
regularize_factor<partitioning::fractional_even>(float factor) {
  // For fractional even tessellation, TessFactor range is [2 ... 64].
  return clamp(factor, 2.0f, 64.0f) * 65536.0;
}

struct TessMeshWorkload {
  fxp16v2 inner0;
  fxp16v2 inner1;
  fxp16v2 outer0;
  fxp16v2 outer1;
  fxp32 inner_factor;
  fxp32 outer_factor;
  char inner_factor_i;
  char outer_factor_i;
  bool has_complement;
  bool padding;
  fxp16v2 inner0_c;
  fxp16v2 inner1_c;
  fxp16v2 outer0_c;
  fxp16v2 outer1_c;
  fxp32 inner_factor_c;
  fxp32 outer_factor_c;
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
    fxp16v2 in0, fxp16v2 in1, fxp16v2 out0, fxp16v2 out1, fxp32 inner_factor, fxp32 outer_factor,
    fxp16v2 in0_c, fxp16v2 in1_c, fxp16v2 out0_c, fxp16v2 out1_c, fxp32 inner_factor_c, fxp32 outer_factor_c, 
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

fxp32 factor_offset(fxp32 base, short count) {
  fxp32 result = base - ((count * fxp32_1) << 1);
  return result & 0x80000000 ? 0 : result;
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
  fxp32 in0f = regularize_factor<partition>(in);
  fxp32 out0f = regularize_factor<partition>(out0);
  fxp32 out1f = regularize_factor<partition>(out1);
  fxp32 out2f = regularize_factor<partition>(out2);

  if (partition == partitioning::fractional_odd) {
    fxp32 allfactor_and = out0f | out1f | out2f | in0f;
    if (allfactor_and != fxp32_1) { // any of factor is not 1
      in0f = max(fxp32_1 + 1u, in0f);
    }
  } else if (partition != partitioning::fractional_even) {
    fxp32 allfactor_and = out0f | out1f | out2f;
    if (allfactor_and != fxp32_1) {
      in0f = max(fxp32_2, in0f);
    }
  }

  char in0 = get_int_factor<partition>(in0f);

  short rings = in0 / 2;
  fxp16v2 _100{fxp16_1, fxp16_0};
  fxp16v2 _010{fxp16_0, fxp16_1};
  fxp16v2 _001{fxp16_0, fxp16_0};
  fxp16v2 _C = fxp16_one_third;

  short u = rings;

  for (short r = 0; r < u; r++, u--) {

    fxp32 outer = place_in_1d<partition>(r, in0f) << 1;
    fxp32 inner = place_in_1d<partition>(r + 1, in0f) << 1;

    fxp16v2 _001x = mix(_001, _C, outer);
    fxp16v2 _001i = mix(_001, _C, inner);
    fxp16v2 _100x = mix(_100, _C, outer);
    fxp16v2 _100i = mix(_100, _C, inner);
    fxp16v2 _010x = mix(_010, _C, outer);
    fxp16v2 _010i = mix(_010, _C, inner);


    if (r + 1 == u) {
      emit_tess_mesh_workload<partition>(
        workloads, out_count,
        _001i, _100i, _001x, _100x, factor_offset(in0f, r + 1), (r == 0 ? out1f : factor_offset(in0f, r)), 
        _100i, _010i, _100x, _010x, factor_offset(in0f, r + 1), (r == 0 ? out2f : factor_offset(in0f, r)), 
        patch_index, true
      );
      if (in0 & 1) {
        fxp32 outer = place_in_1d<partition>(rings, in0f) << 1;
        fxp16v2 _001xp = mix(_001, _C, outer);
        fxp16v2 _100xp = mix(_100, _C, outer);
        fxp16v2 _010xp = mix(_010, _C, outer);

        emit_tess_mesh_workload<partition>(
          workloads, out_count,
          _010i, _001i, _010x, _001x, factor_offset(in0f, r + 1), (r == 0 ? out0f : factor_offset(in0f ,r)),
          _010xp, _010xp, _001xp, _100xp, fxp32_0, fxp32_1,
          patch_index, true
        );
      } else {
        emit_tess_mesh_workload<partition>(
          workloads, out_count,
          _010i, _001i, _010x, _001x, factor_offset(in0f, r + 1), (r == 0 ? out0f : factor_offset(in0f, r)), 
          _C, _C, _C, _C, 0, 0,
          patch_index, false
        );
      }
      return;
    }

    fxp32 outer_c = place_in_1d<partition>(u - 1, in0f) << 1;
    fxp32 inner_c = place_in_1d<partition>(u, in0f) << 1;

    fxp16v2 _001x_c = mix(_001, _C, outer_c);
    fxp16v2 _001i_c = mix(_001, _C, inner_c);
    fxp16v2 _100x_c = mix(_100, _C, outer_c);
    fxp16v2 _100i_c = mix(_100, _C, inner_c);
    fxp16v2 _010x_c = mix(_010, _C, outer_c);
    fxp16v2 _010i_c = mix(_010, _C, inner_c);


    emit_tess_mesh_workload<partition>(
        workloads, out_count, _001i, _100i, _001x, _100x, factor_offset(in0f, r + 1),
        (r == 0 ? out1f : factor_offset(in0f, r)), 
        _001i_c, _100i_c, _001x_c, _100x_c, factor_offset(in0f, u), factor_offset(in0f, u - 1), 
        patch_index, true
    );
    emit_tess_mesh_workload<partition>(
        workloads, out_count, _100i, _010i, _100x, _010x, factor_offset(in0f, r + 1),
        (r == 0 ? out2f : factor_offset(in0f, r)),
        _100i_c, _010i_c, _100x_c, _010x_c, factor_offset(in0f, u), factor_offset(in0f, u - 1), 
        patch_index, true
    );
    emit_tess_mesh_workload<partition>(
        workloads, out_count, _010i, _001i, _010x, _001x, factor_offset(in0f, r + 1),
        (r == 0 ? out0f : factor_offset(in0f, r)),
        _010i_c, _001i_c, _010x_c, _001x_c, factor_offset(in0f, u), factor_offset(in0f, u - 1), 
        patch_index, true
    );
  }

  if (in0 & 1) {
    fxp32 outer = place_in_1d<partition>(rings, in0f) << 1;
    fxp16v2 _001x = mix(_001, _C, outer);
    fxp16v2 _100x = mix(_100, _C, outer);
    fxp16v2 _010x = mix(_010, _C, outer);
    emit_tess_mesh_workload<partition>(workloads, out_count,
      _010x, _010x, _001x, _100x, fxp32_0, fxp32_1,
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

fxp16v2 pxmy(fxp32 x, fxp32 y) {
  return {(fxp16)(x >> 1), (fxp16)(fxp16_1 - (y >> 1))};
}

fxp16v2 pxpy(fxp32 x, fxp32 y) {
  return {(fxp16)(x >> 1), (fxp16)(y >> 1)};
}

fxp16v2 mxpy(fxp32 x, fxp32 y) {
  return {(fxp16)(fxp16_1 - (x >> 1)), (fxp16)(y >> 1)};
}

fxp16v2 mxmy(fxp32 x, fxp32 y) {
  return {(fxp16)(fxp16_1 - (x >> 1)), (fxp16)(fxp16_1 - (y >> 1))};
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
  fxp32 in0f = regularize_factor<partition>(in0);
  fxp32 in1f = regularize_factor<partition>(in1);
  fxp32 out0f = regularize_factor<partition>(out0);
  fxp32 out1f = regularize_factor<partition>(out1);
  fxp32 out2f = regularize_factor<partition>(out2);
  fxp32 out3f = regularize_factor<partition>(out3);

  if (partition == partitioning::fractional_odd) {
    fxp32 allfactor_and = out0f | out1f | out2f | out3f | in0f | in1f;
    if (allfactor_and != fxp32_1) { // any of factor is not 1
      in0f = max(fxp32_1 + 1u, in0f);
      in1f = max(fxp32_1 + 1u, in1f);
    }
  } else if (partition != partitioning::fractional_even) {
    fxp32 allfactor_and = out0f | out1f | out2f | out3f;
    if (allfactor_and != fxp32_1) {
      in0f = max(fxp32_2, in0f);
      in1f = max(fxp32_2, in1f);
    }
  }

  char in0i = get_int_factor<partition>(in0f);
  char in1i = get_int_factor<partition>(in1f);

  short min_in = min(in0i, in1i);
  short rings = min_in / 2;

  short u = max(in0i, in1i) / 2;

  for (short r = 0; r < u && r < rings; r++, u--) {

    fxp32 x_coord = place_in_1d<partition>(r, in0f);
    fxp32 y_coord = place_in_1d<partition>(r, in1f);
    fxp32 x_coordi = place_in_1d<partition>(r + 1, in0f);
    fxp32 y_coordi = place_in_1d<partition>(r + 1, in1f);

    fxp16v2 _01x = pxmy(x_coord, y_coord);
    fxp16v2 _11x = mxmy(x_coord, y_coord);
    fxp16v2 _00x = pxpy(x_coord, y_coord);
    fxp16v2 _10x = mxpy(x_coord, y_coord);
    fxp16v2 _01i = pxmy(x_coordi, y_coordi);
    fxp16v2 _11i = mxmy(x_coordi, y_coordi);
    fxp16v2 _00i = pxpy(x_coordi, y_coordi);
    fxp16v2 _10i = mxpy(x_coordi, y_coordi);

    if (r + 1 == u) {
      emit_tess_mesh_workload<partition>(
          workloads, out_count,
          _11i, _01i, _11x, _01x, factor_offset(in0f, r + 1), (r == 0 ? out3f : factor_offset(in0f, r)),
          _10i, _11i, _10x, _11x, factor_offset(in1f, r + 1), (r == 0 ? out2f : factor_offset(in1f, r)),
          patch_index, true
      );
      emit_tess_mesh_workload<partition>(
          workloads, out_count,
          _00i, _10i, _00x, _10x, factor_offset(in0f, r + 1), (r == 0 ? out1f : factor_offset(in0f, r)),
          _01i, _00i, _01x, _00x, factor_offset(in1f, r + 1), (r == 0 ? out0f : factor_offset(in1f, r)),
         patch_index, true
      );
      break;
    }

    fxp32 x_coord_c = place_in_1d<partition>(u - 1, in0f);
    fxp32 y_coord_c = place_in_1d<partition>(u - 1, in1f);
    fxp32 x_coordi_c = place_in_1d<partition>(u, in0f);
    fxp32 y_coordi_c = place_in_1d<partition>(u, in1f);

    fxp16v2 _01x_c = pxmy(x_coord_c, y_coord_c);
    fxp16v2 _11x_c = mxmy(x_coord_c, y_coord_c);
    fxp16v2 _00x_c = pxpy(x_coord_c, y_coord_c);
    fxp16v2 _10x_c = mxpy(x_coord_c, y_coord_c);
    fxp16v2 _01i_c = pxmy(x_coordi_c, y_coordi_c);
    fxp16v2 _11i_c = mxmy(x_coordi_c, y_coordi_c);
    fxp16v2 _00i_c = pxpy(x_coordi_c, y_coordi_c);
    fxp16v2 _10i_c = mxpy(x_coordi_c, y_coordi_c);
  
    emit_tess_mesh_workload<partition>(
        workloads, out_count,
        _11i, _01i, _11x, _01x, factor_offset(in0f, r + 1), (r == 0 ? out3f : factor_offset(in0f, r)),
        _11i_c, _01i_c, _11x_c, _01x_c, factor_offset(in0f, u), factor_offset(in0f, u - 1),
        patch_index, u - 1 < rings
    );
    emit_tess_mesh_workload<partition>(
        workloads, out_count,
        _10i, _11i, _10x, _11x, factor_offset(in1f, r + 1), (r == 0 ? out2f : factor_offset(in1f, r)),
        _10i_c, _11i_c, _10x_c, _11x_c, factor_offset(in1f, u), factor_offset(in1f, u - 1),
        patch_index, u - 1 < rings
    );

    emit_tess_mesh_workload<partition>(
        workloads, out_count,
        _00i, _10i, _00x, _10x, factor_offset(in0f, r + 1), (r == 0 ? out1f : factor_offset(in0f, r)),
        _00i_c, _10i_c, _00x_c, _10x_c, factor_offset(in0f, u), factor_offset(in0f, u - 1),
        patch_index, u - 1 < rings
    );
    emit_tess_mesh_workload<partition>(
        workloads, out_count,
        _01i, _00i, _01x, _00x, factor_offset(in1f, r + 1), (r == 0 ? out0f : factor_offset(in1f, r)),
        _01i_c, _00i_c, _01x_c, _00x_c, factor_offset(in1f, u), factor_offset(in1f, u - 1),
        patch_index, u - 1 < rings
    );
  }

  if (min_in & 1) {
    fxp32 x_coord = place_in_1d<partition>(rings, in0f);
    fxp32 y_coord = place_in_1d<partition>(rings, in1f);

    fxp16v2 _01x = pxmy(x_coord, y_coord);
    fxp16v2 _11x = mxmy(x_coord, y_coord);
    fxp16v2 _00x = pxpy(x_coord, y_coord);
    fxp16v2 _10x = mxpy(x_coord, y_coord);

    if (in0 > in1) {
      emit_tess_mesh_workload<partition>(
          workloads, out_count, _01x,_11x, _00x,  _10x, factor_offset(in0f, rings), factor_offset(in0f, rings),
          0, 0, 0, 0, 0, 0,
          patch_index, false
      );
    } else {
      emit_tess_mesh_workload<partition>(
          workloads, out_count, _11x, _10x, _01x, _00x, factor_offset(in1f, rings), factor_offset(in1f, rings),
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
float2
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

  fxp16v2 left = workload.inner0;
  left = select(left, workload.outer0, point_at_out);
  left = select(left, workload.inner0_c, point_at_in_c);
  left = select(left, workload.outer0_c, point_at_out_c);

  fxp16v2 right = workload.inner1;
  right = select(right, workload.outer1, point_at_out);
  right = select(right, workload.inner1_c, point_at_in_c);
  right = select(right, workload.outer1_c, point_at_out_c);

  fxp32 factor = workload.inner_factor;
  factor = select(factor, workload.outer_factor, point_at_out);
  factor = select(factor, workload.inner_factor_c, point_at_in_c);
  factor = select(factor, workload.outer_factor_c, point_at_out_c);

  if (real_factor == 0)
    return to_float(min(left, right)); // very edge case...
  ushort tid_ref = real_factor - tid;
  fxp32 ratio = place_in_1d<partition>(min(tid, tid_ref), factor);
  ratio = select(ratio, fxp32_1 - ratio, tid_ref < tid);
  return to_float(mix(left, right, ratio));
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
