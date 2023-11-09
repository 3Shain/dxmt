#pragma once

#include <stdint.h>
#include <string>
#include <set>
#include "cbstring/bstrlib.h"

class Shader;
class CrossShaderDependency;
class ShaderPhase;
class Translator;
class Operand;
class HLSLccReflection;

class HLSLCrossCompilerContext {
public:
  HLSLCrossCompilerContext(HLSLccReflection &refl)
      : glsl(nullptr), beforeMain(nullptr), currentShaderString(nullptr),
        currentPhase(0), indent(0), flags(0), psShader(nullptr),
        dependencies(nullptr), inputPrefix(nullptr), outputPrefix(nullptr),
        psTranslator(nullptr), m_Reflection(refl) {}

  bstring glsl;
  bstring beforeMain;

  bstring *currentShaderString; // either glsl or earlyMain of current phase

  uint32_t currentPhase;

  int indent;
  unsigned int flags;

  Shader *psShader;
  CrossShaderDependency *dependencies;
  const char *inputPrefix;  // Prefix for shader inputs
  const char *outputPrefix; // Prefix for shader outputs

  void DoDataTypeAnalysis(ShaderPhase *psPhase);

  void ClearDependencyData();

  void AddIndentation();

  // Currently active translator
  Translator *psTranslator;

  HLSLccReflection &m_Reflection; // Callbacks for bindings and diagnostic info

  // Retrieve the name for which the input or output is declared as. Takes into
  // account possible redirections.
  std::string GetDeclaredInputName(const Operand *psOperand, int *piRebase,
                                   int iIgnoreRedirect,
                                   uint32_t *puiIgnoreSwizzle) const;
  std::string GetDeclaredOutputName(const Operand *psOperand, int *stream,
                                    uint32_t *puiIgnoreSwizzle, int *piRebase,
                                    int iIgnoreRedirect) const;

  bool OutputNeedsDeclaring(const Operand *psOperand, const int count);

private:
  std::set<std::string> m_EnabledExtensions;
};
