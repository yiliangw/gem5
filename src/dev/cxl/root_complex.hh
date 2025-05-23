#ifndef __DEV_CXL_ROOT_COMPLEX_HH__
#define __DEV_CXL_ROOT_COMPLEX_HH__

#include "dev/cxl/device.hh"
#include "dev/pci/host.hh"
#include "dev/pci/p2p_upstream.hh"
#include "params/CxlRootComplex.hh"
#include "sim/sim_object.hh"

namespace gem5
{

class CxlRootComplex : public SimObject
{
  public:
    PARAMS(GenericPciHost);

    CxlRootComplex(const Params &params);

    ~CxlRootComplex() override;

    Port &getPort(const std::string &if_name,
                  PortID idx = InvalidPortID) override;

    void init() override;

  private:
    GenericPciHost *host;

    std::vector<PciToPciUpstream *> roots;
};

} // namespace gem5

#endif // __DEV_CXL_RC_HH__
