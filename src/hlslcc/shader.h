#pragma once

#include <vector>
#include <string>
#include <map>

#include "growing_array.h"
#include "tokens.h"
#include "reflect.h"
#include "shader_info.h"
#include "instruction.h"
#include "declaration.h"
#include "control_flow_graph.h"
#include "cbstring/bstrlib.h"

struct ConstantArrayChunk {
  ConstantArrayChunk()
      : m_Size(0), m_AccessMask(0), m_Rebase(0), m_ComponentCount(0) {}
  ConstantArrayChunk(uint32_t sz, uint32_t mask, Operand *firstUse)
      : m_Size(sz), m_AccessMask(mask), m_Rebase(0), m_ComponentCount(0) {
    m_UseSites.push_back(firstUse);
  }

  uint32_t m_Size;
  uint32_t m_AccessMask;
  uint32_t m_Rebase;
  uint32_t m_ComponentCount;

  std::vector<Operand *> m_UseSites;
};
typedef std::multimap<uint32_t, ConstantArrayChunk> ChunkMap;

struct ConstantArrayInfo {
  ConstantArrayInfo() : m_OrigDeclaration(0), m_Chunks() {}

  Declaration *m_OrigDeclaration; // Pointer to the original declaration of the
                                  // const array
  ChunkMap m_Chunks; // map of <starting offset, chunk info>, same start offset
                     // might have multiple entries for different access masks
};

class ShaderPhase {
public:
  ShaderPhase()
      : ePhase(MAIN_PHASE), ui32InstanceCount(0), postShaderCode(),
        hasPostShaderCode(0), earlyMain(), ui32OrigTemps(0), ui32TotalTemps(0),
        psTempDeclaration(NULL), pui32SplitInfo(), peTempTypes(),
        acInputNeedsRedirect(), acOutputNeedsRedirect(),
        acPatchConstantsNeedsRedirect(), m_NextFreeTempRegister(1),
        m_NextTexCoordTemp(0), m_CFGInitialized(false), m_CFG() {}

  void ResolveUAVProperties(const ShaderInfo &sInfo);

  void UnvectorizeImmMoves(); // Transform MOV tX.xyz, (0, 1, 2) into MOV tX.x,
                              // 0; MOV tX.y, 1; MOV tX.z, 2 to make datatype
                              // analysis easier

  void PruneConstArrays(); // Walk through everything that accesses a const
                           // array to see if we could make it smaller

  void ExpandSWAPCs(); // Expand all SWAPC opcodes into a bunch of MOVCs. Must
                       // be done first!

  ConstantArrayInfo m_ConstantArrayInfo;

  std::vector<Declaration> psDecl;
  std::vector<Instruction> psInst;

  SHADER_PHASE_TYPE ePhase;
  uint32_t ui32InstanceCount; // In case of hull shaders, how many instances
                              // this phase needs to have. Defaults to 1.
  bstring postShaderCode;     // End of main or before emit()
  int hasPostShaderCode;

  bstring earlyMain; // Code to be inserted at the start of phase

  uint32_t
      ui32OrigTemps; // The number of temporaries this phase originally declared
  uint32_t ui32TotalTemps; // The number of temporaries this phase has now
  Declaration *psTempDeclaration; // Shortcut to the OPCODE_DCL_TEMPS opcode

  // The split table is a table containing the index of the original register
  // this register was split out from, or 0xffffffff Format: lowest 16 bits:
  // original register. bits 16-23: rebase (eg value of 1 means .yzw was changed
  // to .xyz): bits 24-31: component count
  std::vector<uint32_t> pui32SplitInfo;
  std::vector<SHADER_VARIABLE_TYPE> peTempTypes;

  // These are needed in cases we have 2 vec2 texcoords combined into one vec4
  // and they are accessed together.
  std::vector<unsigned char>
      acInputNeedsRedirect; // If 0xff, requires re-routing all reads via a
                            // combined vec4. If 0xfe, the same but the vec4 has
                            // already been declared.
  std::vector<unsigned char> acOutputNeedsRedirect; // Same for outputs
  std::vector<unsigned char>
      acPatchConstantsNeedsRedirect; // Same for patch constants

  // Get the Control Flow Graph for this phase, build it if necessary.
  dxmt::ControlFlow::ControlFlowGraph &GetCFG();

  uint32_t m_NextFreeTempRegister; // A counter for creating new temporaries for
                                   // for-loops.
  uint32_t m_NextTexCoordTemp;     // A counter for creating tex coord temps for
                                   // driver issue workarounds

private:
  bool m_CFGInitialized;
  dxmt::ControlFlow::ControlFlowGraph m_CFG;
};

class Shader {
public:
  Shader()
      : ui32MajorVersion(0), ui32MinorVersion(0), eShaderType(INVALID_SHADER),
        fp64(0), ui32ShaderLength(0), aui32FuncTableToFuncPointer(),
        aui32FuncBodyToFuncTable(), funcTable(), funcPointer(),
        ui32NextClassFuncName(), pui32FirstToken(NULL), phases(), info(),
        abScalarInput(), abScalarOutput(), aIndexedInput(), aIndexedOutput(),
        aIndexedInputParents(), aeResourceDims(), acInputDeclared(),
        acOutputDeclared(), aiOpcodeUsed(NUM_OPCODES, 0),
        ui32CurrentVertexOutputStream(0), m_DummySamplerDeclared(false),
        maxSemanticIndex(0) {}

  // Retrieve the number of components the temp register has.
  uint32_t GetTempComponentCount(SHADER_VARIABLE_TYPE eType,
                                 uint32_t ui32Reg) const;

  // Hull shaders have multiple phases.
  // Each phase has its own temps.
  // Convert from per-phase temps to global temps.
  void ConsolidateHullTempVars();

  // Detect temp registers per data type that are actually used.
  void PruneTempRegisters();

  // Check if inputs and outputs are accessed across semantic boundaries
  // as in, 2x texcoord vec2's are packed together as vec4 but still accessed
  // together.
  void AnalyzeIOOverlap();

  // Compute maxSemanticIndex based on the results of AnalyzeIOOverlap
  void SetMaxSemanticIndex();

  // Change all references to vertex position to always be highp, having them be
  // mediump causes problems on Metal and Vivante GPUs.
  void ForcePositionToHighp();

  void FindUnusedGlobals(
      uint32_t flags); // Finds the DCL_CONSTANT_BUFFER with name "$Globals" and
                       // searches through all usages for each member of it and
                       // mark if they're actually ever used.

  void ExpandSWAPCs();

  uint32_t ui32MajorVersion;
  uint32_t ui32MinorVersion;
  SHADER_TYPE eShaderType;

  int fp64;

  // DWORDs in program code, including version and length tokens.
  uint32_t ui32ShaderLength;

  // Instruction* functions;//non-main subroutines
  dxmt::growing_vector<uint32_t>
      aui32FuncTableToFuncPointer; // dynamic alloc?
  dxmt::growing_vector<uint32_t> aui32FuncBodyToFuncTable;

  struct FuncTableEntry {
    dxmt::growing_vector<uint32_t> aui32FuncBodies;
  };
  dxmt::growing_vector<FuncTableEntry> funcTable;

  struct FuncPointerEntry {
    dxmt::growing_vector<uint32_t> aui32FuncTables;
    uint32_t ui32NumBodiesPerTable;
  };

  dxmt::growing_vector<FuncPointerEntry> funcPointer;

  dxmt::growing_vector<uint32_t> ui32NextClassFuncName;

  const uint32_t *pui32FirstToken; // Reference for calculating current position
                                   // in token stream.

  std::vector<ShaderPhase> phases;

  ShaderInfo info;

  // There are 2 input/output register spaces in DX bytecode: one for per-patch
  // data and one for per-vertex. Which one is used depends on the context:
  // per-vertex space is used in vertex/pixel/geom shaders always
  // hull shader control point phase uses per-vertex by default, other phases
  // are per-patch by default (can access per-vertex with
  // OPERAND_TYPE_I/O_CONTROL_POINT) domain shader is per-patch by default, can
  // access per-vertex with OPERAND_TYPE_I/O_CONTROL_POINT

  // Below, the [2] is accessed with 0 == per-vertex, 1 == per-patch
  // Note that these ints are component masks
  dxmt::growing_vector<int> abScalarInput[2];
  dxmt::growing_vector<int> abScalarOutput[2];

  dxmt::growing_vector<int> aIndexedInput[2];
  dxmt::growing_vector<bool> aIndexedOutput[2];

  dxmt::growing_vector<int> aIndexedInputParents[2];

  dxmt::growing_vector<RESOURCE_DIMENSION> aeResourceDims;

  dxmt::growing_vector<char> acInputDeclared[2];
  dxmt::growing_vector<char> acOutputDeclared[2];

  std::vector<int> aiOpcodeUsed; // Initialized to NUM_OPCODES elements above.

  uint32_t ui32CurrentVertexOutputStream;

  std::vector<char> psIntTempSizes;     // Array for whether this temp register
                                        // needs declaration as int temp
  std::vector<char> psInt16TempSizes;   // min16ints
  std::vector<char> psInt12TempSizes;   // min12ints
  std::vector<char> psUIntTempSizes;    // Same for uints
  std::vector<char> psUInt16TempSizes;  // ... and for uint16's
  std::vector<char> psFloatTempSizes;   // ...and for floats
  std::vector<char> psFloat16TempSizes; // ...and for min16floats
  std::vector<char> psFloat10TempSizes; // ...and for min10floats
  std::vector<char> psDoubleTempSizes;  // ...and for doubles
  std::vector<char> psBoolTempSizes;    // ... and for bools

  bool m_DummySamplerDeclared; // If true, the shader doesn't declare any
                               // samplers but uses texelFetch and we have added
                               // a dummy sampler for Vulkan for that.
  uint32_t
      maxSemanticIndex; // Highest semantic index found by SignatureAnalysis

private:
  void DoIOOverlapOperand(ShaderPhase *psPhase, Operand *psOperand);
};


#if defined(_WIN32) && defined(HLSLCC_DYNLIB)
#define HLSLCC_APIENTRY __stdcall
#if defined(libHLSLcc_EXPORTS)
#define HLSLCC_API __declspec(dllexport)
#else
#define HLSLCC_API __declspec(dllimport)
#endif
#else
#define HLSLCC_APIENTRY
#define HLSLCC_API
#endif

struct DXBCShader;

HLSLCC_API std::string
TranslateOrdinaryRenderPipelineToMSL(DXBCShader *vertex, DXBCShader *fragment);

#ifdef __cplusplus
extern "C" {
#endif

HLSLCC_API char *TranslateToMSL(DXBCShader *shader);

HLSLCC_API DXBCShader *DecodeDXBCShader(const void *bytecode);

HLSLCC_API void GetDXBCShaderInfo(DXBCShader *_shader, ShaderInfo **ShaderInfo);

HLSLCC_API void ReleaseDXBCShader(DXBCShader *shader);

HLSLCC_API SHADER_TYPE GetDXBCShaderType(DXBCShader *shader);

HLSLCC_API size_t GetShaderInputSignatureCount(DXBCShader *shader);

struct DXBCInputOutputSignature {
  char semanticName[64];
  uint32_t semanticIndex;
  uint32_t isPatchConstant;
  INOUT_COMPONENT_TYPE componentType;
  size_t numComponent;
};

HLSLCC_API void ReflectShaderOutputSignature(DXBCShader *shader);

struct DXBCVertexStageInfo {
  int numVertexInputs; //?
};

HLSLCC_API void ReflectVertexStageInfo(DXBCShader *shader,
                                       DXBCVertexStageInfo *info);

struct DXBCFragmentStageInfo {
  int numRenderTargets;
};

HLSLCC_API void ReflectFragmentStageInfo(DXBCShader *shader,
                                         DXBCFragmentStageInfo *info);

struct DXBCHullStageInfo {
  TESSELLATOR_DOMAIN domain;
  TESSELLATOR_OUTPUT_PRIMITIVE outputPrimitive;
  TESSELLATOR_PARTITIONING partition;
  float maxTessFactor;
  size_t outputControlPoints;
};

HLSLCC_API void ReflectHullStageInfo(DXBCShader *shader,
                                     DXBCHullStageInfo *info);

struct DXBCDomainStageInfo {
  size_t inputControlPoints;
};

HLSLCC_API void ReflectDomainStageInfo(DXBCShader *shader,
                                       DXBCDomainStageInfo *info);

struct DXBCGeometryStageInfo {

  uint32_t inputPrimitive;
  uint32_t outputTopology;
  size_t maxVertexOut;
};

HLSLCC_API void ReflectGeometryStageInfo(DXBCShader *shader,
                                         DXBCGeometryStageInfo *info);

struct DXBCComputeStageInfo {
  uint32_t threadGroupSize[3];
};

HLSLCC_API void ReflectComputeStageInfo(DXBCShader *shader,
                                        DXBCComputeStageInfo *info);

HLSLCC_API char *TranslateTessellationShadersToMSL(DXBCShader *vertexShader,
                                                   DXBCShader *hullShader,
                                                   DXBCShader *domainShADER);

#ifdef __cplusplus
}
#endif

