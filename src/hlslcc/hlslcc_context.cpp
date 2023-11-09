#include "hlslcc_context.h"
#include "hlslcc_toolkit.h"
#include "shader.h"
#include "data_type_analysis.h"
#include "use_define_chains.h"
#include "declaration.h"
#include "debug.h"
#include "translator.h"
#include "control_flow_graph.h"
#include "hlslcc.h"
#include <sstream>

void HLSLCrossCompilerContext::DoDataTypeAnalysis(ShaderPhase *psPhase) {
  size_t ui32DeclCount = psPhase->psDecl.size();
  uint32_t i;

  psPhase->psTempDeclaration = NULL;
  psPhase->ui32OrigTemps = 0;
  psPhase->ui32TotalTemps = 0;

  // Retrieve the temp decl count
  for (i = 0; i < ui32DeclCount; ++i) {
    if (psPhase->psDecl[i].eOpcode == OPCODE_DCL_TEMPS) {
      psPhase->ui32TotalTemps = psPhase->psDecl[i].value.ui32NumTemps;
      psPhase->psTempDeclaration = &psPhase->psDecl[i];
      break;
    }
  }

  if (psPhase->ui32TotalTemps == 0)
    return;

  psPhase->ui32OrigTemps = psPhase->ui32TotalTemps;

  // The split table is a table containing the index of the original register
  // this register was split out from, or 0xffffffff Format: lowest 16 bits:
  // original register. bits 16-23: rebase (eg value of 1 means .yzw was changed
  // to .xyz): bits 24-31: component count
  psPhase->pui32SplitInfo.clear();
  psPhase->pui32SplitInfo.resize(psPhase->ui32TotalTemps * 2, 0xffffffff);

  // Build use-define chains and split temps based on those.
  {
    DefineUseChains duChains;
    UseDefineChains udChains;

    BuildUseDefineChains(psPhase->psInst, psPhase->ui32TotalTemps, duChains,
                         udChains, psPhase->GetCFG());

    CalculateStandaloneDefinitions(duChains, psPhase->ui32TotalTemps);

    UDSplitTemps(&psPhase->ui32TotalTemps, duChains, udChains,
                 psPhase->pui32SplitInfo);

    WriteBackUsesAndDefines(duChains);
  }

  dxmt::DataTypeAnalysis::SetDataTypes(
      this, psPhase->psInst, psPhase->ui32TotalTemps, psPhase->peTempTypes);

  if (psPhase->psTempDeclaration &&
      (psPhase->ui32OrigTemps != psPhase->ui32TotalTemps))
    psPhase->psTempDeclaration->value.ui32NumTemps = psPhase->ui32TotalTemps;
}

void HLSLCrossCompilerContext::ClearDependencyData() {
  switch (psShader->eShaderType) {
  case PIXEL_SHADER: {
    dependencies->ClearCrossDependencyData();
    break;
  }
  case HULL_SHADER: {
    break;
  }
  default:
    break;
  }
}

void HLSLCrossCompilerContext::AddIndentation() {
  int i;
  bstring glsl = *currentShaderString;
  for (i = 0; i < indent; ++i) {
    bcatcstr(glsl, "    ");
  }
}

std::string HLSLCrossCompilerContext::GetDeclaredInputName(
    const Operand *psOperand, int *piRebase, int iIgnoreRedirect,
    uint32_t *puiIgnoreSwizzle) const {
  std::ostringstream oss;
  const ShaderInfo::InOutSignature *psIn = NULL;
  int regSpace = psOperand->GetRegisterSpace(this);

  if (iIgnoreRedirect == 0) {
    if ((regSpace == 0 &&
         psShader->phases[currentPhase]
                 .acInputNeedsRedirect[psOperand->ui32RegisterNumber] ==
             0xfe) ||
        (regSpace == 1 &&
         psShader->phases[currentPhase].acPatchConstantsNeedsRedirect
                 [psOperand->ui32RegisterNumber] == 0xfe)) {
      oss << "phase" << currentPhase << "_Input" << regSpace << "_"
          << psOperand->ui32RegisterNumber;
      if (piRebase)
        *piRebase = 0;
      return oss.str();
    }
  }

  if (regSpace == 0)
    psShader->info.GetInputSignatureFromRegister(
        psOperand->ui32RegisterNumber, psOperand->GetAccessMask(), &psIn, true);
  else
    psShader->info.GetPatchConstantSignatureFromRegister(
        psOperand->ui32RegisterNumber, psOperand->GetAccessMask(), &psIn, true);

  if (psIn && piRebase)
    *piRebase = psIn->iRebase;

  const std::string patchPrefix = "patch.";
  std::string res = "";

  bool skipPrefix = false;
  if (psTranslator->TranslateSystemValue(
          psOperand, psIn, res, puiIgnoreSwizzle,
          psShader->aIndexedInput[regSpace][psOperand->ui32RegisterNumber] != 0,
          true, &skipPrefix, &iIgnoreRedirect)) {
    if ((iIgnoreRedirect == 0) && !skipPrefix)
      return inputPrefix + res;
    else
      return res;
  }

  ASSERT(psIn != NULL);
  oss << inputPrefix << (regSpace == 1 ? patchPrefix : "") << psIn->semanticName
      << psIn->semanticIndex;
  return oss.str();
}

std::string HLSLCrossCompilerContext::GetDeclaredOutputName(
    const Operand *psOperand, int *piStream, uint32_t *puiIgnoreSwizzle,
    int *piRebase, int iIgnoreRedirect) const {
  std::ostringstream oss;
  const ShaderInfo::InOutSignature *psOut = NULL;
  int regSpace = psOperand->GetRegisterSpace(this);

  if (iIgnoreRedirect == 0) {
    if ((regSpace == 0 &&
         psShader->phases[currentPhase]
                 .acOutputNeedsRedirect[psOperand->ui32RegisterNumber] ==
             0xfe) ||
        (regSpace == 1 &&
         psShader->phases[currentPhase].acPatchConstantsNeedsRedirect
                 [psOperand->ui32RegisterNumber] == 0xfe)) {
      oss << "phase" << currentPhase << "_Output" << regSpace << "_"
          << psOperand->ui32RegisterNumber;
      if (piRebase)
        *piRebase = 0;
      return oss.str();
    }
  }

  if (regSpace == 0)
    psShader->info.GetOutputSignatureFromRegister(
        psOperand->ui32RegisterNumber, psOperand->GetAccessMask(),
        psShader->ui32CurrentVertexOutputStream, &psOut, true);
  else
    psShader->info.GetPatchConstantSignatureFromRegister(
        psOperand->ui32RegisterNumber, psOperand->GetAccessMask(), &psOut,
        true);

  if (psOut && piRebase)
    *piRebase = psOut->iRebase;

  if (psOut &&
      (psOut->isIndexed.find(currentPhase) != psOut->isIndexed.end())) {
    // Need to route through temp output variable
    oss << "phase" << currentPhase << "_Output" << regSpace << "_"
        << psOut->indexStart.find(currentPhase)->second;
    if (!psOperand->m_SubOperands[0].get()) {
      oss << "[" << psOperand->ui32RegisterNumber << "]";
    }
    if (piRebase)
      *piRebase = 0;
    return oss.str();
  }

  const std::string patchPrefix = "patch.";
  std::string res = "";

  if (psTranslator->TranslateSystemValue(
          psOperand, psOut, res, puiIgnoreSwizzle,
          psShader->aIndexedOutput[regSpace][psOperand->ui32RegisterNumber],
          false, NULL, &iIgnoreRedirect)) {
    // clip/cull planes will always have interim variable, as HLSL operates on
    // float4 but we need to size output accordingly with actual planes count
    // with tessellation factor buffers, a separate buffer from output is used.
    // for some reason TranslateSystemValue return *outSkipPrefix = true for ALL
    // system vars and then we simply ignore it here, so opt to modify
    // iIgnoreRedirect for these special cases

    if (regSpace == 0 && (iIgnoreRedirect == 0))
      return outputPrefix + res;
    else if (iIgnoreRedirect == 0)
      return patchPrefix + res;
    else
      return res;
  }
  ASSERT(psOut != NULL);

  oss << outputPrefix << (regSpace == 1 ? patchPrefix : "")
      << psOut->semanticName << psOut->semanticIndex;
  return oss.str();
}

bool HLSLCrossCompilerContext::OutputNeedsDeclaring(const Operand *psOperand,
                                                    const int count) {
  char compMask = (char)psOperand->ui32CompMask;
  int regSpace = psOperand->GetRegisterSpace(this);
  uint32_t startIndex = psOperand->ui32RegisterNumber +
                        (psShader->ui32CurrentVertexOutputStream *
                         1024); // Assume less than 1K input streams
  ASSERT(psShader->ui32CurrentVertexOutputStream < 4);

  // First check for various builtins, mostly depth-output ones.
  if (psShader->eShaderType == PIXEL_SHADER) {
    if (psOperand->eType == OPERAND_TYPE_OUTPUT_DEPTH_GREATER_EQUAL ||
        psOperand->eType == OPERAND_TYPE_OUTPUT_DEPTH_LESS_EQUAL) {
      return true;
    }

    if (psOperand->eType == OPERAND_TYPE_OUTPUT_DEPTH) {
      // GL doesn't need declaration, Metal does.
      return true;
    }
  }

  // Needs declaring if any of the components hasn't been already declared
  if ((compMask & ~psShader->acOutputDeclared[regSpace][startIndex]) != 0) {
    int offset;
    const ShaderInfo::InOutSignature *psSignature = NULL;

    if (psOperand->eSpecialName == NAME_UNDEFINED) {
      // Need to fetch the actual comp mask
      if (regSpace == 0)
        psShader->info.GetOutputSignatureFromRegister(
            psOperand->ui32RegisterNumber, psOperand->ui32CompMask,
            psShader->ui32CurrentVertexOutputStream, &psSignature);
      else
        psShader->info.GetPatchConstantSignatureFromRegister(
            psOperand->ui32RegisterNumber, psOperand->ui32CompMask,
            &psSignature);

      compMask = (char)psSignature->ui32Mask;
    }
    for (offset = 0; offset < count; offset++) {
      psShader->acOutputDeclared[regSpace][startIndex + offset] |= compMask;
    }

    // if (psSignature && (psSignature->semanticName == "PSIZE") &&
    //     (psShader->eTargetLanguage != LANG_METAL)) {
    //   // gl_PointSize, doesn't need declaring. TODO: Metal doesn't have
    //   // pointsize at all?
    //   return false;
    // }

    return true;
  }

  return false;
}