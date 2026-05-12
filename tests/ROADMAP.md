# AFMT D3D12 Conformance Roadmap

## Phase 0: Build & Deploy AFMT DLLs — COMPLETE
- 64-bit and 32-bit AFMT DLLs built and deployed
- Verified: Rain World, Schedule 1, Nidhogg 2, Undertale, Goat Simulator

## Phase 1: Engine Variants + Game Routing — COMPLETE
- `Engine::AfmtMetal` and `Engine::AfmtD3D9` added to `launch.rs`
- Games routed, backend deployed

## Phase 2: D3D12 Conformance Probes — COMPLETE

### Probe 2: Compute PSO — PASS (14/14)
- Device creation, QI through ID3D12Device12
- Command queue/allocator/list, root signature, committed buffer, fence

### Probe 3: Triangle Render — PASS (180 frames, 0 fail)
- First-ever D3D12 triangle through AFMT without GPTK/CrossOver
- SM50 DXBC→Metal AIR compilation pipeline verified
- Yellow triangle on dark background via SV_VertexID

### Probe 3.5: Vertex Buffer + InputLayout + Color — PASS
- Per-vertex RGB color interpolation working
- 4 bug fixes applied:
  1. InputLayout PSO: WMTVertexDescriptor pointer (avoids PE/Unix ABI mismatch)
  2. ClearRenderTargetView: order-independent RTV lookup
  3. Reset(alloc, pso): initial_state now emits SetPipelineState
  4. SetPipelineState mid-stream: already worked

### Probe 4: Indexed Draw — PASS (180 frames, 0 fail)
- DrawIndexedInstanced with 16-bit index buffer
- 4-vertex quad (2 triangles), per-vertex colors

### Probe 5: Depth Test — PASS (180 frames, 0 fail)
- PSO with depth enabled (LESS comparison, D32_FLOAT)
- ClearDepthStencilView implemented (was no-op)
- DepthStencilState created and applied on render encoder
- Visual confirmation: correct z-ordering (near triangle in front)

### Probe 6: Texture Sampling — PASS (180 frames, 0 fail)
- Texture upload via CopyTextureRegion works
- Sampler creation with D3D12→WMT conversion works
- SM50 shader compiles with `air.sample_texture_2d.v4f32` instruction
- Argument buffer at slot 30 fully working: texture/sampler GPU resource IDs encoded at correct offsets
- **Fix**: Added `useResource` calls for texture resources and arg buffer — Metal requires explicit residency for argument buffer resources
- Visual confirmation: red/green checkerboard textured triangle rendering correctly

## Bug Fixes Applied (All Probes)
- setViewport: fixed wrong struct fields (was using wmtcmd_render_setviewports fields on wmtcmd_render_setviewport)
- waitForFence: added missing method to WMT::RenderCommandEncoder (D3D11 build dependency)
- setDepthStencilState: added WMT command type + C++ wrapper + unix dispatch
- ClearDepthStencilView: implemented (was no-op break)
- DepthStencilState creation in PSO compile (D3D12_COMPARISON_FUNC → WMTCompareFunction mapping)
- Sampler binding: setFragmentSamplerState WMT command type + unix dispatch + C++ wrapper
- CreateSampler: D3D12_SAMPLER_DESC → WMTSamplerInfo → Metal SamplerState
- ApplyRootBindings: separate tex_slot/samp_slot counters for texture/sampler bindings

## Key Architecture Decisions
- WMTVertexDescriptor as pointer (not inline) to avoid PE/Unix ABI mismatch
- ClearRenderTargetView made order-independent from OMSetRenderTargets
- D3D12_COMPARISON_FUNC to WMTCompareFunction: subtract 1 (D3D12 is 1-based, WMT is 0-based)
- SM50 shader pipeline: DXBC → SM50Initialize → SM50Compile → Metal AIR → newLibrary → newFunction
- Shader reflection stored on PSO for argument buffer encoding at bind time

## Compile/Run Commands
```bash
# Compile probe
x86_64-w64-mingw32-g++ -std=c++17 -O2 -o /tmp/probe<N>.exe tests/probe<N>/probe<N>.cpp -ld3d12 -ldxgi -ld3dcompiler -lgdi32 -luser32 -static

# Run probe
WINEPREFIX=/Volumes/AverySSD/metalsharp/runtime/prefix \
DYLD_FALLBACK_LIBRARY_PATH=/Volumes/AverySSD/metalsharp/runtime/wine/lib/wine/x86_64-unix:/Volumes/AverySSD/metalsharp/runtime/wine/lib/dxmt/x86_64-unix \
WINEDLLOVERRIDES="d3d12,dxgi,d3d11=n,b" \
/Volumes/AverySSD/metalsharp/runtime/wine/bin/wine /tmp/probe<N>.exe

# Build AFMT
ninja -C /Volumes/AverySSD/metalsharp/dxmt-src/build src/winemetal/unix/winemetal.so src/d3d12/d3d12.dll

# Deploy (all 3 locations + winemetal.so)
cp build/src/d3d12/d3d12.dll /Volumes/AverySSD/metalsharp/runtime/prefix/dosdevices/c:/windows/system32/d3d12.dll
cp build/src/d3d12/d3d12.dll /Volumes/AverySSD/metalsharp/runtime/wine/lib/dxmt/x86_64-windows/d3d12.dll
cp build/src/winemetal/unix/winemetal.so /Volumes/AverySSD/metalsharp/runtime/wine/lib/dxmt/x86_64-unix/winemetal.so
cp build/src/winemetal/unix/winemetal.so /Volumes/AverySSD/metalsharp/runtime/wine/lib/wine/x86_64-unix/winemetal.so
```
