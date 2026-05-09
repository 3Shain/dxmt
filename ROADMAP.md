# D3D12-Metal Roadmap (DXMT fork)

## Goal
Get RE4 (Resident Evil 4, AppID 2050650) running on macOS via MetalSharp Wine with a custom D3D12→Metal translation layer.

## Current Status: BLOCKED on DXIL Shader Compilation

RE4 reaches its main loop, creates swapchain, presents frames (pink pulsating screen), but **all shader compilation fails** because RE4 uses DXIL (Shader Model 6.x) shaders inside DXBC containers. The existing SM50 compiler only handles SM5.0 DXBC shaders.

### Evidence
- DXBC containers contain `DXIL` chunks (not `SHDR`/`SHEX`)
- Chunk tags found: `SFI0`, `ISG1`, `OSG1`, `PSV0`, `STAT`, `ILDN`, `HASH`, `DXIL`
- SM50Initialize fails: "Invalid DXBC bytecode: shader blob not found"
- All 4 compute PSOs fail compilation
- Game dispatches 88+ compute workloads per frame with null Metal functions
- Zero draw calls (game uses compute-first rendering path)

## Commits on `feat/d3d12-metal`
1. `ba00619` - root signature serialization + command signature stubs
2. `5370a6f` - fix rendering pipeline - resource registry, persistent cmd queue, swapchain
3. `f246d2b` - lazy texture creation + reserved resource fallback
4. `02a0eaf` - swapchain creation + multi-backbuffer + fence fix - PINK SCREEN!
5. `eee259a` - inherit ID3D12GraphicsCommandList2 + stub methods
6. `73b16d0` - diagnostic tracing - DXBC chunk inspection reveals DXIL blocker

## What's Working
- D3D12 device creation + QI chains
- Command queue with persistent WMT command queue
- Command list record + replay (all command types)
- Swapchain creation + multi-backbuffer (4 buffers) + Present
- Fence signal + SetEventOnCompletion (non-blocking)
- Feature support queries (OPTIONS1-11, shader model 6.5, etc.)
- Root signature creation + serialization
- PSO creation (graphics + compute) — structure works, compilation fails
- Resource creation (committed + reserved fallback)
- Descriptor heap + SRV/CBV/UAV/RTV/DSV creation
- Shader resource view creation
- Command replay loop processes: root constants, PSO sets, root signatures, dispatch, copy, vertex/index buffers, viewports, render targets, clears, resource barriers, descriptor heaps
- ExecuteBundle tracing + command merge

## What's NOT Working
- **DXIL shader compilation** — CORE BLOCKER
- No draw calls issued (game uses compute-first path, gated on PSO compilation)
- Root CBV binding incomplete (LookupResourceByGPUAddress TODOs)
- setVertexBytes not in WMT API (only setFragmentBytes)
- No audio (media foundation / XAudio2)

---

## Phase 1: DXIL→Metal Shader Compiler [CURRENT]

### Approach: Write a DXIL→AIR converter
Convert DXIL bytecode to Apple AIR (Assembly Intermediate Representation) / Metallib format.

### Sub-tasks
1. **DXIL container parser** — extract DXIL blob from DXBC container
   - Parse DXBC header (magic, hash, chunk offsets)
   - Find `DXIL` chunk by tag
   - Extract the DXIL program blob (LLVM bitcode)
2. **DXIL bitcode reader** — parse LLVM bitcode into IR
   - LLVM bitcode format: bitstream blocks, records, abbreviation system
   - Extract function bodies, type table, constant pool, value symbol table
   - Map DXIL opcodes to semantics (load, store, call, compute/texture intrinsics)
3. **DXIL intrinsic mapping** — map DXIL ops to Metal equivalents
   - `dx.op.threadId` → thread position in grid
   - `dx.op.bufferLoad` → buffer read
   - `dx.op.textureStore` → texture write
   - `dx.op.createHandle` → resource binding
   - `dx.op.barrier` → threadgroup barrier
   - etc.
4. **Metal AIR/Metallib writer** — emit Metal shader bytecode
   - Use existing `metallib_writer.cpp` in DXMT airconv as reference
   - Or generate MSL source and compile via `wmt_device.newLibrary(source)`
5. **Integration** — wire into `MTLD3D12PipelineState::CompileShader()`
   - Detect DXIL vs DXBC by checking chunk tags
   - Route to DXIL compiler path when DXIL chunk found

### Alternative: DXIL→MSL source (faster to prototype)
Instead of writing AIR directly, generate MSL (Metal Shading Language) source code from DXIL IR, then compile via WMT `newLibrary(source)`. This is easier to debug and good enough for a first pass.

---

## Phase 2: Rendering (after shaders compile)
- Verify compute dispatches execute with real Metal functions
- Implement root CBV binding (GPU address → Metal buffer lookup)
- Add setVertexBytes to WMT API
- Debug visual output
- Audio (stretch goal)

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
