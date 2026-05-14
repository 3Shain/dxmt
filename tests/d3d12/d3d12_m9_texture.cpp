#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#include <windows.h>
#include <dxgi1_6.h>
#include "d3d12_private.h"
#include <cstdio>
#include <cstring>

static bool ok = true;
#define CHECK(e,m) do { if(!(e)){printf("  FAIL: %s\n",m);ok=false;}else{printf("  PASS: %s\n",m);} } while(0)
#define CHK(h,m) CHECK(SUCCEEDED(h),m)

int main() {
    printf("=== M9 Texture Upload Test ===\n\n");
    IDXGIFactory4 *f = nullptr; CreateDXGIFactory2(0,IID_PPV_ARGS(&f));
    IDXGIAdapter1 *a = nullptr; for(UINT i=0;f->EnumAdapters1(i,&a)!=DXGI_ERROR_NOT_FOUND;i++)if(a)break;
    ID3D12Device *d = nullptr; D3D12CreateDevice(a,D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&d));

    // Create upload texture
    D3D12_HEAP_PROPERTIES up = {}; up.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC td = {};
    td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    td.Width = 64; td.Height = 64; td.DepthOrArraySize = 1;
    td.MipLevels = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;

    ID3D12Resource *tex = nullptr;
    HRESULT hr = d->CreateCommittedResource(&up,D3D12_HEAP_FLAG_NONE,&td,
        D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&tex));
    CHK(hr,"CreateCommittedResource(texture)");

    // Write to texture
    uint8_t data[64*64*4];
    for(int i=0;i<64*64*4;i++) data[i]=(uint8_t)(i%256);
    hr = tex->WriteToSubresource(0,nullptr,data,64*4,0);
    CHK(hr,"WriteToSubresource(texture)");

    // Read back
    uint8_t rb[64*64*4]={};
    hr = tex->ReadFromSubresource(rb,64*4,0,0,nullptr);
    CHK(hr,"ReadFromSubresource(texture)");
    CHECK(memcmp(data,rb,64*64*4)==0,"Readback matches write");

    // GetCopyableFootprints
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp;
    UINT64 total;
    hr = d->GetCopyableFootprints(&td,0,1,0,&fp,nullptr,nullptr,&total);
    CHK(hr,"GetCopyableFootprints");
    CHECK(fp.Footprint.Width==64,"Footprint width");
    CHECK(fp.Footprint.RowPitch==256,"Footprint row pitch (64*4)");

    tex->Release(); d->Release(); a->Release(); f->Release();
    printf("\n========================================\n");
    printf(ok?"  M9 Test: ALL PASSED\n":"  M9 Test: FAILURES\n");
    printf("========================================\n");
    return ok?0:1;
}
