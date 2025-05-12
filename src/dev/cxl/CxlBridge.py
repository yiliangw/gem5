from m5.params import *
from m5.objects.ClockedObject import ClockedObject


class CXLBridge(ClockedObject):
    type = "CXLBridge"
    cxx_header = "mem/cxl_bridge.hh"
    cxx_class = "gem5::CXLBridge"

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

    req_fifo_depth= Param.Unsigned(48, "The number of requests to buffer")
    resp_fifo_depth = Param.Unsigned(48, "The number of responses to buffer")
    bridge_lat = Param.Latency("50ns", "The latency of this bridge")
    proto_proc_lat = Param.Latency("14ns", "Conversion latency of cxl protocol in bridge")
    ranges = VectorParam.AddrRange(
        [AllMemory], "Address ranges to pass through the bridge"
    )