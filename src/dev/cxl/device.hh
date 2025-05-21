#ifndef __DEV_CXL_DEVICE_HH__
#define __DEV_CXL_DEVICE_HH__

#include "dev/pci/device.hh"
#include "params/CxlBridge.hh"
#include "params/CxlDevice.hh"
#include "params/CxlEndpoint.hh"

namespace gem5
{

class CxlDevice : public PciDevice
{
  public:
    CxlDevice(const CxlDeviceParams &params) : PciDevice(params) {}
};

class CxlEndpoint : public PciEndpoint
{
  public:
    CxlEndpoint(const CxlDeviceParams &params) : PciEndpoint(params) {}
};

class CxlBridge : public PciBridge
{
  public:
    CxlBridge(const CxlBridgeParams &params) : PciBridge(params) {}
};

} // namespace gem5

#endif // __DEV_CXL_DEVICE_HH__
