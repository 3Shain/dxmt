#include "metal.h"
#include "debug.h"
#include "hlslcc_toolkit.h"
#include "declaration.h"
#include "hlslcc_context.h"
#include <algorithm>
#include <sstream>
#include <cmath>

using namespace dxmt;

#ifndef fpcheck
#ifdef _MSC_VER
#define fpcheck(x) (_isnan(x) || !_finite(x))
#else
#define fpcheck(x) (std::isnan(x) || std::isinf(x))
#endif
#endif // #ifndef fpcheck

bool ToMetal::TranslateSystemValue(const Operand *psOperand,
                                   const ShaderInfo::InOutSignature *sig,
                                   std::string &result,
                                   uint32_t *pui32IgnoreSwizzle, bool isIndexed,
                                   bool isInput, bool *outSkipPrefix,
                                   int *iIgnoreRedirect) {
  if (sig) {
    if (psContext->psShader->eShaderType == HULL_SHADER &&
        sig->semanticName == "SV_TessFactor") {
      if (pui32IgnoreSwizzle)
        *pui32IgnoreSwizzle = 1;
      ASSERT(sig->semanticIndex <= 3);
      std::ostringstream oss;
      oss << "tessFactor.edgeTessellationFactor[" << sig->semanticIndex
          << "]";
      result = oss.str();
      if (outSkipPrefix != NULL)
        *outSkipPrefix = true;
      if (iIgnoreRedirect != NULL)
        *iIgnoreRedirect = 1;
      return true;
    }

    if (psContext->psShader->eShaderType == HULL_SHADER &&
        sig->semanticName == "SV_InsideTessFactor") {
      if (pui32IgnoreSwizzle)
        *pui32IgnoreSwizzle = 1;
      ASSERT(sig->semanticIndex <= 1);
      std::ostringstream oss;
      oss << "tessFactor.insideTessellationFactor";
      if (psContext->psShader->info.eTessDomain != TESSELLATOR_DOMAIN_TRI)
        oss << "[" << sig->semanticIndex << "]";
      result = oss.str();
      if (outSkipPrefix != NULL)
        *outSkipPrefix = true;
      if (iIgnoreRedirect != NULL)
        *iIgnoreRedirect = 1;
      return true;
    }

    if (sig->semanticName == "SV_InstanceID") {
      if (pui32IgnoreSwizzle)
        *pui32IgnoreSwizzle = 1;
    }

    if (((sig->systemValueType == NAME_POSITION ||
          sig->semanticName == "POS") &&
         sig->semanticIndex == 0) &&
        ((psContext->psShader->eShaderType == VERTEX_SHADER &&
          (psContext->flags & HLSLCC_FLAG_METAL_TESSELLATION) == 0))) {
      result = "mtl_Position";
      return true;
    }

    switch (sig->systemValueType) {
    case NAME_POSITION:
      if (psContext->psShader->eShaderType == PIXEL_SHADER)
        result = "hlslcc_FragCoord";
      else
        result = "mtl_Position";
      if (outSkipPrefix != NULL)
        *outSkipPrefix = true;
      return true;
    case NAME_RENDER_TARGET_ARRAY_INDEX:
      result = "mtl_Layer";
      if (outSkipPrefix != NULL)
        *outSkipPrefix = true;
      if (pui32IgnoreSwizzle)
        *pui32IgnoreSwizzle = 1;
      return true;
    case NAME_CLIP_DISTANCE: {
      // this is temp variable, declaration and redirecting to actual output is
      // handled in DeclareClipPlanes
      char tmpName[128];
      sprintf(tmpName, "phase%d_ClipDistance%d", psContext->currentPhase,
              sig->semanticIndex);
      result = tmpName;
      if (outSkipPrefix != NULL)
        *outSkipPrefix = true;
      if (iIgnoreRedirect != NULL)
        *iIgnoreRedirect = 1;
      return true;
    }
    case NAME_VIEWPORT_ARRAY_INDEX:
      result = "mtl_ViewPortIndex";
      if (outSkipPrefix != NULL)
        *outSkipPrefix = true;
      if (pui32IgnoreSwizzle)
        *pui32IgnoreSwizzle = 1;
      return true;
    case NAME_VERTEX_ID:
      result = "mtl_VertexID";
      if (outSkipPrefix != NULL)
        *outSkipPrefix = true;
      if (pui32IgnoreSwizzle)
        *pui32IgnoreSwizzle = 1;
      return true;
    case NAME_INSTANCE_ID:
      result = "mtl_InstanceID";
      if (pui32IgnoreSwizzle)
        *pui32IgnoreSwizzle = 1;
      if (outSkipPrefix != NULL)
        *outSkipPrefix = true;
      return true;
    case NAME_IS_FRONT_FACE:
      result = "(mtl_FrontFace ? 0xffffffffu : uint(0))";
      if (outSkipPrefix != NULL)
        *outSkipPrefix = true;
      if (pui32IgnoreSwizzle)
        *pui32IgnoreSwizzle = 1;
      return true;
    case NAME_SAMPLE_INDEX:
      result = "mtl_SampleID";
      if (outSkipPrefix != NULL)
        *outSkipPrefix = true;
      if (pui32IgnoreSwizzle)
        *pui32IgnoreSwizzle = 1;
      return true;

    default:
      break;
    }

    if (psContext->psShader->phases[psContext->currentPhase].ePhase ==
            HS_CTRL_POINT_PHASE ||
        psContext->psShader->phases[psContext->currentPhase].ePhase ==
            HS_FORK_PHASE) {
      std::ostringstream oss;
      oss << sig->semanticName << sig->semanticIndex;
      result = oss.str();
      return true;
    }
  }

  switch (psOperand->eType) {
  case OPERAND_TYPE_INPUT_COVERAGE_MASK:
  case OPERAND_TYPE_OUTPUT_COVERAGE_MASK:
    result = "mtl_CoverageMask";
    if (outSkipPrefix != NULL)
      *outSkipPrefix = true;
    if (pui32IgnoreSwizzle)
      *pui32IgnoreSwizzle = 1;
    return true;
  case OPERAND_TYPE_INPUT_THREAD_ID:
    result = "mtl_ThreadID";
    if (outSkipPrefix != NULL)
      *outSkipPrefix = true;
    return true;
  case OPERAND_TYPE_INPUT_THREAD_GROUP_ID:
    result = "mtl_ThreadGroupID";
    if (outSkipPrefix != NULL)
      *outSkipPrefix = true;
    return true;
  case OPERAND_TYPE_INPUT_THREAD_ID_IN_GROUP:
    result = "mtl_ThreadIDInGroup";
    if (outSkipPrefix != NULL)
      *outSkipPrefix = true;
    return true;
  case OPERAND_TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED:
    result = "mtl_ThreadIndexInThreadGroup";
    if (outSkipPrefix != NULL)
      *outSkipPrefix = true;
    if (pui32IgnoreSwizzle)
      *pui32IgnoreSwizzle = 1;
    return true;
  case OPERAND_TYPE_INPUT_DOMAIN_POINT:
    result = "mtl_TessCoord";
    if (outSkipPrefix != NULL)
      *outSkipPrefix = true;
    if (pui32IgnoreSwizzle)
      *pui32IgnoreSwizzle = 1;
    return true;
  case OPERAND_TYPE_OUTPUT_DEPTH:
  case OPERAND_TYPE_OUTPUT_DEPTH_GREATER_EQUAL:
  case OPERAND_TYPE_OUTPUT_DEPTH_LESS_EQUAL:
    result = "mtl_Depth";
    if (outSkipPrefix != NULL)
      *outSkipPrefix = true;
    if (pui32IgnoreSwizzle)
      *pui32IgnoreSwizzle = 1;
    return true;
  case OPERAND_TYPE_OUTPUT:
  case OPERAND_TYPE_INPUT: {
    std::ostringstream oss;
    ASSERT(sig != nullptr);
    oss << sig->semanticName << sig->semanticIndex;
    result = oss.str();
    if (dxmt::WriteMaskToComponentCount(sig->ui32Mask) == 1 &&
        pui32IgnoreSwizzle != NULL)
      *pui32IgnoreSwizzle = 1;
    return true;
  }
  case OPERAND_TYPE_INPUT_PATCH_CONSTANT: {
    std::ostringstream oss;
    ASSERT(sig != nullptr);
    oss << sig->semanticName << sig->semanticIndex;
    result = oss.str();
    if (outSkipPrefix != NULL)
      *outSkipPrefix = true;
    return true;
  }
  case OPERAND_TYPE_INPUT_CONTROL_POINT: {
    std::ostringstream oss;
    ASSERT(sig != nullptr);
    oss << sig->semanticName << sig->semanticIndex;
    result = oss.str();
    if (outSkipPrefix != NULL)
      *outSkipPrefix = true;
    return true;
    break;
  }
  default:
    ASSERT(0);
    break;
  }

  return false;
}

void ToMetal::DeclareBuiltinInput(const Declaration *psDecl) {
  const SPECIAL_NAME eSpecialName = psDecl->operands[0].eSpecialName;

  Shader *psShader = psContext->psShader;
  const Operand *psOperand = &psDecl->operands[0];
  const int regSpace = psOperand->GetRegisterSpace(psContext);
  ASSERT(regSpace == 0);

  // we need to at least mark if they are scalars or not (as we might need to
  // use vector ctor)
  if (psOperand->GetNumInputElements(psContext) == 1)
    psShader->abScalarInput[regSpace][psOperand->ui32RegisterNumber] |=
        (int)psOperand->ui32CompMask;

  switch (eSpecialName) {
  case NAME_POSITION:
    ASSERT(psContext->psShader->eShaderType == PIXEL_SHADER);
    m_StructDefinitions[""].m_Members.push_back(MemberDefinition(
        "mtl_FragCoord", "float4 mtl_FragCoord [[ position ]]"));
    bcatcstr(GetEarlyMain(psContext),
             "float4 hlslcc_FragCoord = float4(mtl_FragCoord.xyz, "
             "1.0/mtl_FragCoord.w);\n");
    break;
  case NAME_RENDER_TARGET_ARRAY_INDEX:
    // Only supported on a Mac
    m_StructDefinitions[""].m_Members.push_back(MemberDefinition(
        "mtl_Layer", "uint mtl_Layer [[ render_target_array_index ]]"));
    break;
  case NAME_CLIP_DISTANCE:
    ASSERT(0); // Should never be an input
    break;
  case NAME_VIEWPORT_ARRAY_INDEX:
    // Only supported on a Mac
    m_StructDefinitions[""].m_Members.push_back(
        MemberDefinition("mtl_ViewPortIndex",
                         "uint mtl_ViewPortIndex [[ viewport_array_index ]]"));
    break;
  case NAME_INSTANCE_ID:
    m_StructDefinitions[""].m_Members.push_back(MemberDefinition(
        "mtl_InstanceID", "uint mtl_InstanceID [[ instance_id ]]"));
    m_StructDefinitions[""].m_Members.push_back(MemberDefinition(
        "mtl_BaseInstance",
        "uint mtl_BaseInstance [[ base_instance ]]")); // Requires Metal
                                                       // runtime 1.1+
    break;
  case NAME_IS_FRONT_FACE:
    m_StructDefinitions[""].m_Members.push_back(MemberDefinition(
        "mtl_FrontFace", "bool mtl_FrontFace [[ front_facing ]]"));
    break;
  case NAME_SAMPLE_INDEX:
    m_StructDefinitions[""].m_Members.push_back(
        MemberDefinition("mtl_SampleID", "uint mtl_SampleID [[ sample_id ]]"));
    break;
  case NAME_VERTEX_ID:
    m_StructDefinitions[""].m_Members.push_back(
        MemberDefinition("mtl_VertexID", "uint mtl_VertexID [[ vertex_id ]]"));
    m_StructDefinitions[""].m_Members.push_back(MemberDefinition(
        "mtl_BaseVertex",
        "uint mtl_BaseVertex [[ base_vertex ]]")); // Requires Metal
                                                   // runtime 1.1+
    break;
  case NAME_PRIMITIVE_ID:
    // Not on Metal
    ASSERT(0);
    break;
  default:
    m_StructDefinitions[""].m_Members.push_back(MemberDefinition(
        psDecl->operands[0].specialName,
        std::string("float4 ").append(psDecl->operands[0].specialName)));
    ASSERT(0); // Catch this to see what's happening
    break;
  }
}

void ToMetal::DeclareClipPlanes(const Declaration *decl, unsigned declCount) {
  unsigned planeCount = 0;
  for (unsigned i = 0, n = declCount; i < n; ++i) {
    const Operand *operand = &decl[i].operands[0];
    if (operand->eSpecialName == NAME_CLIP_DISTANCE)
      planeCount += operand->GetMaxComponent();
  }
  if (planeCount == 0)
    return;

  std::ostringstream oss;
  oss << "float mtl_ClipDistance [[ clip_distance ]]";
  if (planeCount > 1)
    oss << "[" << planeCount << "]";
  m_StructDefinitions[GetOutputStructName()].m_Members.push_back(
      MemberDefinition(std::string("mtl_ClipDistance"), oss.str()));

  Shader *shader = psContext->psShader;

  unsigned compCount = 1;
  const ShaderInfo::InOutSignature *psFirstClipSignature;
  if (shader->info.GetOutputSignatureFromSystemValue(NAME_CLIP_DISTANCE, 0,
                                                     &psFirstClipSignature)) {
    if (psFirstClipSignature->ui32Mask & (1 << 3))
      compCount = 4;
    else if (psFirstClipSignature->ui32Mask & (1 << 2))
      compCount = 3;
    else if (psFirstClipSignature->ui32Mask & (1 << 1))
      compCount = 2;
  }

  for (unsigned i = 0, n = declCount; i < n; ++i) {
    const Operand *operand = &decl[i].operands[0];
    if (operand->eSpecialName != NAME_CLIP_DISTANCE)
      continue;

    const ShaderInfo::InOutSignature *signature = 0;
    shader->info.GetOutputSignatureFromRegister(
        operand->ui32RegisterNumber, operand->ui32CompMask, 0, &signature);
    const int semanticIndex = signature->semanticIndex;

    bformata(GetEarlyMain(psContext), "float4 phase%d_ClipDistance%d;\n",
             psContext->currentPhase, signature->semanticIndex);

    const char *swizzleStr[] = {"x", "y", "z", "w"};
    if (planeCount > 1) {
      for (int i = 0; i < compCount; ++i) {
        bformata(GetPostShaderCode(psContext),
                 "%s.mtl_ClipDistance[%d] = phase%d_ClipDistance%d.%s;\n",
                 "output", semanticIndex * compCount + i,
                 psContext->currentPhase, semanticIndex, swizzleStr[i]);
      }
    } else {
      bformata(GetPostShaderCode(psContext),
               "%s.mtl_ClipDistance = phase%d_ClipDistance%d.x;\n", "output",
               psContext->currentPhase, semanticIndex);
    }
  }
}

void ToMetal::DeclareBuiltinOutput(const Declaration *psDecl) {
  std::string out = GetOutputStructName();

  switch (psDecl->operands[0].eSpecialName) {
  case NAME_POSITION:
    m_StructDefinitions[out].m_Members.push_back(
        MemberDefinition("mtl_Position", "float4 mtl_Position [[ position ]]"));
    break;
  case NAME_RENDER_TARGET_ARRAY_INDEX:
    m_StructDefinitions[out].m_Members.push_back(MemberDefinition(
        "mtl_Layer", "uint mtl_Layer [[ render_target_array_index ]]"));
    break;
  case NAME_CLIP_DISTANCE:
    // it will be done separately in DeclareClipPlanes
    break;
  case NAME_VIEWPORT_ARRAY_INDEX:
    // Only supported on a Mac
    m_StructDefinitions[out].m_Members.push_back(
        MemberDefinition("mtl_ViewPortIndex",
                         "uint mtl_ViewPortIndex [[ viewport_array_index ]]"));
    break;
  case NAME_VERTEX_ID:
    ASSERT(0); // VertexID is not an output
    break;
  case NAME_PRIMITIVE_ID:
    // Not on Metal
    ASSERT(0);
    break;
  case NAME_INSTANCE_ID:
    ASSERT(0); // InstanceID is not an output
    break;
  case NAME_IS_FRONT_FACE:
    ASSERT(0); // FrontFacing is not an output
    break;

  // For the quadrilateral domain, there are 6 factors (4 sides, 2 inner).
  case NAME_FINAL_QUAD_U_EQ_0_EDGE_TESSFACTOR:
  case NAME_FINAL_QUAD_V_EQ_0_EDGE_TESSFACTOR:
  case NAME_FINAL_QUAD_U_EQ_1_EDGE_TESSFACTOR:
  case NAME_FINAL_QUAD_V_EQ_1_EDGE_TESSFACTOR:
  case NAME_FINAL_QUAD_U_INSIDE_TESSFACTOR:
  case NAME_FINAL_QUAD_V_INSIDE_TESSFACTOR:

  // For the triangular domain, there are 4 factors (3 sides, 1 inner)
  case NAME_FINAL_TRI_U_EQ_0_EDGE_TESSFACTOR:
  case NAME_FINAL_TRI_V_EQ_0_EDGE_TESSFACTOR:
  case NAME_FINAL_TRI_W_EQ_0_EDGE_TESSFACTOR:
  case NAME_FINAL_TRI_INSIDE_TESSFACTOR:

  // For the isoline domain, there are 2 factors (detail and density).
  case NAME_FINAL_LINE_DETAIL_TESSFACTOR:
  case NAME_FINAL_LINE_DENSITY_TESSFACTOR: {
    // Handled separately
    break;
  }
  default:
    // This might be SV_Position (because d3dcompiler is weird). Get signature
    // and check
    const ShaderInfo::InOutSignature *sig = NULL;
    psContext->psShader->info.GetOutputSignatureFromRegister(
        psDecl->operands[0].ui32RegisterNumber,
        psDecl->operands[0].GetAccessMask(), 0, &sig);
    ASSERT(sig != NULL);
    if (sig->systemValueType == NAME_POSITION && sig->semanticIndex == 0) {
      m_StructDefinitions[out].m_Members.push_back(MemberDefinition(
          "mtl_Position", "float4 mtl_Position [[ position ]]"));
      break;
    }

    ASSERT(0); // Wut
    break;
  }
}

static std::string BuildOperandTypeString(OPERAND_MIN_PRECISION ePrec,
                                          INOUT_COMPONENT_TYPE eType,
                                          int numComponents) {
  SHADER_VARIABLE_TYPE t = SVT_FLOAT;
  switch (eType) {
  case INOUT_COMPONENT_FLOAT32:
    t = SVT_FLOAT;
    break;
  case INOUT_COMPONENT_UINT32:
    t = SVT_UINT;
    break;
  case INOUT_COMPONENT_SINT32:
    t = SVT_INT;
    break;
  default:
    ASSERT(0);
    break;
  }
  // Can be overridden by precision
  switch (ePrec) {
  case OPERAND_MIN_PRECISION_DEFAULT:
    break;

  case OPERAND_MIN_PRECISION_FLOAT_16:
    ASSERT(eType == INOUT_COMPONENT_FLOAT32);
    t = SVT_FLOAT16;
    break;

  case OPERAND_MIN_PRECISION_FLOAT_2_8:
    ASSERT(eType == INOUT_COMPONENT_FLOAT32);
    t = SVT_FLOAT10;
    break;

  case OPERAND_MIN_PRECISION_SINT_16:
    ASSERT(eType == INOUT_COMPONENT_SINT32);
    t = SVT_INT16;
    break;
  case OPERAND_MIN_PRECISION_UINT_16:
    ASSERT(eType == INOUT_COMPONENT_UINT32);
    t = SVT_UINT16;
    break;
  }
  return dxmt::GetConstructorForTypeMetal(t, numComponents);
}

void ToMetal::DeclareHullShaderPassthrough() {
  uint32_t i;

  for (i = 0; i < psContext->psShader->info.inputSignatures.size(); i++) {
    ShaderInfo::InOutSignature *psSig =
        &psContext->psShader->info.inputSignatures[i];

    std::string name;
    {
      std::ostringstream oss;
      oss << psSig->semanticName << psSig->semanticIndex;
      name = oss.str();
    }

    if ((psSig->systemValueType == NAME_POSITION ||
         psSig->semanticName == "POS") &&
        psSig->semanticIndex == 0)
      name = "mtl_Position";

    uint32_t ui32NumComponents = dxmt::GetNumberBitsSet(psSig->ui32Mask);
    std::string typeName =
        BuildOperandTypeString(OPERAND_MIN_PRECISION_DEFAULT,
                               psSig->eComponentType, ui32NumComponents);

    std::ostringstream oss;
    oss << typeName << " " << name;
    oss << " [[ user(" << name << ") ]]";

    std::string declString;
    declString = oss.str();

    m_StructDefinitions[GetInputStructName()].m_Members.push_back(
        MemberDefinition(name, declString));

    std::string out = GetOutputStructName();
    m_StructDefinitions[out].m_Members.push_back(
        MemberDefinition(name, declString));

    // For preserving data layout, declare output struct as domain shader input,
    // too
    oss.str("");
    out += "In";

    oss << typeName << " " << name;

    oss << " [[ "
        << "attribute(" << psSig->ui32Register << ")"
        << " ]] ";

    m_StructDefinitions[out].m_Members.push_back(
        MemberDefinition(name, oss.str()));
  }
}

void ToMetal::HandleOutputRedirect(const Declaration *psDecl,
                                   const std::string &typeName) {
  const Operand *psOperand = &psDecl->operands[0];
  Shader *psShader = psContext->psShader;
  int needsRedirect = 0;
  const ShaderInfo::InOutSignature *psSig = NULL;

  int regSpace = psOperand->GetRegisterSpace(psContext);
  if (regSpace == 0 &&
      psShader->phases[psContext->currentPhase]
              .acOutputNeedsRedirect[psOperand->ui32RegisterNumber] == 0xff) {
    needsRedirect = 1;
  } else if (regSpace == 1 &&
             psShader->phases[psContext->currentPhase]
                     .acPatchConstantsNeedsRedirect[psOperand
                                                        ->ui32RegisterNumber] ==
                 0xff) {
    needsRedirect = 1;
  }

  if (needsRedirect == 1) {
    // TODO What if this is indexed?
    int comp = 0;
    uint32_t origMask = psOperand->ui32CompMask;

    ASSERT(psContext->psShader
               ->aIndexedOutput[regSpace][psOperand->ui32RegisterNumber] == 0);

    bformata(GetEarlyMain(psContext), "%s phase%d_Output%d_%d;\n",
             typeName.c_str(), psContext->currentPhase, regSpace,
             psOperand->ui32RegisterNumber);

    while (comp < 4) {
      int numComps = 0;
      int hasCast = 0;
      uint32_t mask, i;
      psSig = NULL;
      if (regSpace == 0)
        psContext->psShader->info.GetOutputSignatureFromRegister(
            psOperand->ui32RegisterNumber, 1 << comp,
            psContext->psShader->ui32CurrentVertexOutputStream, &psSig, true);
      else
        psContext->psShader->info.GetPatchConstantSignatureFromRegister(
            psOperand->ui32RegisterNumber, 1 << comp, &psSig, true);

      // The register isn't necessarily packed full. Continue with the next
      // component.
      if (psSig == NULL) {
        comp++;
        continue;
      }

      numComps = dxmt::GetNumberBitsSet(psSig->ui32Mask);
      mask = psSig->ui32Mask;

      ((Operand *)psOperand)->ui32CompMask = 1 << comp;
      bstring str = GetPostShaderCode(psContext);
      bcatcstr(str, TranslateOperand(psOperand, TO_FLAG_NAME_ONLY).c_str());
      bcatcstr(str, " = ");

      if (psSig->eComponentType == INOUT_COMPONENT_SINT32) {
        bformata(str, "as_type<int>(");
        hasCast = 1;
      } else if (psSig->eComponentType == INOUT_COMPONENT_UINT32) {
        bformata(str, "as_type<uint>(");
        hasCast = 1;
      }
      bformata(str, "phase%d_Output%d_%d.", psContext->currentPhase, regSpace,
               psOperand->ui32RegisterNumber);
      // Print out mask
      for (i = 0; i < 4; i++) {
        if ((mask & (1 << i)) == 0)
          continue;

        bformata(str, "%c", "xyzw"[i]);
      }

      if (hasCast)
        bcatcstr(str, ")");
      comp += numComps;
      bcatcstr(str, ";\n");
    }

    ((Operand *)psOperand)->ui32CompMask = origMask;
    if (regSpace == 0)
      psShader->phases[psContext->currentPhase]
          .acOutputNeedsRedirect[psOperand->ui32RegisterNumber] = 0xfe;
    else
      psShader->phases[psContext->currentPhase]
          .acPatchConstantsNeedsRedirect[psOperand->ui32RegisterNumber] = 0xfe;
  }
}

void ToMetal::HandleInputRedirect(const Declaration *psDecl,
                                  const std::string &typeName) {
  Operand *psOperand = (Operand *)&psDecl->operands[0];
  Shader *psShader = psContext->psShader;
  int needsRedirect = 0;
  const ShaderInfo::InOutSignature *psSig = NULL;

  int regSpace = psOperand->GetRegisterSpace(psContext);
  if (regSpace == 0) {
    if (psShader->phases[psContext->currentPhase]
            .acInputNeedsRedirect[psOperand->ui32RegisterNumber] == 0xff)
      needsRedirect = 1;
  } else if (psShader->phases[psContext->currentPhase]
                 .acPatchConstantsNeedsRedirect[psOperand
                                                    ->ui32RegisterNumber] ==
             0xff) {
    psContext->psShader->info.GetPatchConstantSignatureFromRegister(
        psOperand->ui32RegisterNumber, psOperand->ui32CompMask, &psSig);
    needsRedirect = 1;
  }

  if (needsRedirect == 1) {
    // TODO What if this is indexed?
    int needsLooping = 0;
    int i = 0;
    uint32_t origArraySize = 0;
    uint32_t origMask = psOperand->ui32CompMask;

    ASSERT(psContext->psShader
               ->aIndexedInput[regSpace][psOperand->ui32RegisterNumber] == 0);

    ++psContext->indent;

    // Does the input have multiple array components (such as geometry shader
    // input, or domain shader control point input)
    if ((psShader->eShaderType == DOMAIN_SHADER && regSpace == 0) ||
        (psShader->eShaderType == GEOMETRY_SHADER)) {
      // The count is actually stored in psOperand->aui32ArraySizes[0]
      origArraySize = psOperand->aui32ArraySizes[0];
      // bformata(glsl, "%s vec4 phase%d_Input%d_%d[%d];\n", Precision,
      // psContext->currentPhase, regSpace, psOperand->ui32RegisterNumber,
      // origArraySize);
      bformata(GetEarlyMain(psContext), "%s phase%d_Input%d_%d[%d];\n",
               typeName.c_str(), psContext->currentPhase, regSpace,
               psOperand->ui32RegisterNumber, origArraySize);
      needsLooping = 1;
      i = origArraySize - 1;
    } else
      // bformata(glsl, "%s vec4 phase%d_Input%d_%d;\n", Precision,
      // psContext->currentPhase, regSpace, psOperand->ui32RegisterNumber);
      bformata(GetEarlyMain(psContext), "%s phase%d_Input%d_%d;\n",
               typeName.c_str(), psContext->currentPhase, regSpace,
               psOperand->ui32RegisterNumber);

    // Do a conditional loop. In normal cases needsLooping == 0 so this is only
    // run once.
    do {
      int comp = 0;
      bstring str = GetEarlyMain(psContext);
      if (needsLooping)
        bformata(str, "phase%d_Input%d_%d[%d] = %s(", psContext->currentPhase,
                 regSpace, psOperand->ui32RegisterNumber, i, typeName.c_str());
      else
        bformata(str, "phase%d_Input%d_%d = %s(", psContext->currentPhase,
                 regSpace, psOperand->ui32RegisterNumber, typeName.c_str());

      while (comp < 4) {
        int numComps = 0;
        int hasCast = 0;
        int hasSig = 0;
        if (regSpace == 0)
          hasSig = psContext->psShader->info.GetInputSignatureFromRegister(
              psOperand->ui32RegisterNumber, 1 << comp, &psSig, true);
        else
          hasSig =
              psContext->psShader->info.GetPatchConstantSignatureFromRegister(
                  psOperand->ui32RegisterNumber, 1 << comp, &psSig, true);

        if (hasSig) {
          numComps = dxmt::GetNumberBitsSet(psSig->ui32Mask);
          if (psSig->eComponentType != INOUT_COMPONENT_FLOAT32) {
            if (numComps > 1)
              bformata(str, "as_type<float%d>(", numComps);
            else
              bformata(str, "as_type<float>(");
            hasCast = 1;
          }

          // Override the array size of the operand so TranslateOperand call
          // below prints the correct index
          if (needsLooping)
            psOperand->aui32ArraySizes[0] = i;

          // And the component mask
          psOperand->ui32CompMask = 1 << comp;

          bformata(str, TranslateOperand(psOperand, TO_FLAG_NAME_ONLY).c_str());

          // Restore the original array size value and mask
          psOperand->ui32CompMask = origMask;
          if (needsLooping)
            psOperand->aui32ArraySizes[0] = origArraySize;

          if (hasCast)
            bcatcstr(str, ")");
          comp += numComps;
        } else // no signature found -> fill with zero
        {
          bcatcstr(str, "0");
          comp++;
        }

        if (comp < 4)
          bcatcstr(str, ", ");
      }
      bcatcstr(str, ");\n");
    } while ((--i) >= 0);

    --psContext->indent;

    if (regSpace == 0)
      psShader->phases[psContext->currentPhase]
          .acInputNeedsRedirect[psOperand->ui32RegisterNumber] = 0xfe;
    else
      psShader->phases[psContext->currentPhase]
          .acPatchConstantsNeedsRedirect[psOperand->ui32RegisterNumber] = 0xfe;
  }
}

static std::string
TranslateResourceDeclaration(HLSLCrossCompilerContext *psContext,
                             const Declaration *psDecl, bool isDepthSampler,
                             bool isUAV) {
  std::ostringstream oss;
  const ResourceBinding *psBinding = 0;
  const RESOURCE_DIMENSION eDimension = psDecl->value.eResourceDimension;
  const uint32_t ui32RegisterNumber = psDecl->operands[0].ui32RegisterNumber;
  RESOURCE_RETURN_TYPE eType = RETURN_TYPE_UNORM;
  std::string access = "sample";

  if (isUAV) {
    if ((psDecl->sUAV.ui32AccessFlags & ACCESS_FLAG_WRITE) != 0) {
      access = "write";
      if ((psDecl->sUAV.ui32AccessFlags & ACCESS_FLAG_READ) != 0) {
        access = "read_write";
      }
    } else {
      access = "read";
      eType = psDecl->sUAV.Type;
    }
    int found;
    found = psContext->psShader->info.GetResourceFromBindingPoint(
        RGROUP_UAV, ui32RegisterNumber, &psBinding);
    if (found) {
      eType = (RESOURCE_RETURN_TYPE)psBinding->resourceReturnType;
      // Figured out by reverse engineering bitcode. flags b00xx means float1,
      // b01xx = float2, b10xx = float3 and b11xx = float4
    }
  } else {
    int found;
    found = psContext->psShader->info.GetResourceFromBindingPoint(
        RGROUP_TEXTURE, ui32RegisterNumber, &psBinding);
    if (found) {
      eType = (RESOURCE_RETURN_TYPE)psBinding->resourceReturnType;

      // TODO: it might make sense to propagate float earlier (as hlslcc might
      // declare other variables depending on sampler prec) metal supports ONLY
      // float32 depth textures
      if (isDepthSampler) {
        switch (eDimension) {
        case RESOURCE_DIMENSION_TEXTURE2D:
        case RESOURCE_DIMENSION_TEXTURE2DMS:
        case RESOURCE_DIMENSION_TEXTURECUBE:
        case RESOURCE_DIMENSION_TEXTURE2DARRAY:
        case RESOURCE_DIMENSION_TEXTURECUBEARRAY:
          eType = RETURN_TYPE_FLOAT;
          break;
        default:
          break;
        }
      }
    }
    switch (eDimension) {
    case RESOURCE_DIMENSION_BUFFER:
    case RESOURCE_DIMENSION_TEXTURE2DMS:
    case RESOURCE_DIMENSION_TEXTURE2DMSARRAY:
      access = "read";
    default:
      break;
    }
  }

  SHADER_VARIABLE_TYPE svtType = dxmt::ResourceReturnTypeToSVTType(eType);
  std::string typeName = dxmt::GetConstructorForTypeMetal(svtType, 1);

  switch (eDimension) {
  case RESOURCE_DIMENSION_BUFFER: {
    oss << "texture1d<" << typeName << ", access::" << access << " >";
    return oss.str();
    break;
  }

  case RESOURCE_DIMENSION_TEXTURE1D: {
    oss << "texture1d<" << typeName << ", access::" << access << " >";
    return oss.str();
    break;
  }

  case RESOURCE_DIMENSION_TEXTURE2D: {
    oss << (isDepthSampler ? "depth2d<" : "texture2d<") << typeName
        << ", access::" << access << " >";
    return oss.str();
    break;
  }

  case RESOURCE_DIMENSION_TEXTURE2DMS: {
    oss << (isDepthSampler ? "depth2d_ms<" : "texture2d_ms<") << typeName
        << ", access::" << access << " >";
    return oss.str();
    break;
  }

  case RESOURCE_DIMENSION_TEXTURE3D: {
    oss << "texture3d<" << typeName << ", access::" << access << " >";
    return oss.str();
    break;
  }

  case RESOURCE_DIMENSION_TEXTURECUBE: {
    oss << (isDepthSampler ? "depthcube<" : "texturecube<") << typeName
        << ", access::" << access << " >";
    return oss.str();
    break;
  }

  case RESOURCE_DIMENSION_TEXTURE1DARRAY: {
    oss << "texture1d_array<" << typeName << ", access::" << access << " >";
    return oss.str();
    break;
  }

  case RESOURCE_DIMENSION_TEXTURE2DARRAY: {
    oss << (isDepthSampler ? "depth2d_array<" : "texture2d_array<") << typeName
        << ", access::" << access << " >";
    return oss.str();
    break;
  }

  case RESOURCE_DIMENSION_TEXTURE2DMSARRAY: {
    // Not really supported in Metal but let's print it here anyway
    oss << "texture2d_ms_array<" << typeName << ", access::" << access << " >";
    return oss.str();
    break;
  }

  case RESOURCE_DIMENSION_TEXTURECUBEARRAY: {
    oss << (isDepthSampler ? "depthcube_array<" : "texturecube_array<")
        << typeName << ", access::" << access << " >";
    return oss.str();
    break;
  }
  default:
    ASSERT(0);
    oss << "texture2d<" << typeName << ", access::" << access << " >";
    return oss.str();
  }
}

static std::string GetInterpolationString(INTERPOLATION_MODE eMode) {
  switch (eMode) {
  case INTERPOLATION_CONSTANT:
    return " [[ flat ]]";

  case INTERPOLATION_LINEAR:
    return "";

  case INTERPOLATION_LINEAR_CENTROID:
    return " [[ centroid_perspective ]]";

  case INTERPOLATION_LINEAR_NOPERSPECTIVE:
    return " [[ center_no_perspective ]]";

  case INTERPOLATION_LINEAR_NOPERSPECTIVE_CENTROID:
    return " [[ centroid_no_perspective ]]";

  case INTERPOLATION_LINEAR_SAMPLE:
    return " [[ sample_perspective ]]";

  case INTERPOLATION_LINEAR_NOPERSPECTIVE_SAMPLE:
    return " [[ sample_no_perspective ]]";
  default:
    ASSERT(0);
    return "";
  }
}

void ToMetal::DeclareStructVariable(const std::string &parentName,
                                    const ShaderVar &var,
                                    uint32_t cumulativeOffset, bool isUsed) {
  DeclareStructVariable(parentName, var.type,
                        cumulativeOffset + var.ui32StartOffset, isUsed);
}

void ToMetal::DeclareStructVariable(const std::string &parentName,
                                    const ShaderVarType &var,
                                    uint32_t cumulativeOffset, bool isUsed) {
  // CB arrays need to be defined as 4 component vectors to match DX11 data
  // layout
  bool arrayWithinCB = (true && (var.Elements > 1) &&
                        (psContext->psShader->eShaderType == COMPUTE_SHADER));
  bool doDeclare = true;

  if (isUsed == false &&
      ((psContext->flags & HLSLCC_FLAG_REMOVE_UNUSED_GLOBALS)) == 0)
    isUsed = true;

  if (var.Class == SVC_STRUCT) {
    if (m_StructDefinitions.find(var.name + "_Type") ==
        m_StructDefinitions.end())
      DeclareStructType(var.name + "_Type", var.Members,
                        cumulativeOffset + var.Offset);

    std::ostringstream oss;
    oss << var.name << "_Type " << var.name;
    if (var.Elements > 1) {
      oss << "[" << var.Elements << "]";
    }
    m_StructDefinitions[parentName].m_Members.push_back(
        MemberDefinition(var.name, oss.str()));
    m_StructDefinitions[parentName].m_Dependencies.push_back(var.name +
                                                             "_Type");
    return;
  } else if (var.Class == SVC_MATRIX_COLUMNS || var.Class == SVC_MATRIX_ROWS) {
    std::ostringstream oss;
    if (psContext->flags & HLSLCC_FLAG_TRANSLATE_MATRICES) {
      // Translate matrices into vec4 arrays
      char prefix[256];
      sprintf(prefix, HLSLCC_TRANSLATE_MATRIX_FORMAT_STRING, var.Rows,
              var.Columns);
      oss << dxmt::GetConstructorForType(psContext, var.Type, 4) << " "
          << prefix << var.name;

      uint32_t elemCount =
          (var.Class == SVC_MATRIX_COLUMNS ? var.Columns : var.Rows);
      if (var.Elements > 1) {
        elemCount *= var.Elements;
      }
      oss << "[" << elemCount << "]";

    } else {
      oss << dxmt::GetMatrixTypeName(psContext, var.Type, var.Columns,
                                       var.Rows);
      oss << " " << var.name;
      if (var.Elements > 1) {
        oss << "[" << var.Elements << "]";
      }
    }

    if (doDeclare)
      m_StructDefinitions[parentName].m_Members.push_back(
          MemberDefinition(var.name, oss.str()));
  } else if (var.Class == SVC_VECTOR && var.Columns > 1) {
    std::ostringstream oss;
    oss << dxmt::GetConstructorForTypeMetal(var.Type,
                                              arrayWithinCB ? 4 : var.Columns);
    oss << " " << var.name;
    if (var.Elements > 1) {
      oss << "[" << var.Elements << "]";
    }

    if (doDeclare)
      m_StructDefinitions[parentName].m_Members.push_back(
          MemberDefinition(var.name, oss.str()));
  } else if ((var.Class == SVC_SCALAR) ||
             (var.Class == SVC_VECTOR && var.Columns == 1)) {
    if (var.Type == SVT_BOOL) {
      // Use int instead of bool.
      // Allows implicit conversions to integer and
      // bool consumes 4-bytes in HLSL and GLSL anyway.
      ((ShaderVarType &)var).Type = SVT_INT;
    }

    std::ostringstream oss;
    oss << dxmt::GetConstructorForTypeMetal(var.Type, arrayWithinCB ? 4 : 1);
    oss << " " << var.name;
    if (var.Elements > 1) {
      oss << "[" << var.Elements << "]";
    }

    if (doDeclare)
      m_StructDefinitions[parentName].m_Members.push_back(
          MemberDefinition(var.name, oss.str()));
  } else {
    ASSERT(0);
  }
}

void ToMetal::DeclareStructType(const std::string &name,
                                const std::vector<ShaderVar> &contents,
                                uint32_t cumulativeOffset,
                                bool stripUnused /* = false */) {
  for (std::vector<ShaderVar>::const_iterator itr = contents.begin();
       itr != contents.end(); itr++) {
    if (stripUnused && !itr->type.m_IsUsed)
      continue;

    DeclareStructVariable(name, *itr, cumulativeOffset, itr->type.m_IsUsed);
  }
}

void ToMetal::DeclareStructType(const std::string &name,
                                const std::vector<ShaderVarType> &contents,
                                uint32_t cumulativeOffset) {
  for (std::vector<ShaderVarType>::const_iterator itr = contents.begin();
       itr != contents.end(); itr++) {
    DeclareStructVariable(name, *itr, cumulativeOffset);
  }
}

void ToMetal::DeclareConstantBuffer(const ConstantBuffer *psCBuf,
                                    uint32_t ui32BindingPoint) {
  const bool isGlobals = (psCBuf->name == "$Globals");
  const bool stripUnused =
      isGlobals && (psContext->flags & HLSLCC_FLAG_REMOVE_UNUSED_GLOBALS);
  std::string cbname = GetCBName(psCBuf->name);

  DeclareStructType(cbname + "_Type", psCBuf->variables, 0, stripUnused);

  size_t slot = CalculateSlotInArgumentBuffer(RGROUP_CBUFFER, ui32BindingPoint);

  std::ostringstream oss;

  oss << "constant " << cbname << "_Type& " << cbname << " [[ id(" << slot
      << ") ]]";

  m_StructDefinitions[GetInputArgumentBufferName()].m_Members.push_back(
      MemberDefinition(cbname, oss.str(), slot));
  m_StructDefinitions[GetInputArgumentBufferName()].m_Dependencies.push_back(
      cbname + "_Type");
}

void ToMetal::DeclareBufferVariable(const Declaration *psDecl, bool isRaw,
                                    bool isUAV) {
  uint32_t regNo = psDecl->operands[0].ui32RegisterNumber;
  std::string BufName, BufType, BufConst;

  BufName = "";
  BufType = "";
  BufConst = "";

  BufName = ResourceName(isUAV ? RGROUP_UAV : RGROUP_TEXTURE, regNo);

  uint32_t slot =
      CalculateSlotInArgumentBuffer(isUAV ? RGROUP_UAV : RGROUP_TEXTURE, regNo);

  if (!isRaw) // declare struct containing uint array when needed
  {
    std::ostringstream typeoss;
    BufType = BufName + "_Type";
    typeoss << "uint value[";
    typeoss << psDecl->ui32BufferStride / 4 << "]";
    m_StructDefinitions[BufType].m_Members.push_back(
        MemberDefinition("value", typeoss.str()));
    m_StructDefinitions[GetInputArgumentBufferName()].m_Dependencies.push_back(
        BufType);
  }

  std::ostringstream oss;

  if (!isUAV || ((psDecl->sUAV.ui32AccessFlags & ACCESS_FLAG_WRITE) == 0)) {
    BufConst = "const ";
    oss << BufConst;
  }

  if (isRaw)
    oss << "device uint *" << BufName;
  else
    oss << "device " << BufType << " *" << BufName;

  oss << " [[ id(" << slot << ") ]]";

  m_StructDefinitions[GetInputArgumentBufferName()].m_Members.push_back(
      MemberDefinition(BufName, oss.str(), slot));

  if (psDecl->sUAV.bCounter) {
    std::ostringstream ossc;
    ossc << "device atomic_uint* " << BufName << "_counter"
         << " [[ id(" << slot + 128 + 64 << ") ]]";
    m_StructDefinitions[GetInputArgumentBufferName()].m_Members.push_back(
        MemberDefinition(BufName + "_counter", ossc.str(), slot + 128 + 64));
  }
}

static int ParseInlineSamplerWrapMode(const std::string &samplerName,
                                      const std::string &wrapName) {
  int res = 0;
  const bool hasWrap = (samplerName.find(wrapName) != std::string::npos);
  if (!hasWrap)
    return res;

  const bool hasU = (samplerName.find(wrapName + 'u') != std::string::npos);
  const bool hasV = (samplerName.find(wrapName + 'v') != std::string::npos);
  const bool hasW = (samplerName.find(wrapName + 'w') != std::string::npos);

  if (hasWrap)
    res |= 1;
  if (hasU)
    res |= 2;
  if (hasV)
    res |= 4;
  if (hasW)
    res |= 8;
  return res;
}

void ToMetal::TranslateDeclaration(const Declaration *psDecl) {
  bstring glsl = *psContext->currentShaderString;
  Shader *psShader = psContext->psShader;

  switch (psDecl->eOpcode) {
  case OPCODE_DCL_INPUT_SGV:
  case OPCODE_DCL_INPUT_PS_SGV:
    DeclareBuiltinInput(psDecl);
    break;
  case OPCODE_DCL_OUTPUT_SIV:
    DeclareBuiltinOutput(psDecl);
    break;
  case OPCODE_DCL_INPUT:
  case OPCODE_DCL_INPUT_PS_SIV:
  case OPCODE_DCL_INPUT_SIV:
  case OPCODE_DCL_INPUT_PS: {
    const Operand *psOperand = &psDecl->operands[0];

    if ((psOperand->eType == OPERAND_TYPE_OUTPUT_CONTROL_POINT_ID) ||
        (psOperand->eType == OPERAND_TYPE_INPUT_FORK_INSTANCE_ID)) {
      break;
    }

    // No need to declare patch constants read again by the hull shader.
    if ((psOperand->eType == OPERAND_TYPE_INPUT_PATCH_CONSTANT) &&
        psContext->psShader->eShaderType == HULL_SHADER) {
      break;
    }
    // ...or control points
    if ((psOperand->eType == OPERAND_TYPE_INPUT_CONTROL_POINT) &&
        psContext->psShader->eShaderType == HULL_SHADER) {
      break;
    }

    // Already declared as part of an array.
    if (psDecl->eOpcode == OPCODE_DCL_INPUT &&
        psShader->aIndexedInput[psOperand->GetRegisterSpace(psContext)]
                               [psDecl->operands[0].ui32RegisterNumber] == -1) {
      break;
    }

    uint32_t ui32Reg = psDecl->operands[0].ui32RegisterNumber;
    uint32_t ui32CompMask = psDecl->operands[0].ui32CompMask;

    std::string name =
        psContext->GetDeclaredInputName(psOperand, nullptr, 1, nullptr);

    // NB: unlike GL we keep arrays of 2-component vectors as is (without
    // collapsing into float4)
    // if(psShader->aIndexedInput[0][psDecl->asOperands[0].ui32RegisterNumber]
    // == -1)
    //     break;

    // Already declared?
    if ((ui32CompMask != 0) &&
        ((ui32CompMask & ~psShader->acInputDeclared[0][ui32Reg]) == 0)) {
      ASSERT(0); // Catch this
      break;
    }

    if (psOperand->eType == OPERAND_TYPE_INPUT_COVERAGE_MASK) {
      std::ostringstream oss;
      oss << "uint " << name << " [[ sample_mask ]]";
      m_StructDefinitions[""].m_Members.push_back(
          MemberDefinition(name, oss.str()));
      break;
    }

    if (psOperand->eType == OPERAND_TYPE_INPUT_THREAD_ID) {
      std::ostringstream oss;
      oss << "uint3 " << name << " [[ thread_position_in_grid ]]";
      m_StructDefinitions[""].m_Members.push_back(
          MemberDefinition(name, oss.str()));
      break;
    }

    if (psOperand->eType == OPERAND_TYPE_INPUT_THREAD_GROUP_ID) {
      std::ostringstream oss;
      oss << "uint3 " << name << " [[ threadgroup_position_in_grid ]]";
      m_StructDefinitions[""].m_Members.push_back(
          MemberDefinition(name, oss.str()));
      break;
    }

    if (psOperand->eType == OPERAND_TYPE_INPUT_THREAD_ID_IN_GROUP) {
      std::ostringstream oss;
      oss << "uint3 " << name << " [[ thread_position_in_threadgroup ]]";
      m_StructDefinitions[""].m_Members.push_back(
          MemberDefinition(name, oss.str()));
      break;
    }
    if (psOperand->eSpecialName == NAME_RENDER_TARGET_ARRAY_INDEX) {
      std::ostringstream oss;
      oss << "uint " << name << " [[ render_target_array_index ]]";
      m_StructDefinitions[""].m_Members.push_back(
          MemberDefinition(name, oss.str()));
      break;
    }
    if (psOperand->eType == OPERAND_TYPE_INPUT_DOMAIN_POINT) {
      std::ostringstream oss;
      std::string patchPositionType =
          psShader->info.eTessDomain == TESSELLATOR_DOMAIN_QUAD ? "float2 "
                                                                : "float3 ";
      oss << patchPositionType << name << " [[ position_in_patch ]]";
      m_StructDefinitions[""].m_Members.push_back(
          MemberDefinition(name, oss.str()));
      break;
    }
    if (psOperand->eType == OPERAND_TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED) {
      std::ostringstream oss;
      oss << "uint " << name << " [[ thread_index_in_threadgroup ]]";
      m_StructDefinitions[""].m_Members.push_back(
          MemberDefinition(name, oss.str()));
      break;
    }
    if (psOperand->eSpecialName == NAME_VIEWPORT_ARRAY_INDEX) {
      std::ostringstream oss;
      oss << "uint " << name << " [[ viewport_array_index ]]";
      m_StructDefinitions[""].m_Members.push_back(
          MemberDefinition(name, oss.str()));
      break;
    }

    if (psDecl->eOpcode == OPCODE_DCL_INPUT_PS_SIV &&
        psOperand->eSpecialName == NAME_POSITION) {
      m_StructDefinitions[""].m_Members.push_back(MemberDefinition(
          "mtl_FragCoord", "float4 mtl_FragCoord [[ position ]]"));
      bcatcstr(GetEarlyMain(psContext),
               "float4 hlslcc_FragCoord = float4(mtl_FragCoord.xyz, "
               "1.0/mtl_FragCoord.w);\n");
      break;
    }

    int regSpace = psDecl->operands[0].GetRegisterSpace(psContext);

    const ShaderInfo::InOutSignature *psSig = NULL;

    // This falls within the specified index ranges. The default is 0 if no
    // input range is specified
    if (regSpace == 0)
      psContext->psShader->info.GetInputSignatureFromRegister(
          ui32Reg, ui32CompMask, &psSig);
    else
      psContext->psShader->info.GetPatchConstantSignatureFromRegister(
          ui32Reg, ui32CompMask, &psSig);

    if (!psSig)
      break;

    // fragment shader cannot reference builtins generated by vertex program
    // (with obvious exception of position)
    // TODO: some visible error? handle more builtins?
    if (psContext->psShader->eShaderType == PIXEL_SHADER &&
        !strncmp(psSig->semanticName.c_str(), "PSIZE", 5))
      break;

    int iNumComponents = psOperand->GetNumInputElements(psContext);
    psShader->acInputDeclared[0][ui32Reg] = (char)psSig->ui32Mask;

    std::string typeName = BuildOperandTypeString(
        psOperand->eMinPrecision, psSig->eComponentType, iNumComponents);

    std::string semantic;
    if (psContext->psShader->eShaderType == VERTEX_SHADER ||
        psContext->psShader->eShaderType == HULL_SHADER ||
        psContext->psShader->eShaderType == DOMAIN_SHADER) {
      std::ostringstream oss;

      oss << "attribute(" << psSig->ui32Register << ")";
      semantic = oss.str();
    } else {
      std::ostringstream oss;

      oss << "user(" << name << ")";
      semantic = oss.str();
    }

    std::string interpolation = "";
    if (psDecl->eOpcode == OPCODE_DCL_INPUT_PS) {
      interpolation = GetInterpolationString(psDecl->value.eInterpolation);
    }

    std::string declString;
    if ((OPERAND_INDEX_DIMENSION)psOperand->iIndexDims == INDEX_2D &&
        psOperand->eType != OPERAND_TYPE_INPUT_CONTROL_POINT &&
        psContext->psShader->eShaderType != HULL_SHADER) {
      std::ostringstream oss;
      oss << typeName << " " << name << " [ " << psOperand->aui32ArraySizes[0]
          << " ] ";

      if (psContext->psShader->eShaderType != HULL_SHADER)
        oss << " [[ " << semantic << " ]] " << interpolation;
      declString = oss.str();
    } else {
      std::ostringstream oss;
      oss << typeName << " " << name;
      if (psContext->psShader->eShaderType != HULL_SHADER)
        oss << " [[ " << semantic << " ]] " << interpolation;
      declString = oss.str();
    }

    if (psOperand->eType == OPERAND_TYPE_INPUT_PATCH_CONSTANT &&
        psContext->psShader->eShaderType == DOMAIN_SHADER) {
      m_StructDefinitions["Mtl_PatchConstant"].m_Members.push_back(
          MemberDefinition(name, declString));
    } else if (psOperand->eType == OPERAND_TYPE_INPUT_CONTROL_POINT &&
               psContext->psShader->eShaderType == DOMAIN_SHADER) {
      m_StructDefinitions["Mtl_ControlPoint"].m_Members.push_back(
          MemberDefinition(name, declString));
    } else if (psContext->psShader->eShaderType == HULL_SHADER) {
      m_StructDefinitions[GetInputStructName()].m_Members.push_back(
          MemberDefinition(name, declString));
    } else {
      m_StructDefinitions[GetInputStructName()].m_Members.push_back(
          MemberDefinition(name, declString));
    }

    HandleInputRedirect(psDecl,
                        BuildOperandTypeString(psOperand->eMinPrecision,
                                               INOUT_COMPONENT_FLOAT32, 4));
    break;
  }
  case OPCODE_DCL_TEMPS: {
    uint32_t i = 0;
    const uint32_t ui32NumTemps = psDecl->value.ui32NumTemps;
    for (i = 0; i < ui32NumTemps; i++) {
      if (psShader->psFloatTempSizes[i] != 0)
        bformata(GetEarlyMain(psContext), "%s " HLSLCC_TEMP_PREFIX "%d;\n",
                 dxmt::GetConstructorForType(psContext, SVT_FLOAT,
                                               psShader->psFloatTempSizes[i]),
                 i);
      if (psShader->psFloat16TempSizes[i] != 0)
        bformata(GetEarlyMain(psContext), "%s " HLSLCC_TEMP_PREFIX "16_%d;\n",
                 dxmt::GetConstructorForType(psContext, SVT_FLOAT16,
                                               psShader->psFloat16TempSizes[i]),
                 i);
      if (psShader->psFloat10TempSizes[i] != 0)
        bformata(GetEarlyMain(psContext), "%s " HLSLCC_TEMP_PREFIX "10_%d;\n",
                 dxmt::GetConstructorForType(psContext, SVT_FLOAT10,
                                               psShader->psFloat10TempSizes[i]),
                 i);
      if (psShader->psIntTempSizes[i] != 0)
        bformata(GetEarlyMain(psContext), "%s " HLSLCC_TEMP_PREFIX "i%d;\n",
                 dxmt::GetConstructorForType(psContext, SVT_INT,
                                               psShader->psIntTempSizes[i]),
                 i);
      if (psShader->psInt16TempSizes[i] != 0)
        bformata(GetEarlyMain(psContext), "%s " HLSLCC_TEMP_PREFIX "i16_%d;\n",
                 dxmt::GetConstructorForType(psContext, SVT_INT16,
                                               psShader->psInt16TempSizes[i]),
                 i);
      if (psShader->psInt12TempSizes[i] != 0)
        bformata(GetEarlyMain(psContext), "%s " HLSLCC_TEMP_PREFIX "i12_%d;\n",
                 dxmt::GetConstructorForType(psContext, SVT_INT12,
                                               psShader->psInt12TempSizes[i]),
                 i);
      if (psShader->psUIntTempSizes[i] != 0)
        bformata(GetEarlyMain(psContext), "%s " HLSLCC_TEMP_PREFIX "u%d;\n",
                 dxmt::GetConstructorForType(psContext, SVT_UINT,
                                               psShader->psUIntTempSizes[i]),
                 i);
      if (psShader->psUInt16TempSizes[i] != 0)
        bformata(GetEarlyMain(psContext), "%s " HLSLCC_TEMP_PREFIX "u16_%d;\n",
                 dxmt::GetConstructorForType(psContext, SVT_UINT16,
                                               psShader->psUInt16TempSizes[i]),
                 i);
      if (psShader->fp64 && (psShader->psDoubleTempSizes[i] != 0))
        bformata(GetEarlyMain(psContext), "%s " HLSLCC_TEMP_PREFIX "d%d;\n",
                 dxmt::GetConstructorForType(psContext, SVT_DOUBLE,
                                               psShader->psDoubleTempSizes[i]),
                 i);
      if (psShader->psBoolTempSizes[i] != 0)
        bformata(GetEarlyMain(psContext), "%s " HLSLCC_TEMP_PREFIX "b%d;\n",
                 dxmt::GetConstructorForType(psContext, SVT_BOOL,
                                               psShader->psBoolTempSizes[i]),
                 i);
    }
    break;
  }
  case OPCODE_SPECIAL_DCL_IMMCONST: {
    ASSERT(0 && "DX9 shaders no longer supported!");
    break;
  }
  case OPCODE_DCL_CONSTANT_BUFFER: {
    const ConstantBuffer *psCBuf = NULL;
    psContext->psShader->info.GetConstantBufferFromBindingPoint(
        RGROUP_CBUFFER, psDecl->operands[0].aui32ArraySizes[0], &psCBuf);
    ASSERT(psCBuf != NULL);

    DeclareConstantBuffer(psCBuf, psDecl->operands[0].aui32ArraySizes[0]);
    break;
  }
  case OPCODE_DCL_RESOURCE: {
    DeclareResource(psDecl);
    break;
  }
  case OPCODE_DCL_OUTPUT: {
    DeclareOutput(psDecl);
    break;
  }

  case OPCODE_DCL_GLOBAL_FLAGS: {
    uint32_t ui32Flags = psDecl->value.ui32GlobalFlags;

    if (ui32Flags & GLOBAL_FLAG_FORCE_EARLY_DEPTH_STENCIL &&
        psContext->psShader->eShaderType == PIXEL_SHADER) {
      psShader->info.bEarlyFragmentTests = true;
    }
    if (!(ui32Flags & GLOBAL_FLAG_REFACTORING_ALLOWED)) {
      // TODO add precise
      // HLSL precise -
      // http://msdn.microsoft.com/en-us/library/windows/desktop/hh447204(v=vs.85).aspx
    }
    if (ui32Flags & GLOBAL_FLAG_ENABLE_DOUBLE_PRECISION_FLOAT_OPS) {
      // Not supported on Metal
      //          psShader->fp64 = 1;
    }
    break;
  }
  case OPCODE_DCL_THREAD_GROUP: {
    break;
  }
  case OPCODE_DCL_TESS_OUTPUT_PRIMITIVE: {
    if (psContext->psShader->eShaderType == HULL_SHADER) {
      psContext->psShader->info.eTessOutPrim = psDecl->value.eTessOutPrim;
      if (psContext->psShader->info.eTessOutPrim ==
          TESSELLATOR_OUTPUT_TRIANGLE_CW)
        psContext->psShader->info.eTessOutPrim =
            TESSELLATOR_OUTPUT_TRIANGLE_CCW;
      else if (psContext->psShader->info.eTessOutPrim ==
               TESSELLATOR_OUTPUT_TRIANGLE_CCW)
        psContext->psShader->info.eTessOutPrim = TESSELLATOR_OUTPUT_TRIANGLE_CW;
    }
    break;
  }
  case OPCODE_DCL_TESS_DOMAIN: {
    psContext->psShader->info.eTessDomain = psDecl->value.eTessDomain;

    if (psContext->psShader->info.eTessDomain == TESSELLATOR_DOMAIN_ISOLINE)
      psContext->m_Reflection.OnDiagnostics(
          "Metal Tessellation: domain(\"isoline\") not supported.", 0, true);
    break;
  }
  case OPCODE_DCL_TESS_PARTITIONING: {
    psContext->psShader->info.eTessPartitioning =
        psDecl->value.eTessPartitioning;
    break;
  }
  case OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY: {
    // Not supported
    break;
  }
  case OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT: {
    // Not supported
    break;
  }
  case OPCODE_DCL_GS_INPUT_PRIMITIVE: {
    // Not supported
    break;
  }
  case OPCODE_DCL_INTERFACE: {
    // Are interfaces ever even used?
    ASSERT(0);
    break;
  }
  case OPCODE_DCL_FUNCTION_BODY: {
    ASSERT(0);
    break;
  }
  case OPCODE_DCL_FUNCTION_TABLE: {
    ASSERT(0);
    break;
  }
  case OPCODE_CUSTOMDATA: {
    // TODO: This is only ever accessed as a float currently. Do trickery if we
    // ever see ints accessed from an array. Walk through all the chunks we've
    // seen in this phase.

    bstring glsl = *psContext->currentShaderString;
    bformata(glsl, "constant float4 ImmCB_%d[%d] =\n{\n",
             psContext->currentPhase, psDecl->asImmediateConstBuffer.size());
    bool isFirst = true;
    std::for_each(
        psDecl->asImmediateConstBuffer.begin(),
        psDecl->asImmediateConstBuffer.end(), [&](const ICBVec4 &data) {
          if (!isFirst) {
            bcatcstr(glsl, ",\n");
          }
          isFirst = false;

          float val[4] = {*(float *)&data.a, *(float *)&data.b,
                          *(float *)&data.c, *(float *)&data.d};

          bformata(glsl, "\tfloat4(");
          for (uint32_t k = 0; k < 4; k++) {
            if (k != 0)
              bcatcstr(glsl, ", ");
            if (fpcheck(val[k]))
              bformata(glsl, "as_type<float>(0x%Xu)", *(uint32_t *)&val[k]);
            else
              dxmt::PrintFloat(glsl, val[k]);
          }
          bcatcstr(glsl, ")");
        });
    bcatcstr(glsl, "\n};\n");
    break;
  }
  case OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT:
  case OPCODE_DCL_HS_JOIN_PHASE_INSTANCE_COUNT:
    break; // Nothing to do

  case OPCODE_DCL_INDEXABLE_TEMP: {
    const uint32_t ui32RegIndex = psDecl->sIdxTemp.ui32RegIndex;
    const uint32_t ui32RegCount = psDecl->sIdxTemp.ui32RegCount;
    const uint32_t ui32RegComponentSize = psDecl->sIdxTemp.ui32RegComponentSize;
    bformata(GetEarlyMain(psContext), "float%d TempArray%d[%d];\n",
             ui32RegComponentSize, ui32RegIndex, ui32RegCount);
    break;
  }
  case OPCODE_DCL_INDEX_RANGE: {
    switch (psDecl->operands[0].eType) {
    case OPERAND_TYPE_OUTPUT:
    case OPERAND_TYPE_INPUT: {
      const ShaderInfo::InOutSignature *psSignature = NULL;
      const char *type = "float";
      uint32_t startReg = 0;
      uint32_t i;
      bstring *oldString;
      int regSpace = psDecl->operands[0].GetRegisterSpace(psContext);
      int isInput = psDecl->operands[0].eType == OPERAND_TYPE_INPUT ? 1 : 0;

      if (regSpace == 0) {
        if (isInput)
          psShader->info.GetInputSignatureFromRegister(
              psDecl->operands[0].ui32RegisterNumber,
              psDecl->operands[0].ui32CompMask, &psSignature);
        else
          psShader->info.GetOutputSignatureFromRegister(
              psDecl->operands[0].ui32RegisterNumber,
              psDecl->operands[0].ui32CompMask,
              psShader->ui32CurrentVertexOutputStream, &psSignature);
      } else
        psShader->info.GetPatchConstantSignatureFromRegister(
            psDecl->operands[0].ui32RegisterNumber,
            psDecl->operands[0].ui32CompMask, &psSignature);

      ASSERT(psSignature != NULL);

      switch (psSignature->eComponentType) {
      case INOUT_COMPONENT_UINT32: {
        type = "uint";
        break;
      }
      case INOUT_COMPONENT_SINT32: {
        type = "int";
        break;
      }
      case INOUT_COMPONENT_FLOAT32: {
        break;
      }
      default:
        ASSERT(0);
        break;
      }

      switch (psSignature->minPrecision) // TODO What if the inputs in the indexed
                                     // range are of different precisions?
      {
      default:
        break;
      case MIN_PRECISION_ANY_16:
        ASSERT(0); // Wut?
        break;
      case MIN_PRECISION_FLOAT_16:
      case MIN_PRECISION_FLOAT_2_8:
        type = "half";
        break;
      case MIN_PRECISION_SINT_16:
        type = "short";
        break;
      case MIN_PRECISION_UINT_16:
        type = "ushort";
        break;
      }

      startReg = psDecl->operands[0].ui32RegisterNumber;
      oldString = psContext->currentShaderString;
      psContext->currentShaderString =
          &psContext->psShader->phases[psContext->currentPhase].earlyMain;
      psContext->AddIndentation();
      bformata(psContext->psShader->phases[psContext->currentPhase].earlyMain,
               "%s4 phase%d_%sput%d_%d[%d];\n", type, psContext->currentPhase,
               isInput ? "In" : "Out", regSpace, startReg,
               psDecl->value.ui32IndexRange);
      glsl =
          isInput
              ? psContext->psShader->phases[psContext->currentPhase].earlyMain
              : psContext->psShader->phases[psContext->currentPhase]
                    .postShaderCode;
      psContext->currentShaderString = &glsl;
      if (isInput == 0)
        psContext->psShader->phases[psContext->currentPhase].hasPostShaderCode =
            1;
      for (i = 0; i < psDecl->value.ui32IndexRange; i++) {
        int dummy = 0;
        std::string realName;
        uint32_t destMask = psDecl->operands[0].ui32CompMask;
        uint32_t rebase = 0;
        const ShaderInfo::InOutSignature *psSig = NULL;
        uint32_t regSpace = psDecl->operands[0].GetRegisterSpace(psContext);

        if (regSpace == 0)
          if (isInput)
            psContext->psShader->info.GetInputSignatureFromRegister(
                startReg + i, destMask, &psSig);
          else
            psContext->psShader->info.GetOutputSignatureFromRegister(
                startReg + i, destMask, 0, &psSig);
        else
          psContext->psShader->info.GetPatchConstantSignatureFromRegister(
              startReg + i, destMask, &psSig);

        ASSERT(psSig != NULL);

        if ((psSig->ui32Mask & destMask) == 0)
          continue; // Skip dummy writes (vec2 texcoords get filled to vec4 with
                    // zeroes etc)

        while ((psSig->ui32Mask & (1 << rebase)) == 0)
          rebase++;

        ((Declaration *)psDecl)->operands[0].ui32RegisterNumber = startReg + i;

        if (isInput) {
          realName = psContext->GetDeclaredInputName(&psDecl->operands[0],
                                                     &dummy, 1, NULL);

          psContext->AddIndentation();
          bformata(glsl, "phase%d_Input%d_%d[%d]", psContext->currentPhase,
                   regSpace, startReg, i);

          if (destMask != OPERAND_4_COMPONENT_MASK_ALL) {
            int k;
            const char *swizzle = "xyzw";
            bcatcstr(glsl, ".");
            for (k = 0; k < 4; k++) {
              if ((destMask & (1 << k)) && (psSig->ui32Mask & (1 << k))) {
                bformata(glsl, "%c", swizzle[k]);
              }
            }
          }

          // for some reason input struct is missed here from
          // GetDeclaredInputName result, so add it manually
          bformata(glsl, " = input.%s", realName.c_str());
          if (destMask != OPERAND_4_COMPONENT_MASK_ALL &&
              destMask != psSig->ui32Mask) {
            int k;
            const char *swizzle = "xyzw";
            bcatcstr(glsl, ".");
            for (k = 0; k < 4; k++) {
              if ((destMask & (1 << k)) && (psSig->ui32Mask & (1 << k))) {
                bformata(glsl, "%c", swizzle[k - rebase]);
              }
            }
          }
        } else {
          realName = psContext->GetDeclaredOutputName(&psDecl->operands[0],
                                                      &dummy, NULL, NULL, 0);

          psContext->AddIndentation();
          bcatcstr(glsl, realName.c_str());
          if (destMask != OPERAND_4_COMPONENT_MASK_ALL &&
              destMask != psSig->ui32Mask) {
            int k;
            const char *swizzle = "xyzw";
            bcatcstr(glsl, ".");
            for (k = 0; k < 4; k++) {
              if ((destMask & (1 << k)) && (psSig->ui32Mask & (1 << k))) {
                bformata(glsl, "%c", swizzle[k - rebase]);
              }
            }
          }

          bformata(glsl, " = phase%d_Output%d_%d[%d]", psContext->currentPhase,
                   regSpace, startReg, i);

          if (destMask != OPERAND_4_COMPONENT_MASK_ALL) {
            int k;
            const char *swizzle = "xyzw";
            bcatcstr(glsl, ".");
            for (k = 0; k < 4; k++) {
              if ((destMask & (1 << k)) && (psSig->ui32Mask & (1 << k))) {
                bformata(glsl, "%c", swizzle[k]);
              }
            }
          }
        }

        bcatcstr(glsl, ";\n");
      }

      ((Declaration *)psDecl)->operands[0].ui32RegisterNumber = startReg;
      psContext->currentShaderString = oldString;
      glsl = *psContext->currentShaderString;

      for (i = 0; i < psDecl->value.ui32IndexRange; i++) {
        if (regSpace == 0) {
          if (isInput)
            psShader->info.GetInputSignatureFromRegister(
                psDecl->operands[0].ui32RegisterNumber + i,
                psDecl->operands[0].ui32CompMask, &psSignature);
          else
            psShader->info.GetOutputSignatureFromRegister(
                psDecl->operands[0].ui32RegisterNumber + i,
                psDecl->operands[0].ui32CompMask,
                psShader->ui32CurrentVertexOutputStream, &psSignature);
        } else
          psShader->info.GetPatchConstantSignatureFromRegister(
              psDecl->operands[0].ui32RegisterNumber + i,
              psDecl->operands[0].ui32CompMask, &psSignature);

        ASSERT(psSignature != NULL);

        ((ShaderInfo::InOutSignature *)psSignature)
            ->isIndexed.insert(psContext->currentPhase);
        ((ShaderInfo::InOutSignature *)psSignature)
            ->indexStart[psContext->currentPhase] = startReg;
        ((ShaderInfo::InOutSignature *)psSignature)
            ->index[psContext->currentPhase] = i;
      }

      break;
    }
    default:
      // TODO Input index ranges.
      ASSERT(0);
    }
    break;
  }

  case OPCODE_HS_DECLS: {
    // Not supported
    break;
  }
  case OPCODE_DCL_INPUT_CONTROL_POINT_COUNT: {
    if (psContext->psShader->eShaderType == HULL_SHADER)
      psShader->info.ui32TessInputControlPointCount =
          psDecl->value.ui32MaxOutputVertexCount;
    else if (psContext->psShader->eShaderType == DOMAIN_SHADER)
      psShader->info.ui32TessOutputControlPointCount =
          psDecl->value.ui32MaxOutputVertexCount;
    break;
  }
  case OPCODE_DCL_OUTPUT_CONTROL_POINT_COUNT: {
    if (psContext->psShader->eShaderType == HULL_SHADER)
      psShader->info.ui32TessOutputControlPointCount =
          psDecl->value.ui32MaxOutputVertexCount;
    break;
  }
  case OPCODE_HS_FORK_PHASE: {
    // Not supported
    break;
  }
  case OPCODE_HS_JOIN_PHASE: {
    // Not supported
    break;
  }
  case OPCODE_DCL_SAMPLER: {
    std::string name =
        TranslateOperand(&psDecl->operands[0], TO_FLAG_NAME_ONLY);

    {
      // for some reason we have some samplers start with "sampler" and some not
      // const bool startsWithSampler = name.find("sampler") == 0;

      std::ostringstream samplerOss;
      // samplerOss << (startsWithSampler ? "" : "sampler") << name;
      samplerOss << "sampler_" << name;
      std::string samplerName = samplerOss.str();

      auto slot = CalculateSlotInArgumentBuffer(
          RGROUP_SAMPLER, psDecl->operands[0].ui32RegisterNumber);

      std::ostringstream oss;
      oss << "sampler " << samplerName << " [[ sampler (" << slot << ") ]]";

      m_StructDefinitions[""].m_Members.push_back(
          MemberDefinition(samplerName, oss.str()));

      SamplerDesc desc = {name, psDecl->operands[0].ui32RegisterNumber,
                          psDecl->operands[0].ui32RegisterNumber};
      m_Samplers.push_back(desc);
    }

    break;
  }
  case OPCODE_DCL_HS_MAX_TESSFACTOR: {
    if (psContext->psShader->eShaderType == HULL_SHADER &&
        psContext->dependencies)
      psContext->dependencies->fMaxTessFactor = psDecl->value.fMaxTessFactor;
    break;
  }
  case OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED: {
    // A hack to support single component 32bit RWBuffers: Declare as raw
    // buffer.
    // TODO: Use textures for RWBuffers when the scripting API has actual format
    // selection etc way to flag the created ComputeBuffer as typed. Even then
    // might want to leave this hack path for 32bit (u)int typed buffers to
    // continue support atomic ops on those formats.
    if (psDecl->value.eResourceDimension == RESOURCE_DIMENSION_BUFFER) {
      DeclareBufferVariable(psDecl, true, true);
      break;
    }
    std::string texName =
        ResourceName(RGROUP_UAV, psDecl->operands[0].ui32RegisterNumber);
    size_t slot = CalculateSlotInArgumentBuffer(
        RGROUP_UAV, psDecl->operands[0].ui32RegisterNumber);
    std::string samplerTypeName =
        TranslateResourceDeclaration(psContext, psDecl, false, true);
    {

      std::ostringstream oss;
      oss << samplerTypeName << " " << texName << " [[ id(" << slot << ") ]] ";

      m_StructDefinitions[GetInputArgumentBufferName()].m_Members.push_back(
          MemberDefinition(texName, oss.str()));

      HLSLCC_TEX_DIMENSION texDim = TD_INT;
      switch (psDecl->value.eResourceDimension) {
      default:
        break;
      case RESOURCE_DIMENSION_TEXTURE2D:
      case RESOURCE_DIMENSION_TEXTURE2DMS:
        texDim = TD_2D;
        break;
      case RESOURCE_DIMENSION_TEXTURE2DARRAY:
      case RESOURCE_DIMENSION_TEXTURE2DMSARRAY:
        texDim = TD_2DARRAY;
        break;
      case RESOURCE_DIMENSION_TEXTURE3D:
        texDim = TD_3D;
        break;
      case RESOURCE_DIMENSION_TEXTURECUBE:
        texDim = TD_CUBE;
        break;
      case RESOURCE_DIMENSION_TEXTURECUBEARRAY:
        texDim = TD_CUBEARRAY;
        break;
      }
      TextureDesc desc = {texName, (int)slot, texDim, false, false, true};
      m_Textures.push_back(desc);
    }
    break;
  }

  case OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED: {
    DeclareBufferVariable(psDecl, false, true);
    break;
  }
  case OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW: {
    DeclareBufferVariable(psDecl, true, true);
    break;
  }
  case OPCODE_DCL_RESOURCE_STRUCTURED: {
    DeclareBufferVariable(psDecl, false, false);
    break;
  }
  case OPCODE_DCL_RESOURCE_RAW: {
    DeclareBufferVariable(psDecl, true, false);
    break;
  }
  case OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED: {
    ShaderVarType *psVarType =
        &psShader->info
             .sGroupSharedVarType[psDecl->operands[0].ui32RegisterNumber];
    std::ostringstream oss;
    oss << "uint value[" << psDecl->sTGSM.ui32Stride / 4 << "]";
    m_StructDefinitions[TranslateOperand(&psDecl->operands[0],
                                         TO_FLAG_NAME_ONLY) +
                        "_Type"]
        .m_Members.push_back(MemberDefinition("value", oss.str()));
    m_StructDefinitions[""].m_Dependencies.push_back(
        TranslateOperand(&psDecl->operands[0], TO_FLAG_NAME_ONLY) + "_Type");
    oss.str("");
    oss << "threadgroup "
        << TranslateOperand(&psDecl->operands[0], TO_FLAG_NAME_ONLY) << "_Type "
        << TranslateOperand(&psDecl->operands[0], TO_FLAG_NAME_ONLY) << "["
        << psDecl->sTGSM.ui32Count << "]";

    bformata(GetEarlyMain(psContext), "%s;\n", oss.str().c_str());
    psVarType->name = "$Element";

    psVarType->Columns = psDecl->sTGSM.ui32Stride / 4;
    psVarType->Elements = psDecl->sTGSM.ui32Count;
    break;
  }
  case OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW: {
    ShaderVarType *psVarType =
        &psShader->info
             .sGroupSharedVarType[psDecl->operands[0].ui32RegisterNumber];

    std::ostringstream oss;
    oss << "threadgroup uint "
        << TranslateOperand(&psDecl->operands[0], TO_FLAG_NAME_ONLY) << "["
        << (psDecl->sTGSM.ui32Count / psDecl->sTGSM.ui32Stride) << "]";

    bformata(GetEarlyMain(psContext), "%s;\n", oss.str().c_str());
    psVarType->name = "$Element";

    psVarType->Columns = 1;
    psVarType->Elements = psDecl->sTGSM.ui32Count / psDecl->sTGSM.ui32Stride;
    break;
  }

  case OPCODE_DCL_STREAM: {
    // Not supported on Metal
    break;
  }
  case OPCODE_DCL_GS_INSTANCE_COUNT: {
    // Not supported on Metal
    break;
  }

  default:
    ASSERT(0);
    break;
  }
}

std::string ToMetal::ResourceName(ResourceGroup group,
                                  const uint32_t ui32RegisterNumber) {
  const ResourceBinding *psBinding = 0;
  std::ostringstream oss;
  int found;

  found = psContext->psShader->info.GetResourceFromBindingPoint(
      group, ui32RegisterNumber, &psBinding);

  if (found) {
    size_t i = 0;
    std::string name = psBinding->name;
    uint32_t ui32ArrayOffset = ui32RegisterNumber - psBinding->bindPoint;

    while (i < name.length()) {
      // array syntax [X] becomes _0_
      // Otherwise declarations could end up as:
      // uniform sampler2D SomeTextures[0];
      // uniform sampler2D SomeTextures[1];
      if (name[i] == '[' || name[i] == ']')
        name[i] = '_';

      ++i;
    }

    if (ui32ArrayOffset) {
      oss << name << ui32ArrayOffset;
    } else {
      oss << name;
    }
    return oss.str();
  } else {
    oss << "UnknownResource" << ui32RegisterNumber;
    return oss.str();
  }
}

size_t ToMetal::CalculateSlotInArgumentBuffer(ResourceGroup group,
                                              const uint32_t registerNum) {
  switch (group) {
  default:
    ASSERT(0);
  case RGROUP_CBUFFER:
    return 32 + registerNum;
  case RGROUP_SAMPLER:
    return registerNum; // NB: still use `sampler (registerNum)`
  case RGROUP_UAV:
    return 64 + registerNum;
  case RGROUP_TEXTURE:
    return 128 + registerNum;
  }
}

void ToMetal::TranslateResourceTexture(const Declaration *psDecl,
                                       uint32_t samplerCanDoShadowCmp,
                                       HLSLCC_TEX_DIMENSION texDim) {
  std::string texName =
      ResourceName(RGROUP_TEXTURE, psDecl->operands[0].ui32RegisterNumber);
  size_t slot = CalculateSlotInArgumentBuffer(
      RGROUP_TEXTURE, psDecl->operands[0].ui32RegisterNumber);
  const bool isDepthSampler =
      (samplerCanDoShadowCmp && psDecl->ui32IsShadowTex);
  std::string samplerTypeName =
      TranslateResourceDeclaration(psContext, psDecl, isDepthSampler, false);

  bool isMS = false;
  switch (psDecl->value.eResourceDimension) {
  default:
    break;
  case RESOURCE_DIMENSION_TEXTURE2DMS:
  case RESOURCE_DIMENSION_TEXTURE2DMSARRAY:
    isMS = true;
    break;
  }

  std::ostringstream oss;
  oss << samplerTypeName << " " << texName << " [[ id(" << slot << ") ]] ";

  m_StructDefinitions[GetInputArgumentBufferName()].m_Members.push_back(
      MemberDefinition(texName, oss.str(), slot));

  TextureDesc desc = {texName, (int)slot, texDim, isMS, isDepthSampler, false};
  m_Textures.push_back(desc);
}

void ToMetal::DeclareResource(const Declaration *psDecl) {
  switch (psDecl->value.eResourceDimension) {
  case RESOURCE_DIMENSION_BUFFER: {
    // Fake single comp 32bit texel buffers by using raw buffer
    DeclareBufferVariable(psDecl, true, false);
    break;

    // TODO: re-enable this code for buffer textures when sripting API has
    // proper support for it
#if 0
            if (!psContext->psDependencies->IsMemberDeclared(texName))
            {
                uint32_t slot = m_TextureSlots.GetBindingSlot(psDecl->asOperands[0].ui32RegisterNumber, BindingSlotAllocator::Texture);
                std::string texName = TranslateOperand(&psDecl->asOperands[0], TO_FLAG_NAME_ONLY);
                std::ostringstream oss;
                oss << "device " << TranslateResourceDeclaration(psContext, psDecl, texName, false, false);

                oss << texName << " [[ texture(" << slot << ") ]]";

                m_StructDefinitions[""].m_Members.push_back(MemberDefinition(texName, oss.str()));
                psContext->m_Reflection.OnTextureBinding(texName, slot, TD_2D, false); //TODO: correct HLSLCC_TEX_DIMENSION?
            }
            break;
#endif
  }
  default:
    ASSERT(0);
    break;

  case RESOURCE_DIMENSION_TEXTURE1D: {
    TranslateResourceTexture(psDecl, 1,
                             TD_2D); // TODO: correct HLSLCC_TEX_DIMENSION?
    break;
  }
  case RESOURCE_DIMENSION_TEXTURE2D: {
    TranslateResourceTexture(psDecl, 1, TD_2D);
    break;
  }
  case RESOURCE_DIMENSION_TEXTURE2DMS: {
    TranslateResourceTexture(psDecl, 0, TD_2D);
    break;
  }
  case RESOURCE_DIMENSION_TEXTURE3D: {
    TranslateResourceTexture(psDecl, 0, TD_3D);
    break;
  }
  case RESOURCE_DIMENSION_TEXTURECUBE: {
    TranslateResourceTexture(psDecl, 1, TD_CUBE);
    break;
  }
  case RESOURCE_DIMENSION_TEXTURE1DARRAY: {
    TranslateResourceTexture(psDecl, 1,
                             TD_2DARRAY); // TODO: correct HLSLCC_TEX_DIMENSION?
    break;
  }
  case RESOURCE_DIMENSION_TEXTURE2DARRAY: {
    TranslateResourceTexture(psDecl, 1, TD_2DARRAY);
    break;
  }
  case RESOURCE_DIMENSION_TEXTURE2DMSARRAY: {
    TranslateResourceTexture(psDecl, 0, TD_2DARRAY);
    break;
  }
  case RESOURCE_DIMENSION_TEXTURECUBEARRAY: {
    TranslateResourceTexture(psDecl, 1, TD_CUBEARRAY);
    break;
  }
  }
  psContext->psShader->aeResourceDims[psDecl->operands[0].ui32RegisterNumber] =
      psDecl->value.eResourceDimension;
}

void ToMetal::DeclareOutput(const Declaration *psDecl) {
  Shader *psShader = psContext->psShader;

  if (!psContext->OutputNeedsDeclaring(&psDecl->operands[0], 1))
    return;

  const Operand *psOperand = &psDecl->operands[0];
  int iNumComponents;
  int regSpace = psDecl->operands[0].GetRegisterSpace(psContext);
  uint32_t ui32Reg = psDecl->operands[0].ui32RegisterNumber;

  const ShaderInfo::InOutSignature *psSignature = NULL;
  SHADER_VARIABLE_TYPE cType = SVT_VOID;

  if (psOperand->eType == OPERAND_TYPE_OUTPUT_DEPTH ||
      psOperand->eType == OPERAND_TYPE_OUTPUT_DEPTH_GREATER_EQUAL ||
      psOperand->eType == OPERAND_TYPE_OUTPUT_DEPTH_LESS_EQUAL) {
    iNumComponents = 1;
    cType = SVT_FLOAT;
  } else {
    if (regSpace == 0)
      psShader->info.GetOutputSignatureFromRegister(
          ui32Reg, psDecl->operands[0].ui32CompMask,
          psShader->ui32CurrentVertexOutputStream, &psSignature);
    else
      psShader->info.GetPatchConstantSignatureFromRegister(
          ui32Reg, psDecl->operands[0].ui32CompMask, &psSignature);

    iNumComponents = dxmt::GetNumberBitsSet(psSignature->ui32Mask);

    switch (psSignature->eComponentType) {
    case INOUT_COMPONENT_UINT32: {
      cType = SVT_UINT;
      break;
    }
    case INOUT_COMPONENT_SINT32: {
      cType = SVT_INT;
      break;
    }
    case INOUT_COMPONENT_FLOAT32: {
      cType = SVT_FLOAT;
      break;
    }
    default:
      ASSERT(0);
      break;
    }
    // Don't set this for oDepth (or variants), because depth output register is
    // in separate space from other outputs (regno 0, but others may overlap
    // with that)
    if (iNumComponents == 1)
      psContext->psShader->abScalarOutput[regSpace][ui32Reg] |=
          (int)psDecl->operands[0].ui32CompMask;

    switch (psOperand->eMinPrecision) {
    case OPERAND_MIN_PRECISION_DEFAULT:
      break;
    case OPERAND_MIN_PRECISION_FLOAT_16:
      cType = SVT_FLOAT16;
      break;
    case OPERAND_MIN_PRECISION_FLOAT_2_8:
      cType = SVT_FLOAT10;
      break;
    case OPERAND_MIN_PRECISION_SINT_16:
      cType = SVT_INT16;
      break;
    case OPERAND_MIN_PRECISION_UINT_16:
      cType = SVT_UINT16;
      break;
    }
  }

  std::string type = dxmt::GetConstructorForTypeMetal(cType, iNumComponents);
  std::string name = psContext->GetDeclaredOutputName(
      &psDecl->operands[0], nullptr, nullptr, nullptr, 1);

  switch (psShader->eShaderType) {
  case PIXEL_SHADER: {
    switch (psDecl->operands[0].eType) {
    case OPERAND_TYPE_OUTPUT_COVERAGE_MASK: {
      std::ostringstream oss;
      oss << type << " " << name << " [[ sample_mask ]]";
      m_StructDefinitions[GetOutputStructName()].m_Members.push_back(
          MemberDefinition(name, oss.str()));
      break;
    }
    case OPERAND_TYPE_OUTPUT_DEPTH: {
      std::ostringstream oss;
      oss << type << " " << name << " [[ depth(any) ]]";
      m_StructDefinitions[GetOutputStructName()].m_Members.push_back(
          MemberDefinition(name, oss.str()));
      break;
    }
    case OPERAND_TYPE_OUTPUT_DEPTH_GREATER_EQUAL: {
      std::ostringstream oss;
      oss << type << " " << name << " [[ depth(greater) ]]";
      m_StructDefinitions[GetOutputStructName()].m_Members.push_back(
          MemberDefinition(name, oss.str()));
      break;
    }
    case OPERAND_TYPE_OUTPUT_DEPTH_LESS_EQUAL: {
      std::ostringstream oss;
      oss << type << " " << name << " [[ depth(less) ]]";
      m_StructDefinitions[GetOutputStructName()].m_Members.push_back(
          MemberDefinition(name, oss.str()));
      break;
    }
    default: {
      std::ostringstream oss;
      oss << type << " " << name << " [[ color(xlt_remap_o["
          << psSignature->semanticIndex << "]) ]]";
      m_NeedFBOutputRemapDecl = true;
      m_StructDefinitions[GetOutputStructName()].m_Members.push_back(
          MemberDefinition(name, oss.str()));
    }
    }
    break;
  }
  case VERTEX_SHADER:
  case DOMAIN_SHADER:
  case HULL_SHADER: {
    std::string out = GetOutputStructName();
    bool isTessKernel =
        (psContext->flags & HLSLCC_FLAG_METAL_TESSELLATION) != 0 &&
        (psContext->psShader->eShaderType == HULL_SHADER ||
         psContext->psShader->eShaderType == VERTEX_SHADER);

    std::ostringstream oss;
    oss << type << " " << name;
    if (!isTessKernel &&
        (psSignature->systemValueType == NAME_POSITION ||
         psSignature->semanticName == "POS") &&
        psOperand->ui32RegisterNumber == 0)
      oss << " [[ position ]]";
    else if (!isTessKernel && psSignature->systemValueType == NAME_UNDEFINED &&
             psSignature->semanticName == "PSIZE" &&
             psSignature->semanticIndex == 0)
      oss << " [[ point_size ]]";
    else
      oss << " [[ user(" << name << ") ]]";
    m_StructDefinitions[out].m_Members.push_back(
        MemberDefinition(name, oss.str()));

    // For preserving data layout, declare output struct as domain shader input,
    // too
    if (psContext->psShader->eShaderType == HULL_SHADER) {
      // Mtl_ControlPointIn or Mtl_PatchConstantIn
      out += "In";

      std::ostringstream oss;
      oss << type << " " << name;

      oss << " [[ "
          << "attribute(" << psSignature->ui32Register << ")"
          << " ]] ";

      m_StructDefinitions[out].m_Members.push_back(
          MemberDefinition(name, oss.str()));
    }
    break;
  }
  case GEOMETRY_SHADER:
  default:
    ASSERT(0);
    break;
  }
  HandleOutputRedirect(psDecl, dxmt::GetConstructorForTypeMetal(cType, 4));
}
