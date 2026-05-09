# D3D12-Metal Roadmap (DXMT fork)

## Goal
Get RE4 (Resident Evil 4, AppID 2050650) running on macOS via MetalSharp Wine with a custom D3D12→Metal translation layer.

## Current Status: Compute Pipeline Working, Graphics Pipeline Needed

RE4's compute shaders compile and execute correctly. Descriptor resolution works. Command buffers complete (status=4). Game shows ~35 frames of loading screen (pink = uninitialized/cleared RT, expected). Game exits after init when it tries to enter the main rendering loop.

### What's Working
- D3D12 device creation + ID3D12Device1 QueryInterface
- DXIL→Metal shader compilation via macOS metal-shaderconverter (v3.1.1)
- Compute PSO creation (CS_FastClear, CS_ZeroFill) with correct threadgroup sizes (256x1x1)
- Descriptor heap + stride fix (sizeof(D3D12Descriptor) instead of hardcoded 64)
- Descriptor table resolution: GPU handles → D3D12Descriptor → Metal resources ✓
- Command list record + replay with all command types
- Swapchain creation + multi-backbuffer (4 buffers) + Present with blit
- Fence signaling with MTLSharedEvent
- Feature support queries (OPTIONS1-12, shader model 6.5, etc.)
- Root signature creation + serialization
- Resource creation (committed + reserved fallback)
- CBV/SRV/UAV/RTV/DSV creation
- MinGW cross-compilation (build-win64.txt + LLVM 15)

### What's NOT Working
- **RE4 exits after ~35 frames** — game finishes init, can't enter graphics rendering
- **No graphics PSOs created** — game creates graphics root signatures but dies before CreateGraphicsPipelineState
- **ID3D12Device2-9 not implemented** — RE4 queries all of them, gets E_NOINTERFACE
- No audio

---

## Phase 1: Complete Device Interface Chain + Missing APIs [IN PROGRESS]

RE4 queries ID3D12Device1-9. Currently only Device1 works. Need to implement Device2-9 stubs so the game doesn't bail when it can't get a newer device interface.

### 1a. Add ID3D12Device2-9 GUIDs to d3d12.h
The MinGW headers only define up to ID3D12Device1. Need to add GUIDs for Device2-9 so QueryInterface can match them. These are well-known GUIDs from Microsoft's d3d12.h SDK headers.

### 1b. Make MTLD3D12Device inherit ID3D12Device9
Inherit the full chain. Stub all new virtual methods. Key methods to actually implement (non-stub):
- Device2: `CreatePipelineState` (stream-based desc) — may be how RE4 creates graphics PSOs
- Device4: `CreateCommandList1` — creates command lists without PSO arg
- Device4: `CreateCommittedResource1`, `CreatePlacedResource1` — with heap flags
- Device8: `CreateCommittedResource2` — with enhanced desc

### 1c. Add missing command list interfaces
RE4 may query ID3D12GraphicsCommandList1-6. Check and add stubs.

### 1d. CopyDescriptors fix
Currently broken — divides increment by sizeof(D3D12Descriptor) giving 0. Fix to use direct pointer arithmetic.

---

## Phase 2: Graphics Pipeline

Once RE4 stays alive past init, it will try to create graphics PSOs with VS/PS shaders.

### 2a. Graphics PSO compilation
- CompileShader already handles VS/PS via metal-shaderconverter
- Need to create WMT RenderPipelineState with vertex + fragment functions
- Wire up blend state, rasterizer state, depth stencil, RTV formats, topology

### 2b. Render pass encoding
- OMSetRenderTargets → open render encoder with correct attachments
- ClearRenderTargetView → render encoder clear
- ClearDepthStencilView → render encoder clear
- SetPipelineState → render encoder setRenderPipelineState

### 2c. Draw call replay
- DrawInstanced → WMT render draw command
- DrawIndexedInstanced → WMT render draw indexed command
- IASetVertexBuffers → set vertex buffer offsets/strides
- IASetIndexBuffer → set index buffer
- IASetPrimitiveTopology → triangle/line/point list

### 2d. Root signature binding for graphics
- Graphics root constants → setVertexBytes / setFragmentBytes
- Graphics root CBVs → setVertexBuffer / setFragmentBuffer
- Graphics root descriptor tables → resolve descriptors, bind textures/buffers

---

## Phase 3: Polish
- Per-game DLL routing
- Performance tuning (remove waitUntilCompleted, use async fence)
- Support other DX12 games
- Shader cache warmup (pre-compile all shaders from game data)

---

## Build & Deploy

### Build (MinGW cross-compile)
```bash
cd /tmp/dxmt-src
rm -f build/src/d3d12/d3d12.dll
ninja -C build src/d3d12/d3d12.dll
```

### Deploy
```bash
cp build/src/d3d12/d3d12.dll ~/.metalsharp/runtime/wine/lib/wine/x86_64-windows/d3d12.dll
cp build/src/d3d12/d3d12.dll ~/.metalsharp/prefix-steam/drive_c/windows/system32/d3d12.dll
```

### Launch RE4
```bash
# Kill everything first
kill -9 $(ps aux | grep -iE 'wine|steam|re4|cef|winedevice|steamservice|steamwebhelper' | grep -v grep | grep -v ipcserver | awk '{print $2}')

# Launch
rm -f /tmp/dxmt_dxgi_trace.log
WINEPREFIX=~/.metalsharp/prefix-steam WINEDEBUG=-all \
  nohup ~/.metalsharp/runtime/wine/bin/wine \
  ~/.metalsharp/prefix-steam/drive_c/Program\ Files\ \(x86\)/Steam/steamapps/common/RESIDENT\ EVIL\ 4\ \ BIOHAZARD\ RE4/re4.exe &>/dev/null &
```

### Key Paths
- Source: `/tmp/dxmt-src/` → symlinks to `/Volumes/AverySSD/metalsharp/dxmt-src/`
- Shader cache: `/tmp/dxmt_shader_cache/`
- Trace file: `/tmp/dxmt_dxgi_trace.log`
- Cross file: `build-win64.txt`
- LLVM 15: `/opt/homebrew/opt/llvm@15`
- Fake winebuild: `~/.metalsharp/runtime/wine/bin/winebuild`

### RE4 Command Profile (per frame during init)
- 8 Dispatch (compute only)
- 1 SetGraphicsRoot32BitConstants
- 14 SetGraphicsRootDescriptorTable
- 1 OMSetRenderTargets
- 1 OMSetStencilRef
- 4 ClearDepthStencilView
- 1 ClearRenderTargetView
- 2 SetPipelineState
- ResourceBarrier (no-op)

### Device Interface GUIDs (from Microsoft d3d12.h)
```
ID3D12Device  = 189819f1-1db6-4b57-be54-1821339b85f7
ID3D12Device1 = 77acce80-638e-4e65-8895-c1f23386863e
ID3D12Device2 = 30baa41e-b15b-475c-a0bb-1af5c5b64328  ← RE4 queries this
ID3D12Device3 = 81dadc15-2bad-4392-93c5-101345c4aa98
ID3D12Device4 = e865df17-a9ee-46f9-a463-3098315aa2e5
ID3D12Device5 = 8b4f173b-2fea-4b80-8f58-4307191ab95d
ID3D12Device6 = c70b221b-40e4-4a17-89af-025a0727a6dc
ID3D12Device7 = 9218e6bb-f944-4f7e-a75c-b1b2c7b701f3
ID3D12Device8 = 9b7e4c0f-342c-4106-a19f-4f2704f689f0
ID3D12Device9 = 4c80e962-f032-4f60-bc9e-ebc2cfa1d83c  (this is actually Device10)
ID3D12Device10= 74eaee3f-2f4b-476d-82ba-2b85cb49e310
```

Wait — the GUIDs RE4 queries don't all match standard Device2-10 GUIDs. Let me verify against the trace:
- 74eaee3f = ID3D12Device9 or Device10
- 4c80e962 = ID3D12Device8 or Device9
- 9218e6bb = ID3D12Device7
- c70b221b = ID3D12Device6
- 8b4f173b = ID3D12Device5
- e865df17 = ID3D12Device4
- 81dadc15 = ID3D12Device3
- 30baa41e = ID3D12Device2
- db6f6ddb = maybe ID3D12Device11 or ID3D12Device12
- 9b7e4c0f = ID3D12Device8
- 54ec77fa = unknown
