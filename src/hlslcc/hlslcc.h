#pragma once

#include <string>
#include <vector>
#include <map>
#include <algorithm>

typedef enum INTERPOLATION_MODE {
  INTERPOLATION_UNDEFINED = 0,
  INTERPOLATION_CONSTANT = 1,
  INTERPOLATION_LINEAR = 2,
  INTERPOLATION_LINEAR_CENTROID = 3,
  INTERPOLATION_LINEAR_NOPERSPECTIVE = 4,
  INTERPOLATION_LINEAR_NOPERSPECTIVE_CENTROID = 5,
  INTERPOLATION_LINEAR_SAMPLE = 6,
  INTERPOLATION_LINEAR_NOPERSPECTIVE_SAMPLE = 7,
} INTERPOLATION_MODE;

#define TO_FLAG_NONE 0x0
#define TO_FLAG_INTEGER 0x1
#define TO_FLAG_NAME_ONLY 0x2
#define TO_FLAG_DECLARATION_NAME 0x4
#define TO_FLAG_DESTINATION 0x8 // Operand is being written to by assignment.
#define TO_FLAG_UNSIGNED_INTEGER 0x10
#define TO_FLAG_DOUBLE 0x20
// --- TO_AUTO_BITCAST_TO_FLOAT ---
// If the operand is an integer temp variable then this flag
// indicates that the temp has a valid floating point encoding
// and that the current expression expects the operand to be floating point
// and therefore intBitsToFloat must be applied to that variable.
#define TO_AUTO_BITCAST_TO_FLOAT 0x40
#define TO_AUTO_BITCAST_TO_INT 0x80
#define TO_AUTO_BITCAST_TO_UINT 0x100
// AUTO_EXPAND flags automatically expand the operand to at least (i/u)vecX
// to match HLSL functionality.
#define TO_AUTO_EXPAND_TO_VEC2 0x200
#define TO_AUTO_EXPAND_TO_VEC3 0x400
#define TO_AUTO_EXPAND_TO_VEC4 0x800
#define TO_FLAG_BOOL 0x1000
// These flags are only used for Metal:
// Force downscaling of the operand to match
// the other operand (Metal doesn't like mixing halfs with floats)
#define TO_FLAG_FORCE_HALF 0x2000

typedef enum {
  INVALID_SHADER = -1,
  PIXEL_SHADER,
  VERTEX_SHADER,
  GEOMETRY_SHADER,
  HULL_SHADER,
  DOMAIN_SHADER,
  COMPUTE_SHADER,
} SHADER_TYPE;

// Enum for texture dimension reflection data
typedef enum {
  TD_FLOAT = 0,
  TD_INT,
  TD_2D,
  TD_3D,
  TD_CUBE,
  TD_2DSHADOW,
  TD_2DARRAY,
  TD_CUBEARRAY
} HLSLCC_TEX_DIMENSION;

// The prefix for all temporary variables used by the generated code.
// Using a texture or uniform name like this will cause conflicts
#define HLSLCC_TEMP_PREFIX "u_"

typedef struct MemberDefinition {
  std::string name;
  std::string decl;
  unsigned preferredOrder;
  MemberDefinition(std::string name, std::string decl)
      : name(name), decl(decl), preferredOrder(0) {}
  MemberDefinition(std::string name, std::string decl, unsigned preferredOrder)
      : name(name), decl(decl), preferredOrder(preferredOrder) {}
} MemberDefinition;

typedef std::vector<MemberDefinition> MemberDefinitions;

// We store struct definition contents inside a vector of strings
struct StructDefinition {
  StructDefinition() : m_Members(), m_Dependencies(), m_IsPrinted(false) {}

  MemberDefinitions m_Members; // A vector of strings with the struct members
  std::vector<std::string>
      m_Dependencies; // A vector of struct names this struct depends on.
  bool m_IsPrinted;   // Has this struct been printed out yet?
};

typedef std::map<std::string, StructDefinition> StructDefinitions;

// Map of extra function definitions we need to add before the shader body but
// after the declarations.
typedef std::map<std::string, std::string> FunctionDefinitions;

// The shader stages (Vertex, Pixel et al) do not depend on each other
// in HLSL. GLSL is a different story. HLSLCrossCompiler requires
// that hull shaders must be compiled before domain shaders, and
// the pixel shader must be compiled before all of the others.
// During compilation the GLSLCrossDependencyData struct will
// carry over any information needed about a different shader stage
// in order to construct valid GLSL shader combinations.

// Using GLSLCrossDependencyData is optional. However some shader
// combinations may show link failures, or runtime errors.
class CrossShaderDependency {
public:
public:
  CrossShaderDependency()
      : fMaxTessFactor(64.0), numPatchesInThreadGroup(0),
        hasControlPoint(false), hasPatchConstant(false) {}

  // dcl_tessellator_partitioning and dcl_tessellator_output_primitive appear in
  // hull shader for D3D, but they appear on inputs inside domain shaders for
  // GL. Hull shader must be compiled before domain so the ensure correct
  // partitioning and primitive type information can be saved when compiling
  // hull and passed to domain compilation.
  float fMaxTessFactor;
  int numPatchesInThreadGroup;
  bool hasControlPoint;
  bool hasPatchConstant;

  std::vector<std::string> m_SharedDependencies;

  inline void ClearCrossDependencyData() { m_SharedDependencies.clear(); }

};

// Interface for retrieving reflection and diagnostics data
class HLSLccReflection {
public:
  HLSLccReflection() {}
  virtual ~HLSLccReflection() {}

  // Called on errors or diagnostic messages
  virtual void OnDiagnostics(const std::string &error, int line, bool isError) {
  }
};

// GS enabled?
// Affects vertex shader (i.e. need to compile vertex shader again to use
// with/without GS). This flag is needed in order for the interfaces between
// stages to match when GS is in use. PS inputs VtxGeoOutput GS outputs
// VtxGeoOutput Vs outputs VtxOutput if GS enabled. VtxGeoOutput otherwise.
static const unsigned int HLSLCC_FLAG_GS_ENABLED = 0x10;

static const unsigned int HLSLCC_FLAG_TESS_ENABLED = 0x20;

// If set, combines texture/sampler pairs used together into samplers named
// "texturename_X_samplername".
// Not necessary in Metal
// static const unsigned int HLSLCC_FLAG_COMBINE_TEXTURE_SAMPLERS = 0x200;

// If set, skips all members of the $Globals constant buffer struct that are not
// referenced in the shader code
static const unsigned int HLSLCC_FLAG_REMOVE_UNUSED_GLOBALS = 0x10000;

#define HLSLCC_TRANSLATE_MATRIX_FORMAT_STRING "hlslcc_mtx%dx%d"

// If set, translates all matrix declarations into vec4 arrays (as the DX
// bytecode treats them), and prefixes the name with 'hlslcc_mtx<rows>x<cols>'
static const unsigned int HLSLCC_FLAG_TRANSLATE_MATRICES = 0x20000;

// If set, emits Vulkan-style (set, binding) bindings, also captures that info
// from any declaration named "<Name>_hlslcc_set_X_bind_Y" Unless bindings are
// given explicitly, they are allocated into set 0 (map stored in
// GLSLCrossDependencyData)
// static const unsigned int HLSLCC_FLAG_VULKAN_BINDINGS = 0x40000;

// If set, metal output will use linear sampler for shadow compares, otherwise
// point sampler.
// static const unsigned int HLSLCC_FLAG_METAL_SHADOW_SAMPLER_LINEAR = 0x80000;

// Massage shader steps into Metal compute kernel from vertex/hull shaders +
// post-tessellation vertex shader from domain shader
static const unsigned int HLSLCC_FLAG_METAL_TESSELLATION = 0x2000000;

// If set, each line of the generated source will be preceded by a comment
// specifying which DirectX bytecode instruction it maps to
static const unsigned int HLSLCC_FLAG_INCLUDE_INSTRUCTIONS_COMMENTS =
    0x10000000;

// If set, try to generate consistent varying locations based on the semantic
// indices in the hlsl source, i.e "TEXCOORD11" gets assigned to layout(location
// = 11)
static const unsigned int HLSLCC_FLAG_KEEP_VARYING_LOCATIONS = 0x20000000;