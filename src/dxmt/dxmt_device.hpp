#pragma once
#include "Metal/MTLDevice.hpp"
#include "dxmt_command_queue.hpp"
#include <memory>

namespace dxmt {

class Device {
public:
  virtual ~Device() {}

  virtual MTL::Device* device() = 0;
  virtual CommandQueue& queue() = 0;
};

struct DEVICE_DESC {
  MTL::Device *device;
};

std::unique_ptr<Device> CreateDXMTDevice(const DEVICE_DESC &desc);

} // namespace dxmt
