#include "dxmt_device.hpp"
#include "dxmt_command_queue.hpp"
#include "Metal.hpp"

namespace dxmt {

class DeviceImpl : public Device {
public:
  virtual MTL::Device *
  device() override {
    return (MTL::Device *)device_.handle;
  };
  virtual CommandQueue &
  queue() override {
    return cmd_queue_;
  };

  DeviceImpl(const DEVICE_DESC &desc) : device_(desc.device), cmd_queue_(device_) {}

private:
  WMT::Reference<WMT::Device> device_;
  CommandQueue cmd_queue_;
};

std::unique_ptr<Device>
CreateDXMTDevice(const DEVICE_DESC &desc) {
  return std::make_unique<DeviceImpl>(desc);
};

} // namespace dxmt
