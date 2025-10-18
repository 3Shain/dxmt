#include "dxmt_device.hpp"
#include "dxmt_command_queue.hpp"
#include "Metal.hpp"

namespace dxmt {

class DeviceImpl : public Device {
public:
  virtual WMT::Device
  device() override {
    return device_;
  };
  virtual CommandQueue &
  queue() override {
    return cmd_queue_;
  };

  virtual WMTMetalVersion metalVersion() override {
    return metal_version_;
  };

  DeviceImpl(const DEVICE_DESC &desc) : device_(desc.device), cmd_queue_(device_) {
    uint64_t macos_major_version = 0;
    device_.setShouldMaximizeConcurrentCompilation(true);
    WMTGetOSVersion(&macos_major_version, nullptr, nullptr);
    if (macos_major_version >= 15) {
      metal_version_ = WMTMetal320;
    } else {
      metal_version_ = WMTMetal310;
    }
  }

private:
  WMT::Reference<WMT::Device> device_;
  CommandQueue cmd_queue_;
  WMTMetalVersion metal_version_;
};

std::unique_ptr<Device>
CreateDXMTDevice(const DEVICE_DESC &desc) {
  return std::make_unique<DeviceImpl>(desc);
};

} // namespace dxmt
