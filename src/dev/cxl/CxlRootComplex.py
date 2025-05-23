from m5.SimbOject import SimObject
from m5.objects.PciHost import GenericPciHost
from m5.objects.PciDevice import PciBus
from m5.objects.PciToPciUpstream import PciToPciUpstream
from m5.objects.XBar import PciXBar
from m5.params import *


class CxlRootComplex(SimObject):
    type = "CxlRootComplex"
    cxx_class = "gem5::CxlRootComplex"
    cxx_header = "dev/cxl/root_complex.hh"

    host = Param.GenericPciHost(GenericPciHost(), "The PCI host")
    root_bus = Param.PciXBar(PciXBar(), "PCI root bus")
    root_ports = VectorParam.PciToPciUpstream([], "PCI root ports")

    def __init__(self, root_port_nb: int = 1):
        super().__init__()
        # Initialize root ports and connect them to the root bus
        if root_port_nb < 0:
            raise ValueError(f"Invalide number of root ports: {root_port_nb}")
        for _ in root_port_nb:
            port = PciToPciUpstream()
            port.device.upstream = self.host
            port.bridge.up_response_port = self.root_bus.mem_side_ports
            port.bridge.up_request_port = self.root_bus.cpu_side_ports
            self.root_ports.append(port)
        # Connect the host bridge to the root bus
        self.host.down_request_port = self.root_bus.cpu_side_ports
        self.host.down_response_port = self.root_bus.mem_side_ports

    def get_root_

    def connect_root_port(self, port_idx, response_port: ResponsePort, request_port: RequestPort):
        port = self.root_ports[port_idx]
        port.bridge
