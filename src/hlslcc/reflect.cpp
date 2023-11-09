#include "reflect.h"
#include "debug.h"
#include "decode.h"
#include "cbstring/bstrlib.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void SanitizeVariableName(std::string &Name) {
  /* MSDN
     http://msdn.microsoft.com/en-us/library/windows/desktop/bb944006(v=vs.85).aspx
     The uniform function parameters appear in the
     constant table prepended with a dollar sign ($),
     unlike the global variables. The dollar sign is
     required to avoid name collisions between local
     uniform inputs and global variables of the same name.*/

  /* Leave $ThisPointer, $Element and $Globals as-is.
     Otherwise remove $ character ($ is not a valid character for GLSL variable
     names). */
  if (Name[0] == '$') {
    if (strcmp(Name.c_str(), "$Element") != 0 &&
        strcmp(Name.c_str(), "$Globals") != 0 &&
        strcmp(Name.c_str(), "$ThisPointer") != 0) {
      Name[0] = '_';
    }
  }
}

static std::string ReadStringFromTokenStream(const uint32_t *tokens) {
  char *charTokens = (char *)tokens;
  return std::string(charTokens);
}

static int MaskToRebaseOffset(const uint32_t mask) {
  int res = 0;
  uint32_t m = mask;
  while ((m & 1) == 0) {
    res++;
    m = m >> 1;
  }
  return res;
}

static void ExtractInputSignatures(const uint32_t *pui32Tokens,
                                ShaderInfo &shaderInfo, const int extended) {

  const uint32_t *pui32FirstSignatureToken = pui32Tokens;
  const uint32_t elementCount = *pui32Tokens++;
  /* const uint32_t ui32Key = * */ pui32Tokens++;

  shaderInfo.inputSignatures.clear();
  shaderInfo.inputSignatures.resize(elementCount);

  uint32_t i = 0;
  for (auto &currentSig : shaderInfo.inputSignatures) {
    uint32_t ui32ComponentMasks;
    uint32_t ui32SemanticNameOffset;

    currentSig.ui32Stream = 0;
    currentSig.minPrecision = MIN_PRECISION_DEFAULT;

    if (extended)
      currentSig.ui32Stream = *pui32Tokens++;

    ui32SemanticNameOffset = *pui32Tokens++;
    currentSig.semanticIndex = *pui32Tokens++;
    currentSig.systemValueType = (SPECIAL_NAME)*pui32Tokens++;
    currentSig.eComponentType = (INOUT_COMPONENT_TYPE)*pui32Tokens++;
    currentSig.ui32Register = *pui32Tokens++;

    ui32ComponentMasks = *pui32Tokens++;
    currentSig.ui32Mask = ui32ComponentMasks & 0x7F;
    // Shows which components are read
    currentSig.ui32ReadWriteMask = (ui32ComponentMasks & 0x7F00) >> 8;
    currentSig.iRebase = MaskToRebaseOffset(currentSig.ui32Mask);

    if (extended)
      currentSig.minPrecision = (MIN_PRECISION)*pui32Tokens++;

    currentSig.semanticName = ReadStringFromTokenStream(
        (const uint32_t *)((const char *)pui32FirstSignatureToken +
                           ui32SemanticNameOffset));

    currentSig.vertexAttribute = i++;
  }
}

static void ExtractOutputSignatures(const uint32_t *pui32Tokens,
                                 ShaderInfo &psShaderInfo, const int minPrec,
                                 const int streams) {
  // uint32_t i = 0;

  const uint32_t *pui32FirstSignatureToken = pui32Tokens;
  const uint32_t elementCount = *pui32Tokens++;
  /*const uint32_t ui32Key = * */ pui32Tokens++;

  psShaderInfo.outputSignatures.clear();
  psShaderInfo.outputSignatures.resize(elementCount);

  for (auto &psCurrentSignature : psShaderInfo.outputSignatures) {
    uint32_t ui32ComponentMasks;

    uint32_t ui32SemanticNameOffset;

    psCurrentSignature.ui32Stream = 0;
    psCurrentSignature.minPrecision = MIN_PRECISION_DEFAULT;

    if (streams)
      psCurrentSignature.ui32Stream = *pui32Tokens++;

    ui32SemanticNameOffset = *pui32Tokens++;
    psCurrentSignature.semanticIndex = *pui32Tokens++;
    psCurrentSignature.systemValueType = (SPECIAL_NAME)*pui32Tokens++;
    psCurrentSignature.eComponentType = (INOUT_COMPONENT_TYPE)*pui32Tokens++;
    psCurrentSignature.ui32Register = *pui32Tokens++;

    // Massage some special inputs/outputs to match the types of GLSL
    // counterparts
    if (psCurrentSignature.systemValueType == NAME_RENDER_TARGET_ARRAY_INDEX) {
      psCurrentSignature.eComponentType = INOUT_COMPONENT_SINT32;
    }

    ui32ComponentMasks = *pui32Tokens++;
    psCurrentSignature.ui32Mask = ui32ComponentMasks & 0x7F;
    // Shows which components are NEVER written.
    psCurrentSignature.ui32ReadWriteMask = (ui32ComponentMasks & 0x7F00) >> 8;
    psCurrentSignature.iRebase =
        MaskToRebaseOffset(psCurrentSignature.ui32Mask);

    if (minPrec)
      psCurrentSignature.minPrecision = (MIN_PRECISION)*pui32Tokens++;

    psCurrentSignature.semanticName = ReadStringFromTokenStream(
        (const uint32_t *)((const char *)pui32FirstSignatureToken +
                           ui32SemanticNameOffset));
  }
}

static void ExtractPatchConstantSignatures(const uint32_t *pui32Tokens,
                                        ShaderInfo &psShaderInfo,
                                        const int minPrec, const int streams) {
  uint32_t i;

  const uint32_t *pui32FirstSignatureToken = pui32Tokens;
  const uint32_t elementCount = *pui32Tokens++;
  /*const uint32_t ui32Key = * */ pui32Tokens++;

  psShaderInfo.patchConstantSignatures.clear();
  psShaderInfo.patchConstantSignatures.resize(elementCount);

  for (i = 0; i < elementCount; ++i) {
    uint32_t ui32ComponentMasks;
    ShaderInfo::InOutSignature *psCurrentSignature =
        &psShaderInfo.patchConstantSignatures[i];
    uint32_t ui32SemanticNameOffset;

    psCurrentSignature->ui32Stream = 0;
    psCurrentSignature->minPrecision = MIN_PRECISION_DEFAULT;

    if (streams)
      psCurrentSignature->ui32Stream = *pui32Tokens++;

    ui32SemanticNameOffset = *pui32Tokens++;
    psCurrentSignature->semanticIndex = *pui32Tokens++;
    psCurrentSignature->systemValueType = (SPECIAL_NAME)*pui32Tokens++;
    psCurrentSignature->eComponentType = (INOUT_COMPONENT_TYPE)*pui32Tokens++;
    psCurrentSignature->ui32Register = *pui32Tokens++;

    // Massage some special inputs/outputs to match the types of GLSL
    // counterparts
    if (psCurrentSignature->systemValueType == NAME_RENDER_TARGET_ARRAY_INDEX) {
      psCurrentSignature->eComponentType = INOUT_COMPONENT_SINT32;
    }

    ui32ComponentMasks = *pui32Tokens++;
    psCurrentSignature->ui32Mask = ui32ComponentMasks & 0x7F;
    // Shows which components are NEVER written.
    psCurrentSignature->ui32ReadWriteMask = (ui32ComponentMasks & 0x7F00) >> 8;
    psCurrentSignature->iRebase =
        MaskToRebaseOffset(psCurrentSignature->ui32Mask);

    if (minPrec)
      psCurrentSignature->minPrecision = (MIN_PRECISION)*pui32Tokens++;

    psCurrentSignature->semanticName = ReadStringFromTokenStream(
        (const uint32_t *)((const char *)pui32FirstSignatureToken +
                           ui32SemanticNameOffset));

    psCurrentSignature->vertexAttribute =
        i + psShaderInfo.inputSignatures.size();
  }
}

static const uint32_t *
ExtractResourceBinding(ShaderInfo &shaderInfo,
                    const uint32_t *pui32FirstResourceToken,
                    const uint32_t *pui32Tokens, ResourceBinding &binding) {
  uint32_t nameOffset = *pui32Tokens++;

  binding.name = ReadStringFromTokenStream(
      (const uint32_t *)((const char *)pui32FirstResourceToken + nameOffset));
  SanitizeVariableName(binding.name);

  binding.resourceType = (ResourceType)*pui32Tokens++;
  binding.resourceReturnType = (RESOURCE_RETURN_TYPE)*pui32Tokens++;
  binding.resourceDimension = (REFLECT_RESOURCE_DIMENSION)*pui32Tokens++;
  binding.numSamples =
      *pui32Tokens++; // fxc generates 2^32 - 1 for non MS images
  binding.bindPoint = *pui32Tokens++;
  binding.bindCount = *pui32Tokens++;
  binding.flags = *pui32Tokens++;
  if (((shaderInfo.ui32MajorVersion >= 5) &&
       (shaderInfo.ui32MinorVersion >= 1)) ||
      (shaderInfo.ui32MajorVersion > 5)) {
    binding.reigsterSpace = *pui32Tokens++;
    binding.rangeId = *pui32Tokens++;
  }

  shaderInfo.bindOffset += binding.bindCount;

  return pui32Tokens;
}

// Read D3D11_SHADER_TYPE_DESC
static void ExtractShaderVariableType(const uint32_t ui32MajorVersion,
                                   const uint32_t *pui32FirstConstBufToken,
                                   const uint32_t *pui32tokens,
                                   ShaderVarType *varType) {
  const uint16_t *pui16Tokens = (const uint16_t *)pui32tokens;
  uint16_t ui32MemberCount;
  uint32_t ui32MemberOffset;
  const uint32_t *pui32MemberTokens;
  uint32_t i;

  varType->Class = (SHADER_VARIABLE_CLASS)pui16Tokens[0];
  varType->Type = (SHADER_VARIABLE_TYPE)pui16Tokens[1];
  varType->Rows = pui16Tokens[2];
  varType->Columns = pui16Tokens[3];
  varType->Elements = pui16Tokens[4];

  varType->MemberCount = ui32MemberCount = pui16Tokens[5];
  varType->Members.clear();

  if (varType->ParentCount) {
    // Add empty brackets for array parents. Indices are filled in later in the
    // printing codes.
    if (varType->Parent->Elements > 1)
      varType->fullName = varType->Parent->fullName + "[]." + varType->name;
    else
      varType->fullName = varType->Parent->fullName + "." + varType->name;
  }

  if (ui32MemberCount) {
    varType->Members.resize(ui32MemberCount);

    ui32MemberOffset = pui32tokens[3];

    pui32MemberTokens =
        (const uint32_t *)((const char *)pui32FirstConstBufToken +
                           ui32MemberOffset);

    for (i = 0; i < ui32MemberCount; ++i) {
      uint32_t ui32NameOffset = *pui32MemberTokens++;
      uint32_t ui32MemberTypeOffset = *pui32MemberTokens++;

      varType->Members[i].Parent = varType;
      varType->Members[i].ParentCount = varType->ParentCount + 1;

      varType->Members[i].Offset = *pui32MemberTokens++;

      varType->Members[i].name = ReadStringFromTokenStream(
          (const uint32_t *)((const char *)pui32FirstConstBufToken +
                             ui32NameOffset));

      ExtractShaderVariableType(
          ui32MajorVersion, pui32FirstConstBufToken,
          (const uint32_t *)((const char *)pui32FirstConstBufToken +
                             ui32MemberTypeOffset),
          &varType->Members[i]);
    }
  }
}

static const uint32_t *ExtractConstantBufferDefinition(
    ShaderInfo &psShaderInfo, const uint32_t *pui32FirstConstBufToken,
    const uint32_t *pui32Tokens, ConstantBuffer &constantBuffer) {
  uint32_t i;
  uint32_t nameOffset = *pui32Tokens++;
  uint32_t numVariables = *pui32Tokens++;
  uint32_t ui32VarOffset = *pui32Tokens++;
  const uint32_t *pui32VarToken =
      (const uint32_t *)((const char *)pui32FirstConstBufToken + ui32VarOffset);

  constantBuffer.name = ReadStringFromTokenStream(
      (const uint32_t *)((const char *)pui32FirstConstBufToken + nameOffset));
  SanitizeVariableName(constantBuffer.name);

  constantBuffer.variables.clear();
  constantBuffer.variables.resize(numVariables);

  for (auto &variable : constantBuffer.variables) {
    // D3D11_SHADER_VARIABLE_DESC

    uint32_t ui32TypeOffset;
    uint32_t ui32DefaultValueOffset;

    nameOffset = *pui32VarToken++;

    variable.name = ReadStringFromTokenStream(
        (const uint32_t *)((const char *)pui32FirstConstBufToken + nameOffset));
    SanitizeVariableName(variable.name);

    variable.ui32StartOffset = *pui32VarToken++;
    variable.ui32Size = *pui32VarToken++;

    // skip ui32Flags
    pui32VarToken++;

    ui32TypeOffset = *pui32VarToken++;

    variable.type.name = variable.name;
    variable.type.fullName = variable.name;
    variable.type.Parent = 0;
    variable.type.ParentCount = 0;
    variable.type.Offset = 0;
    variable.type.m_IsUsed = false;

    ExtractShaderVariableType(
        psShaderInfo.ui32MajorVersion, pui32FirstConstBufToken,
        (const uint32_t *)((const char *)pui32FirstConstBufToken +
                           ui32TypeOffset),
        &variable.type);

    ui32DefaultValueOffset = *pui32VarToken++;

    if (psShaderInfo.ui32MajorVersion >= 5) {
      /*uint32_t StartTexture = * */ pui32VarToken++;
      /*uint32_t TextureSize = *  */ pui32VarToken++;
      /*uint32_t StartSampler = * */ pui32VarToken++;
      /*uint32_t SamplerSize = *  */ pui32VarToken++;
    }

    variable.haveDefaultValue = 0;

    if (ui32DefaultValueOffset) {
      uint32_t i = 0;
      const uint32_t ui32NumDefaultValues = variable.ui32Size / 4;
      const uint32_t *pui32DefaultValToken =
          (const uint32_t *)((const char *)pui32FirstConstBufToken +
                             ui32DefaultValueOffset);

      // Always a sequence of 4-bytes at the moment.
      // bool const becomes 0 or 0xFFFFFFFF int, int & float are 4-bytes.
      ASSERT(variable.ui32Size % 4 == 0);

      variable.haveDefaultValue = 1;

      variable.pui32DefaultValues.clear();
      variable.pui32DefaultValues.resize(variable.ui32Size / 4);

      for (i = 0; i < ui32NumDefaultValues; ++i) {
        variable.pui32DefaultValues[i] = pui32DefaultValToken[i];
      }
    }
  }

  {
    constantBuffer.ui32TotalSizeInBytes = *pui32Tokens++;

    // skip ui32Flags
    pui32Tokens++;
    // skip ui32BufferType
    pui32Tokens++;
  }

  return pui32Tokens;
}

static void ExtractResourcesDefinition(const uint32_t *pui32Tokens, // in
                          ShaderInfo &shaderInfo       // out
) {
  const uint32_t *pui32ConstantBuffers;
  const uint32_t *pui32ResourceBindings;
  const uint32_t *pui32FirstToken = pui32Tokens;

  const uint32_t numConstantBuffers = *pui32Tokens++;
  const uint32_t ui32ConstantBufferOffset = *pui32Tokens++;

  const uint32_t numResourceBindings = *pui32Tokens++;
  const uint32_t ui32ResourceBindingOffset = *pui32Tokens++;
  /*uint32_t ui32ShaderModel = * */ pui32Tokens++;
  /*uint32_t ui32CompileFlags = * */
  pui32Tokens++; // D3DCompile flags?
                 // http://msdn.microsoft.com/en-us/library/gg615083(v=vs.85).aspx

  // Resources
  pui32ResourceBindings = (const uint32_t *)((const char *)pui32FirstToken +
                                             ui32ResourceBindingOffset);

  shaderInfo.resouceBindings.clear();
  shaderInfo.resouceBindings.resize(numResourceBindings);
  // psResBindings =
  //     numResourceBindings == 0 ? NULL : &shaderInfo.resouceBindings[0];

  for (unsigned i = 0; i < numResourceBindings; ++i) {
  }

  for (auto &binding : shaderInfo.resouceBindings) {
    pui32ResourceBindings = ExtractResourceBinding(shaderInfo, pui32FirstToken,
                                                pui32ResourceBindings, binding);
    ASSERT(binding.bindPoint < MAX_RESOURCE_BINDINGS);
  }

  // Constant buffers
  pui32ConstantBuffers = (const uint32_t *)((const char *)pui32FirstToken +
                                            ui32ConstantBufferOffset);

  shaderInfo.constantBuffers.clear();
  shaderInfo.constantBuffers.resize(numConstantBuffers);

  for (auto &constantBuffer : shaderInfo.constantBuffers) {
    pui32ConstantBuffers = ExtractConstantBufferDefinition(
        shaderInfo, pui32FirstToken, pui32ConstantBuffers, constantBuffer);
  }

  // Map resource bindings to constant buffers
  if (shaderInfo.constantBuffers.size()) {
    /* HLSL allows the following:
     cbuffer A
     {...}
     cbuffer A
     {...}
     And both will be present in the assembly if used

     So we need to track which ones we matched already and throw an error if two
     buffers have the same name
    */
    std::vector<uint32_t> alreadyBound(numConstantBuffers, 0);
    for (auto &binding : shaderInfo.resouceBindings) {
      ResourceGroup eRGroup;
      uint32_t cbufIndex = 0;

      eRGroup = ShaderInfo::ResourceTypeToResourceGroup(binding.resourceType);

      // Find the constant buffer whose name matches the resource at the given
      // resource binding point
      for (cbufIndex = 0; cbufIndex < shaderInfo.constantBuffers.size();
           cbufIndex++) {
        if (shaderInfo.constantBuffers[cbufIndex].name == binding.name &&
            alreadyBound[cbufIndex] == 0) {
          shaderInfo.aui32ResourceMap[eRGroup][binding.bindPoint] = cbufIndex;
          alreadyBound[cbufIndex] = 1;
          break;
        }
      }
    }
  }
}

static const uint16_t *ReadClassType(const uint32_t *pui32FirstInterfaceToken,
                                     const uint16_t *pui16Tokens,
                                     ClassType *psClassType) {
  const uint32_t *pui32Tokens = (const uint32_t *)pui16Tokens;
  uint32_t ui32NameOffset = *pui32Tokens;
  pui16Tokens += 2;

  psClassType->ui16ID = *pui16Tokens++;
  psClassType->ui16ConstBufStride = *pui16Tokens++;
  psClassType->ui16Texture = *pui16Tokens++;
  psClassType->ui16Sampler = *pui16Tokens++;

  psClassType->name = ReadStringFromTokenStream(
      (const uint32_t *)((const char *)pui32FirstInterfaceToken +
                         ui32NameOffset));

  return pui16Tokens;
}

static const uint16_t *
ReadClassInstance(const uint32_t *pui32FirstInterfaceToken,
                  const uint16_t *pui16Tokens, ClassInstance *psClassInstance) {
  uint32_t ui32NameOffset = *pui16Tokens++ << 16;
  ui32NameOffset |= *pui16Tokens++;

  psClassInstance->ui16ID = *pui16Tokens++;
  psClassInstance->ui16ConstBuf = *pui16Tokens++;
  psClassInstance->ui16ConstBufOffset = *pui16Tokens++;
  psClassInstance->ui16Texture = *pui16Tokens++;
  psClassInstance->ui16Sampler = *pui16Tokens++;

  psClassInstance->name = ReadStringFromTokenStream(
      (const uint32_t *)((const char *)pui32FirstInterfaceToken +
                         ui32NameOffset));

  return pui16Tokens;
}

static void ReadInterfaces(const uint32_t *pui32Tokens,
                           ShaderInfo &psShaderInfo) {
  uint32_t i;
  uint32_t ui32StartSlot;
  const uint32_t *pui32FirstInterfaceToken = pui32Tokens;
  const uint32_t ui32ClassInstanceCount = *pui32Tokens++;
  const uint32_t ui32ClassTypeCount = *pui32Tokens++;
  const uint32_t ui32InterfaceSlotRecordCount = *pui32Tokens++;
  /*const uint32_t ui32InterfaceSlotCount = * */ pui32Tokens++;
  const uint32_t ui32ClassInstanceOffset = *pui32Tokens++;
  const uint32_t ui32ClassTypeOffset = *pui32Tokens++;
  const uint32_t ui32InterfaceSlotOffset = *pui32Tokens++;

  const uint16_t *pui16ClassTypes =
      (const uint16_t *)((const char *)pui32FirstInterfaceToken +
                         ui32ClassTypeOffset);
  const uint16_t *pui16ClassInstances =
      (const uint16_t *)((const char *)pui32FirstInterfaceToken +
                         ui32ClassInstanceOffset);
  const uint32_t *pui32InterfaceSlots =
      (const uint32_t *)((const char *)pui32FirstInterfaceToken +
                         ui32InterfaceSlotOffset);

  const uint32_t *pui32InterfaceSlotTokens = pui32InterfaceSlots;

  ClassType *psClassTypes;
  ClassInstance *psClassInstances;

  psShaderInfo.psClassTypes.clear();
  psShaderInfo.psClassTypes.resize(ui32ClassTypeCount);
  psClassTypes = &psShaderInfo.psClassTypes[0];

  for (i = 0; i < ui32ClassTypeCount; ++i) {
    pui16ClassTypes = ReadClassType(pui32FirstInterfaceToken, pui16ClassTypes,
                                    psClassTypes + i);
    psClassTypes[i].ui16ID = (uint16_t)i;
  }

  psShaderInfo.psClassInstances.clear();
  psShaderInfo.psClassInstances.resize(ui32ClassInstanceCount);
  psClassInstances = &psShaderInfo.psClassInstances[0];

  for (i = 0; i < ui32ClassInstanceCount; ++i) {
    pui16ClassInstances = ReadClassInstance(
        pui32FirstInterfaceToken, pui16ClassInstances, psClassInstances + i);
  }

  // Slots map function table to $ThisPointer cbuffer variable index
  ui32StartSlot = 0;
  for (i = 0; i < ui32InterfaceSlotRecordCount; ++i) {
    uint32_t k;

    const uint32_t ui32SlotSpan = *pui32InterfaceSlotTokens++;
    const uint32_t ui32Count = *pui32InterfaceSlotTokens++;
    const uint32_t ui32TypeIDOffset = *pui32InterfaceSlotTokens++;
    const uint32_t ui32TableIDOffset = *pui32InterfaceSlotTokens++;

    const uint16_t *pui16TypeID =
        (const uint16_t *)((const char *)pui32FirstInterfaceToken +
                           ui32TypeIDOffset);
    const uint32_t *pui32TableID =
        (const uint32_t *)((const char *)pui32FirstInterfaceToken +
                           ui32TableIDOffset);

    for (k = 0; k < ui32Count; ++k) {
      psShaderInfo.aui32TableIDToTypeID[*pui32TableID++] = *pui16TypeID++;
    }

    ui32StartSlot += ui32SlotSpan;
  }
}

void LoadShaderInfo(const uint32_t ui32MajorVersion,
                    const uint32_t ui32MinorVersion,
                    const ReflectionChunks *psChunks, ShaderInfo &shaderInfo) {
  const uint32_t *pui32Inputs = psChunks->pui32Inputs;
  const uint32_t *pui32Inputs11 = psChunks->pui32Inputs11;
  const uint32_t *pui32Resources = psChunks->pui32Resources;
  const uint32_t *pui32Interfaces = psChunks->pui32Interfaces;
  const uint32_t *pui32Outputs = psChunks->pui32Outputs;
  const uint32_t *pui32Outputs11 = psChunks->pui32Outputs11;
  const uint32_t *pui32OutputsWithStreams = psChunks->pui32OutputsWithStreams;
  const uint32_t *pui32PatchConstants = psChunks->pui32PatchConstants;
  const uint32_t *pui32PatchConstants11 = psChunks->pui32PatchConstants11;

  shaderInfo.eTessOutPrim = TESSELLATOR_OUTPUT_UNDEFINED;
  shaderInfo.eTessPartitioning = TESSELLATOR_PARTITIONING_UNDEFINED;
  shaderInfo.ui32TessInputControlPointCount = 0;
  shaderInfo.ui32TessOutputControlPointCount = 0;
  shaderInfo.eTessDomain = TESSELLATOR_DOMAIN_UNDEFINED;
  shaderInfo.bEarlyFragmentTests = false;

  shaderInfo.ui32MajorVersion = ui32MajorVersion;
  shaderInfo.ui32MinorVersion = ui32MinorVersion;

  if (pui32Inputs)
    ExtractInputSignatures(pui32Inputs, shaderInfo, 0);
  if (pui32Inputs11)
    ExtractInputSignatures(pui32Inputs11, shaderInfo, 1);
  if (pui32Resources)
    ExtractResourcesDefinition(pui32Resources, shaderInfo);
  if (pui32Interfaces)
    ReadInterfaces(pui32Interfaces, shaderInfo);
  if (pui32Outputs)
    ExtractOutputSignatures(pui32Outputs, shaderInfo, 0, 0);
  if (pui32Outputs11)
    ExtractOutputSignatures(pui32Outputs11, shaderInfo, 1, 1);
  if (pui32OutputsWithStreams)
    ExtractOutputSignatures(pui32OutputsWithStreams, shaderInfo, 0, 1);
  if (pui32PatchConstants)
    ExtractPatchConstantSignatures(pui32PatchConstants, shaderInfo, 0, 0);
  if (pui32PatchConstants11)
    ExtractPatchConstantSignatures(pui32PatchConstants11, shaderInfo, 1, 1);

  {
    uint32_t i;
    for (i = 0; i < shaderInfo.constantBuffers.size(); ++i) {
      if (shaderInfo.constantBuffers[i].name == "$ThisPointer") {
        shaderInfo.psThisPointerConstBuffer = &shaderInfo.constantBuffers[i];
      }
    }
  }
}
