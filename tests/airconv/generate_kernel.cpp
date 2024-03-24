#include "d3dcommon.h"
#include "../../src/airconv/airconv_public.h"
#include <cassert>
#include <fstream>
#include <ios>
#include "d3dcompiler.h"

int main() {
  ID3DBlob *vsBlob;
  // ID3D11VertexShader *vertexShader;
  ID3DBlob *shaderCompileErrorsBlob;
  HRESULT hResult =
      D3DCompileFromFile(L"../dx11/shader_cube.hlsl", nullptr, nullptr, "ps_main", "ps_5_0",
                         0, 0, &vsBlob, &shaderCompileErrorsBlob);

  assert(SUCCEEDED(hResult));

  LPVOID air;
  UINT airSize;
  ConvertDXBC(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &air, &airSize);

  std::ofstream out;
  out.open("dest.metallib", std::ios::out | std::ios::binary);
  out.write((const char *)air, airSize);
  out.close();

  return 0;
}