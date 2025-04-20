#define ASM_FORWARD(name, offset)                                              \
  __attribute__((naked, dllexport)) void name(void) {                          \
    asm("movq    .refptr.__wine_unixlib_handle(%rip), %rax\n"                  \
        "movq    (%rax), %rax\n"                                               \
        "addq    $" #offset "* 8 , %rax\n"                                         \
        "jmpq    *(%rax)\n\t");                                                \
  };

ASM_FORWARD(SM50GetCompiledBitcode, 77)
ASM_FORWARD(SM50DestroyBitcode, 78)
ASM_FORWARD(SM50GetErrorMesssage, 79)
ASM_FORWARD(SM50FreeError, 80)
ASM_FORWARD(SM50CompileGeometryPipelineVertex, 81)
ASM_FORWARD(SM50CompileGeometryPipelineGeometry, 82)
ASM_FORWARD(SM50CompileTessellationPipelineVertex, 83)
ASM_FORWARD(SM50CompileTessellationPipelineHull, 84)
ASM_FORWARD(SM50CompileTessellationPipelineDomain, 85)
extern void *__wine_unixlib_handle;