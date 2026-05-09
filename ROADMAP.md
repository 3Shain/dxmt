# D3D12-Metal Roadmap (DXMT fork)

## Goal
Get RE4 (Resident Evil 4, AppID 2050650) running on macOS via MetalSharp Wine with a custom D3D12→Metal translation layer.

## Current Status: DXIL Shaders Compile! Investigating Game Exit

RE4's DXIL shaders now successfully compile to Metal via Apple's `metal-shaderconverter.exe` (Windows version, v3.0.6) running inside Wine. Both compute PSOs compile and load as WMT functions. Game exits shortly after PSO creation — investigating why.

### Shader Compilation Pipeline (WORKING)
- DXBC container → CDXBCParser finds DXIL blob → write to temp file
- `metal-shaderconverter.exe` (Wine) → metallib output
- Parse reflection JSON for real entry point names
- WMT `newLibrary(metallib)` → `newFunction(entryPoint)`
- Result: `CS_FastClear` (4176 bytes), `CS_ZeroFill` (4108 bytes)

### Current Issues
- Game exits/crashes after PSO creation (no error in our trace)
- Only 2 PSOs created (was 4 before — may be a timing issue)
- Command lists still only contain SetPipelineState commands
- No root constants or dispatch calls recorded

## Commits on `feat/d3d12-metal`
1. `ba00619` - root signature serialization + command signature stubs
2. `5370a6f` - fix rendering pipeline - resource registry, persistent cmd queue, swapchain
3. `f246d2b` - lazy texture creation + reserved resource fallback
4. `02a0eaf` - swapchain creation + multi-backbuffer + fence fix - PINK SCREEN!
5. `eee259a` - inherit ID3D12GraphicsCommandList2 + stub methods
6. `73b16d0` - diagnostic tracing - DXBC chunk inspection reveals DXIL blocker
7. `cb4faac` - DXIL compilation via Apple metal-shaderconverter - SHADERS COMPILE!

## What's Working
- D3D12 device creation + QI chains
- Command queue with persistent WMT command queue
- Command list record + replay (all command types)
- Swapchain creation + multi-backbuffer (4 buffers) + Present
- **DXIL→Metal shader compilation via Apple metal-shaderconverter**
- **PSO creation with real Metal compute functions (CS_FastClear, CS_ZeroFill)**
- Feature support queries (OPTIONS1-11, shader model 6.5, etc.)
- Root signature creation + serialization
- PSO creation (graphics + compute) — structure works, compilation fails
- Resource creation (committed + reserved fallback)
- Descriptor heap + SRV/CBV/UAV/RTV/DSV creation
- Shader resource view creation
- Command replay loop processes: root constants, PSO sets, root signatures, dispatch, copy, vertex/index buffers, viewports, render targets, clears, resource barriers, descriptor heaps
- ExecuteBundle tracing + command merge

## What's NOT Working
- **Game exits after PSO creation** — investigating crash/exit reason
- Command lists only have SetPipelineState, no dispatch/root constants
- Root CBV binding incomplete (LookupResourceByGPUAddress TODOs)
- setVertexBytes not in WMT API (only setFragmentBytes)
- No audio (media foundation / XAudio2)

---

## Phase 1: DXIL Shader Compilation [DONE]

### Approach: Apple metal-shaderconverter (Windows exe in Wine)
Uses Apple's official DXIL→Metal converter rather than writing our own compiler.

### Pipeline
1. SM50Initialize fails → detect DXIL chunk via CDXBCParser
2. Write full DXBC container to temp file
3. Run `metal-shaderconverter.exe` (v3.0.6) inside Wine via `_spawnlp`
4. Parse reflection JSON for entry point name (CS_FastClear, CS_ZeroFill, etc.)
5. Load metallib via WMT `newLibrary` → `newFunction(entryPoint)`

### Files Added
- `src/airconv/dxil/dxil_container.hpp/cpp` — DXIL blob parser
- `src/airconv/dxil/llvm_bitcode.hpp/cpp` — LLVM bitcode reader (future use)
- `src/airconv/dxil/dxil_to_msl.hpp/cpp` — DXIL→MSL stub (future use)

### Dependencies
- `metal-shaderconverter.exe` installed to `C:\windows\system32\` in Wine prefix
- `metalirconverter.dll` in same location
- Downloaded from Apple Developer: Metal Shader Converter for Windows 3.0

---

## Phase 2: Rendering Pipeline [NEXT]
- Investigate game exit after PSO creation
- Root signature binding via top-level Argument Buffer
- Implement resource encoding per Metal Shader Converter docs
- Command list replay with real Metal functions
- Debug visual output

## Phase 3: Polish
- Per-game DLL routing via mscompatdb.so
- Performance tuning
- Support other DX12 games

---

## Key Files
- `/tmp/dxmt-src/` — source tree
- `~/Desktop/untitled folder/dxmt-d3d12-metal/` — persistent backup
- `src/d3d12/d3d12_pipeline_state.cpp` — PSO Compile(), CompileShader()
- `src/airconv/` — existing DXBC→Metal compiler (SM5.0 only)
- `src/airconv/metallib_writer.cpp` — metallib binary writer
- `src/airconv/dxbc_converter.cpp` — DXBC IR→Metal conversion reference

## Launch Commands
```bash
# Kill everything
killall -9 wineserver steam.exe steamwebhelper.exe steamservice.exe re4.exe

# Launch Steam
WINEPREFIX=~/.metalsharp/prefix-steam WINEDEBUG=-all \
  nohup ~/.metalsharp/runtime/wine/bin/wine \
  "$HOME/.metalsharp/prefix-steam/drive_c/Program Files (x86)/Steam/steam.exe" \
  -no-cef-sandbox --disable-gpu -console &

# Launch RE4 directly
WINEPREFIX=~/.metalsharp/prefix-steam WINEDEBUG=-all \
  nohup ~/.metalsharp/runtime/wine/bin/wine \
  "$HOME/.metalsharp/prefix-steam/drive_c/Program Files (x86)/Steam/steamapps/common/RESIDENT EVIL 4  BIOHAZARD RE4/re4.exe" &
```

## Deploy Targets (ALL must be updated)
- `~/.metalsharp/prefix-steam/drive_c/windows/system32/d3d12.dll`
- `~/.metalsharp/runtime/wine/lib/wine/x86_64-windows/d3d12.dll`
- `~/.metalsharp/prefix-steam/drive_c/Program Files (x86)/Steam/steamapps/common/RESIDENT EVIL 4  BIOHAZARD RE4/d3d12.dll`

## RE4 Shader Profile
- Container: DXBC (magic 0x43425844)
- Shader format: DXIL (LLVM bitcode inside DXBC)
- Shader model: SM6.x
- Chunks: SFI0, ISG1, OSG1, PSV0, STAT, ILDN, HASH, DXIL
- Only compute PSOs created (4 total, 2 unique sizes: 3124 and 3504 bytes)
- Game uses compute-first rendering (88 dispatches/frame, 0 draw calls)
