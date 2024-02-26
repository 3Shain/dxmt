# Current Status of `dxmt`

The main (and only) contributor, 3Shain, doesn't have enough time and energy to work on it.

How could this situation be improved?

- A stable funding to support 3Shain work on it full-time. **He is going to be available on job market this summer.**
- Or just wait for years with no promised ETA

## Goals, Milestones and Some Technical Details

The goal in general is to implement a graphical API translation layer, from Direct3D to Metal. And currently we intend to first focus on 64-bit Direct3D 11 and Metal 3.1 as source and target, on **Apple Silicon**. Higher (as well as lower) version of Direct3D probably will be considered, but we won't support any older version of Metal. (That's to say, macOS 14 Sonoma will be a hard requirement.). Intel devices are not primarily supported, but will be considered if possible (because the memory model is different).

> Why Metal 3.1? Because it provides some APIs that huge simplifies some task, e.g. dynamic vertex strides. **And that's probably becuase GPTK needs those features (and of course dxmts need them as well!)**

To archive the primary goal (D3D11 to Metal 3.1), there are three major challenges:

1. a framework, that provides Metal API (which is Apple's OS exclusive) in `Wine` PE (WoW64) side. (This is pretty much done.)
2. shader transpilation/bytecode conversion, from `dxbc` to `msl`/`air`
3. and translation from Direct11 APIs to Metal APIs, in `Wine` PE side.

### Why implement in `Wine` PE side instead of Unix side?

Because of the WoW64 architecture of wine 8+.

PE-to-Unix call is trivial to do (if the code already runs on x86_64 mode, otherwise a syscall/unixcall is required), but Unix-to-PE call is much harder **because every wine thread is always a unix thread, but a unix thread is not necessarily a wine thread**. And wine puts important informations (e.g. `PEB`) in thread context/segment registers, which are available on threads managed by wine.

To simplify the implementation, I chose to implement things inside PE. (and in fact this introduces another problem: the callbacks of unix API (e.g. ObjC Blocks!), they are invoked from unix thread/dispatch queue...and need extra cares) (and funny fact: directx doesn't provides many callback-style APIs? but Metal exploits callback a lot)

In comparison, GPTK implements everything (except for some dummy dlls forwarding APIs) in unix side, and it's not a problem for a x86_64 Wine program to invoke GPTK interfaces. And all wine APIs that GPTK needs access are exported via symbol `macdrv_functions` in `winemac.so`, while some of them are only available in PE side in wine 8+. Note that GPTK primarily operates on crossover 22.1, which is based on wine 7.7. So to make GPTK work on wine 8 some extra efforts are needed, i.e. implement kernel callbacks. In fact GPTK doesn't really use many PE-exclusive APIs (because it's made by Apple, whose developers know about their own system), so it seems to be not a problem because a Unix-to-PE call is rare and they don't implement features that calls to user callbacks?

However we need to take 32-bit programs into consideration (which is not gonna to happen in the short run), there is only one option: because Apple just killed 32-bit macOS.

### Possible routes on shader transpilation

At the moment there are 4 routes

#### 1. [HLSLcc](https://github.com/Unity-Technologies/HLSLcc) by Unity

HLSLcc translates dxbc to msl. Currently it's used as proof of concept.


#### 2. [MetalShaderConverter](https://developer.apple.com/metal/shader-converter/)

Made by Apple, translates dxil to air. But it's closed source. And we need an extra step to convert dxbc to dxil first.


#### 3. dxvk + SPIR-V

First convert dxbc to spirv, then msl.

**I mean this is definitely viable but I won't consider it seriously**

#### 4. Create Our Own

Actually we can make a dxbc to air converter. This is viable because we can use these projects as references:

- [D3D11TranslationLater](https://github.com/microsoft/D3D12TranslationLayer)
- [DirectXShaderCompiler](https://github.com/microsoft/DirectXShaderCompiler)
- [floor](https://github.com/a2flo/floor)

These projects give solutions to critical problems
- how to parse DXBC
- how to convert DXBC to LLVM IR
- how to decompile metallib
- how to emit AIR and metallib

and the only thing we need to solve is to find all air symbols e.g. `air.fast_rsqr` and their semantics. These are definitely nowhere published but I think it's not hard if you play a lot with [shader playground](https://shader-playground.timjones.io/).

---
I'm in favor of \#4 and it's my current plan. Although it will take longer to achieve a decent implementation. But a factor I have to take into account is the **resource binding model** which highly couples shader and graphical pipeline transpilation. Therefore to move from one route to another is not trivial at all. I will be more confident if more things are in my control.

### More On Difference and Similarity between D3D11 and Metal. 

// TBD

[Glossary](https://github.com/3Shain/dxmt/blob/main/docs/GLOSSARY.md)