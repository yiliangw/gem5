from m5.objects.PciDevice import (
    PciDevice, PciEndpoint, PciBridge
)


class CxlDevice(PciDevice):
    type = "CxlDevice"
    cxx_class = "gem5::CxlDevice"
    cxx_header = "dev/cxl/device.hh"
    abstract = True


class CxlEndpoint(PciEndpoint):
    type = "CxlEndpoint"
    cxx_class = "gem5:CxlEndpoint"
    cxx_header = "dev/cxl/device.hh"


class CxlBridge(PciBridge):
    type = "CxlBridge"
    cxx_class = "gem5:CxlBridge"
    cxx_header = "dev/cxl/device.hh"
