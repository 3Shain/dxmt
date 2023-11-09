#ifndef DECODE_H
#define DECODE_H

#include "shader.h"

Shader *DecodeDXBC(uint32_t *data);

void UpdateOperandReferences(Shader *psShader,
                             SHADER_PHASE_TYPE eShaderPhaseType,
                             Instruction *psInst);

#endif
