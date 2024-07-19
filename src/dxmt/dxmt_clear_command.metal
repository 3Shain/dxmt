#include <metal_common>
#include <metal_texture>

using namespace metal;

[[kernel]] void clear_texture_1d_uint(
    texture1d<uint, access::read_write> tex [[texture(0)]],
    constant uint4& value [[buffer(1)]],
    ushort pos [[thread_position_in_grid]]
) {
  uint width = tex.get_width();
  if(width < pos) {
    tex.write(value, pos);
  }
}

[[kernel]] void clear_texture_1d_array_uint(
    texture1d_array<uint, access::read_write> tex [[texture(0)]],
    constant uint4& value [[buffer(1)]],
    ushort2 pos [[thread_position_in_grid]]
) {
  uint width = tex.get_width();
  uint array_len = tex.get_array_size();
  if(width < pos.x && array_len < pos.y) {
    tex.write(value, pos.x, pos.y);
  }
}

[[kernel]] void clear_texture_2d_uint(
    texture2d<uint, access::read_write> tex [[texture(0)]],
    constant uint4& value [[buffer(1)]],
    ushort2 pos [[thread_position_in_grid]]
) {
  uint width = tex.get_width();
  uint height = tex.get_height();
  if(width < pos.x && height < pos.y) {
    tex.write(value, pos);
  }
}

[[kernel]] void clear_texture_2d_array_uint(
    texture2d_array<uint, access::read_write> tex [[texture(0)]],
    constant uint4& value [[buffer(1)]],
    ushort3 pos [[thread_position_in_grid]]
) {
  uint width = tex.get_width();
  uint height = tex.get_height();
  uint array_len = tex.get_array_size();
  if(width < pos.x && height < pos.y && array_len < pos.z) {
    tex.write(value, pos.xy, pos.z);
  }
}

[[kernel]] void clear_texture_3d_uint(
    texture3d<uint, access::read_write> tex [[texture(0)]],
    constant uint4& value [[buffer(1)]],
    ushort3 pos [[thread_position_in_grid]]
) {
  uint width = tex.get_width();
  uint height = tex.get_height();
  uint depth = tex.get_depth();
  if(width < pos.x && height < pos.y && depth < pos.z) {
    tex.write(value, pos);
  }
}

[[kernel]] void clear_texture_buffer_uint(
    texture_buffer<uint, access::read_write> tex [[texture(0)]],
    constant uint4& value [[buffer(1)]],
    ushort pos [[thread_position_in_grid]]
) {
  uint width = tex.get_width();
  if(width < pos) {
    tex.write(value, pos);
  }
}

[[kernel]] void clear_buffer_uint(
    device uint* tex [[buffer(0)]],
    constant uint& size [[buffer(2)]],
    constant uint4& value [[buffer(1)]],
    ushort pos [[thread_position_in_grid]]
) {
  if(pos < size) {
    tex[pos] = value.x;
  }
}

[[kernel]] void clear_texture_1d_float(
    texture1d<float, access::read_write> tex [[texture(0)]],
    constant float4& value [[buffer(1)]],
    ushort pos [[thread_position_in_grid]]
) {
  uint width = tex.get_width();
  if(width < pos) {
    tex.write(value, pos);
  }
}

[[kernel]] void clear_texture_1d_array_float(
    texture1d_array<float, access::read_write> tex [[texture(0)]],
    constant float4& value [[buffer(1)]],
    ushort2 pos [[thread_position_in_grid]]
) {
  uint width = tex.get_width();
  uint array_len = tex.get_array_size();
  if(width < pos.x && array_len < pos.y) {
    tex.write(value, pos.x, pos.y);
  }
}

[[kernel]] void clear_texture_2d_float(
    texture2d<float, access::read_write> tex [[texture(0)]],
    constant float4& value [[buffer(1)]],
    ushort2 pos [[thread_position_in_grid]]
) {
  uint width = tex.get_width();
  uint height = tex.get_height();
  if(width < pos.x && height < pos.y) {
    tex.write(value, pos);
  }
}

[[kernel]] void clear_texture_2d_array_float(
    texture2d_array<float, access::read_write> tex [[texture(0)]],
    constant float4& value [[buffer(1)]],
    ushort3 pos [[thread_position_in_grid]]
) {
  uint width = tex.get_width();
  uint height = tex.get_height();
  uint array_len = tex.get_array_size();
  if(width < pos.x && height < pos.y && array_len < pos.z) {
    tex.write(value, pos.xy, pos.z);
  }
}

[[kernel]] void clear_texture_3d_float(
    texture3d<float, access::read_write> tex [[texture(0)]],
    constant float4& value [[buffer(1)]],
    ushort3 pos [[thread_position_in_grid]]
) {
  uint width = tex.get_width();
  uint height = tex.get_height();
  uint depth = tex.get_depth();
  if(width < pos.x && height < pos.y && depth < pos.z) {
    tex.write(value, pos);
  }
}

[[kernel]] void clear_texture_buffer_float(
    texture_buffer<float, access::read_write> tex [[texture(0)]],
    constant float4& value [[buffer(1)]],
    ushort pos [[thread_position_in_grid]]
) {
  uint width = tex.get_width();
  if(width < pos) {
    tex.write(value, pos);
  }
}