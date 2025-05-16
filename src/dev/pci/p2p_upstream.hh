/*
 * Copyright (c) 2015 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __DEV_PCI_P2P_UPSTREAM_HH__
#define __DEV_PCI_P2P_UPSTREAM_HH__

#include "base/addr_range.hh"
#include "dev/pci/p2p_bridge.hh"
#include "dev/pci/types.hh"
#include "dev/pci/up_down_bridge.hh"
#include "dev/pci/upstream.hh"
#include "params/PciToPciUpstream.hh"

namespace gem5
{

class Platform;

/**
 * The PCI host describes the interface between PCI main bus and a
 * simulated system.
 *
 * The PCI host controller has three main responsibilities:
 * <ol>
 *     <li>Bridge memory packet between the PCI main bus and the system bus.
 *     <li>Map and deliver interrupts to the CPU.
 *     <li>Map memory addresses from the PCI bus's various memory
 *         spaces (Legacy IO, non-prefetchable memory, and
 *         prefetchable memory) to physical memory.
 * </ol>
 *
 * PCI devices that are on the main PCI bus need to register themselves with a
 * PCI host using the PciUptream API to receive a PciUpstream::DeviceInterface.
 *
 * The PciHost class itself provides very little functionality. Simple
 * PciHost functionality is implemented by the GenericPciHost class. The actual
 * bridge is implemented by PciHostBridge which is a member of this class.
 */
class PciToPciUpstream : public PciUpstream
{
  public:
    PARAMS(PciToPciUpstream);
    PciToPciUpstream(const Params &p);
    virtual ~PciToPciUpstream();

    /**
     * Get the range for the configuration memory space for which this PCI
     * host is responsible. The range should include the full configuration
     * space even where no bus/device are present.
     */
    AddrRange getConfigAddrRange() const override;

  protected:
    AddrRange interfaceConfigRange(PciBusNum bus_num,
                                   const PciDevAddr &dev_addr) const override;

    Addr interfacePioAddr(PciBusNum bus_num, const PciDevAddr &dev_addr,
                          Addr pci_addr) const override;

    Addr interfaceMemAddr(PciBusNum bus_num, const PciDevAddr &dev_addr,
                          Addr pci_addr) const override;

    Addr interfaceDmaAddr(PciBusNum bus_num, const PciDevAddr &dev_addr,
                          Addr pci_addr) const override;

    AddrRange interfaceBusConfigRange(PciBusNum start_bus,
                                      PciBusNum end_bus) const override;

    PciBusNum getBusNum() const override;

    void interfacePostInt(PciBusNum bus_num, const PciDevAddr &dev_addr,
                          PciIntPin pin) override;
    void interfaceClearInt(PciBusNum bus_num, const PciDevAddr &dev_addr,
                           PciIntPin pin) override;

    PciUpDownBridge *bridge;
    PciToPciBridge *device;
    PciUpstream::BridgeInterface nextUpstream;
};

} // namespace gem5

#endif // __DEV_PCI_P2P_UPSTREAM_HH__
