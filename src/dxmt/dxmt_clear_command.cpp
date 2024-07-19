#include "dxmt_clear_command.hpp"
#include "Foundation/NSAutoreleasePool.hpp"
#include "Foundation/NSError.hpp"
#include "Foundation/NSString.hpp"
#include "Metal/MTLLibrary.hpp"

extern "C" unsigned char dxmt_clear_command_metallib[];
extern "C" unsigned int dxmt_clear_command_metallib_len;

#define CREATE_PIPELINE(name)                                                  \
  auto name##_function = transfer(library->newFunction(                        \
      NS::String::string(#name, NS::ASCIIStringEncoding)));                    \
  name##_pipeline =                                                            \
      transfer(device->newComputePipelineState(name##_function, &error));

namespace dxmt {
ClearCommandContext::ClearCommandContext(MTL::Device *device) {
  auto pool = transfer(NS::AutoreleasePool::alloc()->init());
  auto dispatch_data =
      dispatch_data_create(dxmt_clear_command_metallib,
                           dxmt_clear_command_metallib_len, nullptr, nullptr);

  Obj<NS::Error> error;
  auto library = transfer(device->newLibrary(dispatch_data, &error));

  CREATE_PIPELINE(clear_texture_1d_uint);
  CREATE_PIPELINE(clear_texture_1d_array_uint);
  CREATE_PIPELINE(clear_texture_2d_uint);
  CREATE_PIPELINE(clear_texture_2d_array_uint);
  CREATE_PIPELINE(clear_texture_3d_uint);
  CREATE_PIPELINE(clear_texture_buffer_uint);
  CREATE_PIPELINE(clear_buffer_uint);
  CREATE_PIPELINE(clear_texture_1d_float);
  CREATE_PIPELINE(clear_texture_1d_array_float);
  CREATE_PIPELINE(clear_texture_2d_float);
  CREATE_PIPELINE(clear_texture_2d_array_float);
  CREATE_PIPELINE(clear_texture_3d_float);
  CREATE_PIPELINE(clear_texture_buffer_float);

  dispatch_release(dispatch_data);
}
} // namespace dxmt