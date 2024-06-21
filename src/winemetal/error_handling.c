#include <stdio.h>
#include <windows.h>
#include <assert.h>
#include <dbghelp.h>

// copied from llvm project
void PrintStackTraceForThread(HANDLE hProcess, HANDLE hThread,
                              STACKFRAME64 *StackFrame, CONTEXT *Context) {
  // Initialize the symbol handler.
  SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
  SymInitialize(hProcess, NULL, TRUE);

  printf("Current Thread Id: 0x%lx\n", GetCurrentThreadId());
  printf("Context RIP: 0x%llx\n", Context->Rip);

  while (TRUE) {
    if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, hProcess, hThread, StackFrame,
                     Context, 0, SymFunctionTableAccess64, SymGetModuleBase64,
                     0)) {
      break;
    }

    if (StackFrame->AddrFrame.Offset == 0) {
      break;
    }

    // Print the PC in hexadecimal.
    DWORD64 PC = StackFrame->AddrPC.Offset;

    printf("0x%llx\n", PC);

    // Verify the PC belongs to a module in this process.
    if (!SymGetModuleBase64(hProcess, PC)) {
      printf(" <unknown module>\n");
      continue;
    }

    IMAGEHLP_MODULE64 M;
    memset(&M, 0, sizeof(IMAGEHLP_MODULE64));
    M.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
    if (SymGetModuleInfo64(hProcess, SymGetModuleBase64(hProcess, PC), &M)) {
      DWORD64 const disp = PC - M.BaseOfImage;
      printf(", %s(0x%llx) + 0x%llx byte(s)", M.ImageName, M.BaseOfImage,
             (disp));
    } else {
      printf(", <unknown module>");
    }

    // Print the symbol name.
    char buffer[512];
    IMAGEHLP_SYMBOL64 *symbol = buffer;
    memset(symbol, 0, sizeof(IMAGEHLP_SYMBOL64));
    symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
    symbol->MaxNameLength = 512 - sizeof(IMAGEHLP_SYMBOL64);

    DWORD64 dwDisp;
    if (!SymGetSymFromAddr64(hProcess, PC, &dwDisp, symbol)) {
      printf("\n");
      continue;
    }

    buffer[511] = 0;
    printf(", %s() + 0x%llx byte(s)", (symbol->Name), (dwDisp));

    // Print the source file and line number information.
    IMAGEHLP_LINE64 line = {};
    DWORD dwLineDisp;
    line.SizeOfStruct = sizeof(line);
    if (SymGetLineFromAddr64(hProcess, PC, &dwLineDisp, &line)) {
      printf(", %s, line %ld + 0x%lx byte(s)", line.FileName, line.LineNumber,
             dwLineDisp);
    }

    printf("\n");
  }
}

struct E_CONTEXT {
  CONTEXT ctx;
  BOOL is_in_scope;
};

typedef struct E_CONTEXT *PE_CONTEXT;

DWORD dwTlsIndex;
PVOID pVEH;

void print_stack(PCONTEXT C) {
  STACKFRAME64 StackFrame;
  StackFrame.AddrPC.Offset = C->Rip;
  StackFrame.AddrStack.Offset = C->Rsp;
  StackFrame.AddrFrame.Offset = C->Rbp;
  StackFrame.AddrPC.Mode = AddrModeFlat;
  StackFrame.AddrStack.Mode = AddrModeFlat;
  StackFrame.AddrFrame.Mode = AddrModeFlat;
  PrintStackTraceForThread(GetCurrentProcess(), GetCurrentThread(), &StackFrame,
                           C);
}

LONG CALLBACK ExceptionHandler(PEXCEPTION_POINTERS ExceptionInfo) {
  switch (ExceptionInfo->ExceptionRecord->ExceptionCode) {
  case DBG_PRINTEXCEPTION_C:
  case 0x4001000AL: // DbgPrintExceptionWideC
  case 0x406D1388:  // set debugger thread name
    return EXCEPTION_CONTINUE_EXECUTION;
  }

  PE_CONTEXT data = (PE_CONTEXT)TlsGetValue(dwTlsIndex);
  if (!data || !data->is_in_scope) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  printf("native error at call boundary 0%llx\n", data->ctx.Rip);
  // print_stack(&data->ctx);

  ExceptionInfo->ExceptionRecord->ExceptionAddress = (PVOID)data->ctx.Rip;
  //   ExceptionInfo->ContextRecord->Rip = data->ctx.Rip;
  //   ExceptionInfo->ContextRecord->Rsp = data->ctx.Rsp;
  //   ExceptionInfo->ContextRecord->Rbp = data->ctx.Rbp;
  //   ExceptionInfo->ContextRecord->Rcx = data->ctx.Rcx;
  //   ExceptionInfo->ContextRecord->Rdx = data->ctx.Rdx;
  //   ExceptionInfo->ContextRecord->R8 = data->ctx.R8;
  //   ExceptionInfo->ContextRecord->R9 = data->ctx.R9;

  // let wine find the "corrected" stackframe
  memcpy(ExceptionInfo->ContextRecord, &data->ctx, sizeof(CONTEXT));

  return EXCEPTION_CONTINUE_SEARCH;
}

void initialize_veh() {
  if ((dwTlsIndex = TlsAlloc()) == TLS_OUT_OF_INDEXES)
    assert(0 && "fail to initialize tls");
  pVEH = AddVectoredExceptionHandler(1, ExceptionHandler);
  TlsSetValue(dwTlsIndex, 0);
};

void cleanup_veh() {
  RemoveVectoredExceptionHandler(pVEH);
  TlsFree(dwTlsIndex);
};

__attribute__((dllexport)) void ____enter_scope(PCONTEXT C) {
  PE_CONTEXT data = (PE_CONTEXT)TlsGetValue(dwTlsIndex);

  // if((C->Rip & 0xD78E) == 0xd78e) {
  // __builtin_trap();
  // }

  if (!data) {
    data = (PE_CONTEXT)LocalAlloc(LPTR, sizeof(struct E_CONTEXT));
    data->is_in_scope = FALSE;
    TlsSetValue(dwTlsIndex, data);
  }
  if (data->is_in_scope) {
    return;
  }
  memcpy(&data->ctx, C, sizeof(CONTEXT));
  data->is_in_scope = TRUE;
}

__attribute__((dllexport)) void ____exit_scope() {
  PE_CONTEXT data = (PE_CONTEXT)TlsGetValue(dwTlsIndex);
  if (!data) {
    return;
  }
  if (!data->is_in_scope) {
    return;
  }
  data->is_in_scope = FALSE;
}