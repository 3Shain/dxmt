#include "dxmt_device.hpp"
#include "config/config.hpp"
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
    int version_conf = Config::getInstance().getOption<int>("dxmt.shaderMetalVersion", 0);
    switch (version_conf) {
    case WMTMetal310:
    case WMTMetal320:
      metal_version_ = (WMTMetalVersion)version_conf;
      break;
    default:
      metal_version_ = WMTMetalVersionMax;
      break;
    }
    WMTGetOSVersion(&macos_major_version, nullptr, nullptr);
    if (macos_major_version >= 15) {
      metal_version_ = std::min(WMTMetal320, metal_version_);
    } else {
      metal_version_ = std::min(WMTMetal310, metal_version_);
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
