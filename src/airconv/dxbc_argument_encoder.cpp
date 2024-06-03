#include "airconv_public.h"
#include "dxbc_converter.hpp"
#include <functional>

#ifdef __OBJC__
#import <Metal/Metal.h>
ArgumentEncoder_t CreateArgumentEncoderInternal(
  dxmt::dxbc::ShaderInfo *shader_info, Device_t __mtlDevice
) {
  using namespace dxmt::shader::common;
  id<MTLDevice> mtlDevice = (id<MTLDevice>)__mtlDevice;
  auto descs = [[NSMutableArray alloc] init];

  auto addNewField = [&](std::function<void(MTLArgumentDescriptor *)> &&fn) {
    MTLArgumentDescriptor *desc = [[MTLArgumentDescriptor alloc] init];
    fn(desc);
    [descs addObject:desc];
    [desc release];
  };

  for (auto &[range_id, cbv] : shader_info->cbufferMap) {
    addNewField([&](MTLArgumentDescriptor *descriptor) {
      descriptor.access = MTLBindingAccessReadOnly;
      descriptor.arrayLength = 0;
      descriptor.dataType = MTLDataTypePointer;
      descriptor.index =
        GetArgumentIndex(SM50BindingType::ConstantBuffer, range_id);
    });
  }
  for (auto &[range_id, sampler] : shader_info->samplerMap) {
    addNewField([&](MTLArgumentDescriptor *descriptor) {
      descriptor.access = MTLBindingAccessReadOnly;
      descriptor.arrayLength = 0;
      descriptor.dataType = MTLDataTypeSampler;
      descriptor.index = GetArgumentIndex(SM50BindingType::Sampler, range_id);
    });
  }
  for (auto &[range_id, srv] : shader_info->srvMap) {
    MTLTextureType texture_type = MTLTextureType1D;
    MTLDataType data_type = MTLDataTypeTexture;
    switch (srv.resource_type) {
    case ResourceType::TextureBuffer:
      if (srv.strucure_stride >= 0) {
        data_type = MTLDataTypePointer;
      }
      texture_type = MTLTextureTypeTextureBuffer;
      break;
    case ResourceType::Texture1D:
      texture_type = MTLTextureType1D;
      break;
    case ResourceType::Texture1DArray:
      texture_type = MTLTextureType1DArray;
      break;
    case ResourceType::Texture2D:
      texture_type = MTLTextureType2D;
      break;
    case ResourceType::Texture2DArray:
      texture_type = MTLTextureType2DArray;
      break;
    case ResourceType::Texture2DMultisampled:
      texture_type = MTLTextureType2DMultisample;
      break;
    case ResourceType::Texture2DMultisampledArray:
      texture_type = MTLTextureType2DMultisampleArray;
      break;
    case ResourceType::Texture3D:
      texture_type = MTLTextureType3D;
      break;
    case ResourceType::TextureCube:
      texture_type = MTLTextureTypeCube;
      break;
    case ResourceType::TextureCubeArray:
      texture_type = MTLTextureTypeCubeArray;
      break;
    }
    addNewField([&](MTLArgumentDescriptor *descriptor) {
      descriptor.access = MTLBindingAccessReadOnly;
      descriptor.arrayLength = 0;
      descriptor.index = GetArgumentIndex(SM50BindingType::SRV, range_id);
      descriptor.dataType = data_type;
      descriptor.textureType = texture_type;
    });
  }
  for (auto &[range_id, uav] : shader_info->uavMap) {
    MTLTextureType texture_type = MTLTextureType1D;
    MTLDataType data_type = MTLDataTypeTexture;
    switch (uav.resource_type) {
    case ResourceType::TextureBuffer:
      if (uav.strucure_stride >= 0) {
        data_type = MTLDataTypePointer;
      }
      texture_type = MTLTextureTypeTextureBuffer;
      break;
    case ResourceType::Texture1D:
      texture_type = MTLTextureType1D;
      break;
    case ResourceType::Texture1DArray:
      texture_type = MTLTextureType1DArray;
      break;
    case ResourceType::Texture2D:
      texture_type = MTLTextureType2D;
      break;
    case ResourceType::Texture2DArray:
      texture_type = MTLTextureType2DArray;
      break;
    case ResourceType::Texture2DMultisampled:
      texture_type = MTLTextureType2DMultisample;
      break;
    case ResourceType::Texture2DMultisampledArray:
      texture_type = MTLTextureType2DMultisampleArray;
      break;
    case ResourceType::Texture3D:
      texture_type = MTLTextureType3D;
      break;
    case ResourceType::TextureCube:
      texture_type = MTLTextureTypeCube;
      break;
    case ResourceType::TextureCubeArray:
      texture_type = MTLTextureTypeCubeArray;
      break;
    }
    if (uav.with_counter) {
      assert(0 && "TODO: implement counter slot");
    }
    addNewField([&](MTLArgumentDescriptor *descriptor) {
      descriptor.access = uav.written ? (uav.read ? MTLBindingAccessReadWrite
                                                  : MTLBindingAccessWriteOnly)
                                      : MTLBindingAccessReadOnly;
      descriptor.arrayLength = 0;
      descriptor.index = GetArgumentIndex(SM50BindingType::UAV, range_id);
      descriptor.dataType = data_type;
      descriptor.textureType = texture_type;
    });
  }

  if ([descs count] == 0) {
    [descs release];
    return nullptr;
  }

  auto mtlAncillaryArgEncoder =
    [mtlDevice newArgumentEncoderWithArguments:descs];

  [descs release];
  return (ArgumentEncoder_t)mtlAncillaryArgEncoder;
}
#else
ArgumentEncoder_t CreateArgumentEncoderInternal(
  dxmt::dxbc::ShaderInfo *shader_info, Device_t __mtlDevice
) {
  return nullptr;
}
#endif
