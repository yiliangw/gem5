from m5.params import *
from m5.objects.XBar import NoncoherentXBar
from m5.objects.ClockedObject import ClockedObject
from m5.objects.PciDevice import (
    PciEndpoint,
    PciMemBar,
    PciMemUpperBar
)


class CxlBridge(ClockedObject):
    type = "CxlBridge"
    cxx_header = "dev/cxl/bridge.hh"
    cxx_class = "gem5::CxlBridge"

    mem_side_port = RequestPort(
        "This port sends requests and receives responses"
    )
    master = DeprecatedParam(
        mem_side_port, "`master` is now called `mem_side_port`"
    )
    cpu_side_port = ResponsePort(
        "This port receives requests and sends responses"
    )
    slave = DeprecatedParam(
        cpu_side_port, "`slave` is now called `cpu_side_port`"
    )

    req_fifo_depth = Param.Unsigned(48, "The number of requests to buffer")
    resp_fifo_depth = Param.Unsigned(48, "The number of responses to buffer")
    bridge_lat = Param.Latency("50ns", "The latency of this bridge")
    proto_proc_lat = Param.Latency(
        "14ns", "Conversion latency of cxl protocol in bridge")
    ranges = VectorParam.AddrRange(
        [AllMemory], "Address ranges to pass through the bridge"
    )


class CxlMemBar(NoncoherentXBar):
    # 128-bit crossbar by default
    width = 16

    # Assume a simpler datapath than a coherent crossbar, incuring
    # less pipeline stages for decision making and forwarding of
    # requests.
    frontend_latency = 2
    forward_latency = 1
    response_latency = 2


class CxlMemory(PciEndpoint):
    type = 'CxlMemory'
    cxx_header = 'dev/cxl/memory.hh'
    cxx_class = 'gem5::CxlMemory'

    cxl_rsp_port = ResponsePort(
        "This port sends responses to and receives requests from the Host"
    )
    mem_req_port = RequestPort(
        "This port sends requests to and receives responses from the back-end memory media"
    )

    rsp_size = Param.Unsigned(48, "The number of responses to buffer")
    req_size = Param.Unsigned(48, "The number of requests to buffer")

    proto_proc_lat = Param.Latency(
        "15ns", "Latency of the CXL controller processing CXL.mem sub-protocol packets")
    cxl_mem_range = Param.AddrRange(
        "2GB", "CXL expander memory range that can be identified as system memory")

    VendorID = 0x8086
    DeviceID = 0X7890
    Command = 0x0
    Status = 0x280
    Revision = 0x0
    ClassCode = 0x05
    SubClassCode = 0x00
    ProgIF = 0x00
    InterruptLine = 0x1f
    InterruptPin = 0x01

    # Primary
    BAR0 = PciMemBar(size='2GB')
    BAR1 = PciMemUpperBar()
