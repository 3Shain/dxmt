/*
 * Copyright 2026 Feifan He for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "d3d12_command_allocator.hpp"
#include "com/com_pointer.hpp"

namespace dxmt {

MTLD3D12CommandAllocatorImpl::MTLD3D12CommandAllocatorImpl(MTLD3D12Device *pDevice, D3D12_COMMAND_LIST_TYPE Type) :
    MTLD3D12Pageable<MTLD3D12CommandAllocator>(pDevice),
    type_(Type) {}

HRESULT
CreateCommandAllocator(MTLD3D12Device *pDevice, D3D12_COMMAND_LIST_TYPE Type, REFIID riid, void **ppCommandAllocator) {
  switch (Type) {
  case D3D12_COMMAND_LIST_TYPE_DIRECT:
  case D3D12_COMMAND_LIST_TYPE_BUNDLE:
  case D3D12_COMMAND_LIST_TYPE_COMPUTE:
  case D3D12_COMMAND_LIST_TYPE_COPY:
    break;
  default:
    return E_INVALIDARG;
  }
  auto command_allocator = Com(new MTLD3D12CommandAllocatorImpl(pDevice, Type));
  HRESULT hr = command_allocator->Initialize();
  if (FAILED(hr))
    return hr;
  return command_allocator->QueryInterface(riid, ppCommandAllocator);
}

HRESULT
MTLD3D12CommandAllocatorImpl::Initialize() {
  return S_OK;
}

HRESULT
STDMETHODCALLTYPE
MTLD3D12CommandAllocatorImpl::QueryInterface(REFIID riid, void **ppvObject) {
  if (ppvObject == nullptr)
    return E_POINTER;

  *ppvObject = nullptr;

  if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D12Object) || riid == __uuidof(ID3D12DeviceChild) ||
      riid == __uuidof(ID3D12Pageable) || riid == __uuidof(ID3D12CommandAllocator)) {
    *ppvObject = ref(this);
    return S_OK;
  }

  if (logQueryInterfaceError(__uuidof(ID3D12CommandQueue), riid)) {
    WARN("D3D12CommandAllocator: Unknown interface query ", str::format(riid));
  }

  return E_NOINTERFACE;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12CommandAllocatorImpl::Reset() {
  return Initialize();
};

}; // namespace dxmt