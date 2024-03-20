#pragma once

/*
 * This header file contains shader-related enumerations that are designed to be
 * implementation-independent in case we need these for other projects
 */

namespace dxmt::shader::common {

enum class ShaderType {
  Vertex,
  /* Metal: fragment function */
  Pixel,
  /* Metal: kernel function */
  Compute,
  /* Not present in Metal */
  Hull,
  /* Metal: post-vertex function */
  Domain,
  /* Not present in Metal */
  Geometry,
  Mesh,
  /* Metal: object function */
  Amplification,
};

enum class ScalerDataType {
  Float,
  Uint,
  Int,
  /* Unsupported in Metal */
  Double,
};

enum class ResourceType {
  TextureBuffer,
  Texture1D,
  Texture1DArray,
  Texture2D,
  Texture2DArray,
  Texture2DMultisampled,
  Texture2DMultisampledArray,
  Texture3D,
  TextureCube,
  TextureCubeArray,
};

/* It's often called System Generated Value in DirectX */
enum class InputAttribute {
  VertexId,
  PrimitiveId,
  InstanceId,

  /* For compute */

  /* uint? */
  ThreadId,
  /* uint3 */
  ThreadIdInGroup,
  /* uint? */
  ThreadGroupId,
  /* uint */
  ThreadIdInGroupFlatten,
};

/* It's often called System Interpret Value in DirectX */
enum class OutputAttribute {
  Depth,
  SampleMask,
  Coverage,
  StencilRef,
};

enum class Interpolation {
  Flat,
  Sample,
  SampleNoPerspective,
  Centroid,
  CentroidNoPerspective,
  Center,
  CenterPerspective,
};

} // namespace dxmt::shader::common