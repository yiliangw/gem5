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

#include "dev/pci/p2p_upstream.hh"

#include "base/addr_range.hh"
#include "base/types.hh"
#include "dev/pci/types.hh"
#include "dev/pci/up_down_bridge.hh"
#include "dev/pci/upstream.hh"
#include "params/PciToPciUpstream.hh"

namespace gem5
{

PciToPciUpstream::PciToPciUpstream(const Params &p)
    : PciUpstream(p),
      bridge(p.bridge),
      device(p.device),
      nextUpstream(p.device->params().upstream->registerBridge())
{
    bridge->setUpstream(this);
}

PciToPciUpstream::~PciToPciUpstream() {}

PciBusNum
PciToPciUpstream::getBusNum() const
{
    return device->getSecondaryBus();
}

AddrRange
PciToPciUpstream::getConfigAddrRange() const
{
    if (getBusNum() == 0) {
        return AddrRange();
    }

    return nextUpstream.busConfigRange(getBusNum(),
                                       device->getSubordinateBus());
}

AddrRange
PciToPciUpstream::interfaceConfigRange(PciBusNum bus_num,
                                       const PciDevAddr &dev_addr) const
{
    if (getBusNum() == 0) {
        return AddrRange();
    }

    return nextUpstream.configRange(bus_num, dev_addr);
}

Addr
PciToPciUpstream::interfacePioAddr(PciBusNum bus_num,
                                   const PciDevAddr &dev_addr,
                                   Addr pci_addr) const
{
    if (getBusNum() == 0) {
        return MaxAddr;
    }

    return nextUpstream.pioAddr(bus_num, dev_addr, pci_addr);
}

Addr
PciToPciUpstream::interfaceMemAddr(PciBusNum bus_num,
                                   const PciDevAddr &dev_addr,
                                   Addr pci_addr) const
{
    if (getBusNum() == 0) {
        return MaxAddr;
    }

    return nextUpstream.memAddr(bus_num, dev_addr, pci_addr);
}

Addr
PciToPciUpstream::interfaceDmaAddr(PciBusNum bus_num,
                                   const PciDevAddr &dev_addr,
                                   Addr pci_addr) const
{
    if (getBusNum() == 0) {
        return MaxAddr;
    }

    return nextUpstream.dmaAddr(bus_num, dev_addr, pci_addr);
}

void
PciToPciUpstream::interfacePostInt(PciBusNum bus_num,
                                   const PciDevAddr &dev_addr, PciIntPin pin)
{
    if (getBusNum() == 0) {
        warn("Posting interrupt on unmapped bus\n");
        return;
    }

    return nextUpstream.postInt(bus_num, dev_addr, pin);
}

void
PciToPciUpstream::interfaceClearInt(PciBusNum bus_num,
                                    const PciDevAddr &dev_addr, PciIntPin pin)
{
    if (getBusNum() == 0) {
        warn("Clearing interrupt on unmapped bus\n");
        return;
    }

    return nextUpstream.clearInt(bus_num, dev_addr, pin);
}

AddrRange
PciToPciUpstream::interfaceBusConfigRange(PciBusNum start_bus,
                                          PciBusNum end_bus) const
{
    return nextUpstream.busConfigRange(start_bus, end_bus);
}

} // namespace gem5
