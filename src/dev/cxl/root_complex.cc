#include "dev/cxl/root_complex.hh"
#include "sim/port.hh"

namespace gem5
{
CxlRootComplex::CxlRootComplex(const Param &p)
    : SimObject(p), host(p.host)
{
    panic_if(p.port__root_response_ports_connection_count != p.port__root_request_ports_connection_count, "The number of response ports and request ports for root ports must be the same");
    for (int i = 0; i < p.port__root_response_ports_connection_count; ++i) {
        roots.emplace_back();
    }
}

CxlRootComplex::~CxlRootComplex()
{

}

CxlRootComplex::getPort(const std::string &if_name, PortID idx)
{
}

}