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
  uint clamped = clamp(factor, 1.0h, 64.0h);
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

struct Trapezoid {
  half2 inner0;
  half2 inner1;
  half2 outer0;
  half2 outer1;
  half inner_factor;
  half outer_factor;
  char inner_factor_i;
  char outer_factor_i;
  short patch_index;
};

int
get_next_index(threadgroup int *out_count) {
  return __metal_atomic_fetch_add_explicit(out_count, 1, int(memory_order_relaxed), __METAL_MEMORY_SCOPE_THREADGROUP__);
}

template <partitioning partition>
void
emit_trapezoid(
    object_data Trapezoid *trapezoids, threadgroup int *out_count, half2 in0, half2 in1, half2 out0, half2 out1,
    half inner_factor, half outer_factor, short patch_index
) {
  trapezoids[get_next_index(out_count)] = {
      in0,
      in1,
      out0,
      out1,
      inner_factor,
      outer_factor,
      get_int_factor<partition>(inner_factor),
      get_int_factor<partition>(outer_factor),
      patch_index
  };
}

template <partitioning partition>
void
gen_trapezoid_triangle_impl(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in, float out0, float out1,
    float out2
) {
  if (isnan(out0) || out0 <= 0) return;
  if (isnan(out1) || out1 <= 0) return;
  if (isnan(out2) || out2 <= 0) return;

  object_data Trapezoid *trapezoids = (object_data Trapezoid *)out_buffer;
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

  for (short r = 0; r < rings; r++) {

    half outer = 2.0h * place_in_1d<partition>(r, in0f);
    half inner = 2.0h * place_in_1d<partition>(r + 1, in0f);

    half2 _001x = mix(_001, _C, outer);
    half2 _001i = mix(_001, _C, inner);
    half2 _100x = mix(_100, _C, outer);
    half2 _100i = mix(_100, _C, inner);
    half2 _010x = mix(_010, _C, outer);
    half2 _010i = mix(_010, _C, inner);

    emit_trapezoid<partition>(
        trapezoids, out_count, _001i, _100i, _001x, _100x, half(in0f - 2.0h * (r + 1)),
        half(r == 0 ? out1f : (in0f - 2.0h * (r))), patch_index
    );
    emit_trapezoid<partition>(
        trapezoids, out_count, _100i, _010i, _100x, _010x, half(in0f - 2.0h * (r + 1)),
        half(r == 0 ? out2f : (in0f - 2.0h * (r))), patch_index
    );
    emit_trapezoid<partition>(
        trapezoids, out_count, _010i, _001i, _010x, _001x, half(in0f - 2.0h * (r + 1)),
        half(r == 0 ? out0f : (in0f - 2.0h * (r))), patch_index
    );
  }

  if (in0 & 1) {
    half outer = 2.0h * place_in_1d<partition>(rings, in0f);
    half2 _001x = mix(_001, _C, outer);
    half2 _100x = mix(_100, _C, outer);
    half2 _010x = mix(_010, _C, outer);
    emit_trapezoid<partition>(trapezoids, out_count, _010x.xy, _010x.xy, _001x.xy, _100x.xy, 0.0h, 1.0h, patch_index);
  }

  return;
}

void gen_trapezoid_triangle_integer(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in, float out0, float out1,
    float out2
) asm("dxmt.generate_trapezoid.triangle.integer");
void
gen_trapezoid_triangle_integer(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in, float out0, float out1,
    float out2
) {
  gen_trapezoid_triangle_impl<partitioning::integer>(
      patch_index, out_count, out_buffer, in, out0, out1, out2
  );
}

void gen_trapezoid_triangle_pow2(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in, float out0, float out1,
    float out2
) asm("dxmt.generate_trapezoid.triangle.pow2");
void
gen_trapezoid_triangle_pow2(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in, float out0, float out1,
    float out2
) {
  gen_trapezoid_triangle_impl<partitioning::pow2>(patch_index, out_count, out_buffer, in, out0, out1, out2);
}

void gen_trapezoid_triangle_odd(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in, float out0, float out1,
    float out2
) asm("dxmt.generate_trapezoid.triangle.odd");
void
gen_trapezoid_triangle_odd(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in, float out0, float out1,
    float out2
) {
  gen_trapezoid_triangle_impl<partitioning::fractional_odd>(
      patch_index, out_count, out_buffer, in, out0, out1, out2
  );
}

void gen_trapezoid_triangle_even(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in, float out0, float out1,
    float out2
) asm("dxmt.generate_trapezoid.triangle.even");
void
gen_trapezoid_triangle_even(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in, float out0, float out1,
    float out2
) {
  gen_trapezoid_triangle_impl<partitioning::fractional_even>(
      patch_index, out_count, out_buffer, in, out0, out1, out2
  );
}

template <partitioning partition>
void
gen_trapezoid_quad_impl(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in0, float in1, float out0,
    float out1, float out2, float out3
) {
  if (isnan(out0) || out0 <= 0) return;
  if (isnan(out1) || out1 <= 0) return;
  if (isnan(out2) || out2 <= 0) return;
  if (isnan(out3) || out3 <= 0) return;

  object_data Trapezoid *trapezoids = (object_data Trapezoid *)out_buffer;
  half in0f = regularize_factor<partition>(in0);
  half in1f = regularize_factor<partition>(in1);
  half out0f = regularize_factor<partition>(out0);
  half out1f = regularize_factor<partition>(out1);
  half out2f = regularize_factor<partition>(out2);
  half out3f = regularize_factor<partition>(out3);
  char in0i = get_int_factor<partition>(in0f);
  char in1i = get_int_factor<partition>(in1f);

  short min_in = min(in0i, in1i);
  short rings = min_in / 2;
  half2 _01{0.0f, 1.0f};
  half2 _11{1.0f, 1.0f};
  half2 _00{0.0f, 0.0f};
  half2 _10{1.0f, 0.0f};

  for (short r = 0; r < rings; r++) {

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
    emit_trapezoid<partition>(
        trapezoids, out_count, _01i, _11i, _01x, _11x, half(in0f - 2.0h * (r + 1)),
        half(r == 0 ? out3f : (in0f - 2.0h * (r))), patch_index
    );
    emit_trapezoid<partition>(
        trapezoids, out_count, _11i, _10i, _11x, _10x, half(in1f - 2.0h * (r + 1)),
        half(r == 0 ? out2f : (in1f - 2.0h * (r))), patch_index
    );

    emit_trapezoid<partition>(
        trapezoids, out_count, _10i, _00i, _10x, _00x, half(in0f - 2.0h * (r + 1)),
        half(r == 0 ? out1f : (in0f - 2.0h * (r))), patch_index
    );
    emit_trapezoid<partition>(
        trapezoids, out_count, _00i, _01i, _00x, _01x, half(in1f - 2.0h * (r + 1)),
        half(r == 0 ? out0f : (in1f - 2.0h * (r))), patch_index
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
      emit_trapezoid<partition>(
          trapezoids, out_count, _11x, _01x, _10x, _00x, half(in0f - 2.0h * rings), half(in0f - 2.0h * rings),
          patch_index
      );
    } else {
      emit_trapezoid<partition>(
          trapezoids, out_count, _10x, _11x, _00x, _01x, half(in1f - 2.0h * rings), half(in1f - 2.0h * rings),
          patch_index
      );
    }
  }

  return;
}

void gen_trapezoid_quad_integer(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in0, float in1, float out0,
    float out1, float out2, float out3
) asm("dxmt.generate_trapezoid.quad.integer");
void
gen_trapezoid_quad_integer(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in0, float in1, float out0,
    float out1, float out2, float out3
) {
  gen_trapezoid_quad_impl<partitioning::integer>(
      patch_index, out_count, out_buffer, in0, in1, out0, out1, out2, out3
  );
}

void gen_trapezoid_quad_pow2(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in0, float in1, float out0,
    float out1, float out2, float out3
) asm("dxmt.generate_trapezoid.quad.pow2");
void
gen_trapezoid_quad_pow2(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in0, float in1, float out0,
    float out1, float out2, float out3
) {
  gen_trapezoid_quad_impl<partitioning::pow2>(
      patch_index, out_count, out_buffer, in0, in1, out0, out1, out2, out3
  );
}

void gen_trapezoid_quad_odd(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in0, float in1, float out0,
    float out1, float out2, float out3
) asm("dxmt.generate_trapezoid.quad.odd");
void
gen_trapezoid_quad_odd(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in0, float in1, float out0,
    float out1, float out2, float out3
) {
  gen_trapezoid_quad_impl<partitioning::fractional_odd>(
      patch_index, out_count, out_buffer, in0, in1, out0, out1, out2, out3
  );
}

void gen_trapezoid_quad_even(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in0, float in1, float out0,
    float out1, float out2, float out3
) asm("dxmt.generate_trapezoid.quad.even");
void
gen_trapezoid_quad_even(
    int patch_index, threadgroup int *out_count, object_data int *out_buffer, float in0, float in1, float out0,
    float out1, float out2, float out3
) {
  gen_trapezoid_quad_impl<partitioning::fractional_even>(
      patch_index, out_count, out_buffer, in0, in1, out0, out1, out2, out3
  );
}

struct domain_location {
  float2 uv;
  int primitive_id;
  bool active;
};

template <partitioning partition>
half2
get_domain_point(object_data Trapezoid &trapezoid, ushort tid, short real_inner_factor, short real_outer_factor) {
  bool point_at_out_edge = tid > real_inner_factor;
  short real_factor = select(real_inner_factor, real_outer_factor, point_at_out_edge);
  tid = select(tid, (ushort)(tid - (real_inner_factor + 1)), point_at_out_edge);
  half2 left = select(trapezoid.inner0, trapezoid.outer0, point_at_out_edge);
  half2 right = select(trapezoid.inner1, trapezoid.outer1, point_at_out_edge);
  half factor = select(trapezoid.inner_factor, trapezoid.outer_factor, point_at_out_edge);
  if (real_factor == 0)
    return left; // very edge case...
  ushort tid_ref = real_factor - tid;
  half ratio = place_in_1d<partition>(min(tid, tid_ref), factor);
  ratio = select(ratio, 1.0h - ratio, tid_ref < tid);
  return mix(left, right, ratio);
}

template <partitioning partition>
domain_location
get_domain_location_impl(int trapezoid_index, int thread_index, object_data int *data) {
  domain_location ret{float2(0), 0, false};
  object_data Trapezoid &trapezoid = ((object_data Trapezoid *)data)[trapezoid_index];

  int max_in = trapezoid.inner_factor_i;
  int max_out = trapezoid.outer_factor_i;
  int count = max_in + max_out + 2;

  if (thread_index < count) {
    ret.active = true;
    ret.uv = (float2)get_domain_point<partition>(trapezoid, thread_index, max_in, max_out);
    ret.primitive_id = trapezoid.patch_index;
  }

  return ret;
};

domain_location get_domain_location_integer(int trapezoid_index, int thread_index, object_data int *data) asm(
    "dxmt.get_domain_location.integer"
);

domain_location
get_domain_location_integer(int trapezoid_index, int thread_index, object_data int *data) {
  return get_domain_location_impl<partitioning::integer>(trapezoid_index, thread_index, data);
};

domain_location get_domain_location_pow2(int trapezoid_index, int thread_index, object_data int *data) asm(
    "dxmt.get_domain_location.pow2"
);

domain_location
get_domain_location_pow2(int trapezoid_index, int thread_index, object_data int *data) {
  return get_domain_location_impl<partitioning::pow2>(trapezoid_index, thread_index, data);
};

domain_location
get_domain_location_odd(int trapezoid_index, int thread_index, object_data int *data) asm("dxmt.get_domain_location.odd"
);

domain_location
get_domain_location_odd(int trapezoid_index, int thread_index, object_data int *data) {
  return get_domain_location_impl<partitioning::fractional_odd>(trapezoid_index, thread_index, data);
};

domain_location get_domain_location_even(int trapezoid_index, int thread_index, object_data int *data) asm(
    "dxmt.get_domain_location.even"
);

domain_location
get_domain_location_even(int trapezoid_index, int thread_index, object_data int *data) {
  return get_domain_location_impl<partitioning::fractional_even>(trapezoid_index, thread_index, data);
};

struct dummy_vertex {
  float4 pos [[position]];
};

using MeshTri = metal::mesh<dummy_vertex, void, 130, 128, topology::triangle>;

void generatePrimitiveTriangle(int trapezoid_index, object_data int *data, MeshTri mesh) asm(
    "dxmt.domain_generate_primitives.triangle"
);

void
generatePrimitiveTriangle(int trapezoid_index, object_data int *data, MeshTri mesh) {
  object_data Trapezoid &trapezoid = ((object_data Trapezoid *)data)[trapezoid_index];

  short max_in = trapezoid.inner_factor_i;
  short max_out = trapezoid.outer_factor_i;
  short baseOuter = max_in + 1;
  short steps = max(max_in, max_out);

  short index_count = 0;

  for (short i = 0; i < steps; i++) {
    char outer_i0 = min(i, max_out) + baseOuter;
    char outer_i1 = min(short(i + 1), max_out) + baseOuter;
    char inner_i0 = min(i, max_in);
    char inner_i1 = min(short(i + 1), max_in);

      mesh.set_index(index_count++, outer_i0);
      mesh.set_index(index_count++, outer_i1);
      mesh.set_index(index_count++, inner_i0);
      mesh.set_index(index_count++, outer_i1);
      mesh.set_index(index_count++, inner_i1);
      mesh.set_index(index_count++, inner_i0);
  }

  mesh.set_primitive_count(index_count / 3);
}

void generatePrimitiveTriangleCCW(int trapezoid_index, object_data int *data, MeshTri mesh) asm(
    "dxmt.domain_generate_primitives.triangle_ccw"
);

void
generatePrimitiveTriangleCCW(int trapezoid_index, object_data int *data, MeshTri mesh) {
  object_data Trapezoid &trapezoid = ((object_data Trapezoid *)data)[trapezoid_index];

  short max_in = trapezoid.inner_factor_i;
  short max_out = trapezoid.outer_factor_i;
  short baseOuter = max_in + 1;
  short steps = max(max_in, max_out);

  short index_count = 0;

  for (short i = 0; i < steps; i++) {
    char outer_i0 = min(i, max_out) + baseOuter;
    char outer_i1 = min(short(i + 1), max_out) + baseOuter;
    char inner_i0 = min(i, max_in);
    char inner_i1 = min(short(i + 1), max_in);

      mesh.set_index(index_count++, outer_i0);
      mesh.set_index(index_count++, inner_i0);
      mesh.set_index(index_count++, outer_i1);
      mesh.set_index(index_count++, outer_i1);
      mesh.set_index(index_count++, inner_i0);
      mesh.set_index(index_count++, inner_i1);
  }

  mesh.set_primitive_count(index_count / 3);
}