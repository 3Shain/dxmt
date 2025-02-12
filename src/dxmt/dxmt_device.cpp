#include "dxmt_device.hpp"
#include "Metal/MTLDevice.hpp"
#include "dxmt_command_queue.hpp"

namespace dxmt {

class DeviceImpl : public Device {
public:
  virtual MTL::Device *
  device() override {
    return device_;
  };
  virtual CommandQueue &
  queue() override {
    return cmd_queue_;
  };

  DeviceImpl(const DEVICE_DESC &desc) : device_(desc.device), cmd_queue_(desc.device) {
    device_->setShouldMaximizeConcurrentCompilation(true);
  }

private:
  Obj<MTL::Device> device_;
  CommandQueue cmd_queue_;
};

std::unique_ptr<Device>
CreateDXMTDevice(const DEVICE_DESC &desc) {
  return std::make_unique<DeviceImpl>(desc);
};

} // namespace dxmt
