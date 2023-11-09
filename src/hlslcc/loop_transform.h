#pragma once

class ShaderPhase;
class HLSLCrossCompilerContext;
namespace dxmt {
void DoLoopTransform(HLSLCrossCompilerContext *psContext, ShaderPhase &phase);
}
