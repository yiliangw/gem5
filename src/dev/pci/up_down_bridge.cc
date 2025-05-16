/*
 * Copyright (c) 2025 REDS institute of the HEIG-VD
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

#include "dev/pci/up_down_bridge.hh"

#include <algorithm>

#include "base/addr_range.hh"
#include "base/logging.hh"
#include "base/types.hh"
#include "debug/PciUpDownBridge.hh"
#include "dev/pci/upstream.hh"
#include "params/PciUpDownBridge.hh"
#include "sim/clocked_object.hh"

namespace gem5
{

PciUpDownBridge::PciUpDownBridge(const PciUpDownBridgeParams &p)
    : ClockedObject(p),
      upRequestPort(p.name + ".up_request_port", *this, downResponsePort,
                    ticksToCycles(p.delay), p.req_size),
      downResponsePort(p.name + ".down_response_port", *this, upRequestPort,
                       ticksToCycles(p.delay), p.resp_size),
      downRequestPort(p.name + ".down_request_port", *this, upResponsePort,
                      ticksToCycles(p.delay), p.req_size),
      upResponsePort(p.name + ".up_response_port", *this, downRequestPort,
                     ticksToCycles(p.delay), p.resp_size)
{}

PciUpDownBridge::~PciUpDownBridge() {}

Port &
PciUpDownBridge::getPort(const std::string &if_name, PortID idx)
{
    if (if_name == "up_request_port")
        return upRequestPort;
    else if (if_name == "up_response_port")
        return upResponsePort;
    else if (if_name == "down_request_port")
        return downRequestPort;
    else if (if_name == "down_response_port")
        return downResponsePort;
    else
        return ClockedObject::getPort(if_name, idx);
}

void
PciUpDownBridge::init()
{
    fatal_if(!upstream, "No PCI upstream given for this bridge.\n");

    // make sure all ports are connected
    if (!upRequestPort.isConnected() || !downResponsePort.isConnected() ||
        !downRequestPort.isConnected() || !upResponsePort.isConnected())
        fatal("All ports of the host bridge must be connected.\n");

    upResponsePort.ranges = { upstream->getConfigAddrRange() };
    downResponsePort.ranges = { AddrRange(0, -1) };

    downResponsePort.sendRangeChange();
    upResponsePort.sendRangeChange();

    upstream->sendBusChange();
}

PciUpDownBridge::UpDownBridgeResponsePort::UpDownBridgeResponsePort(
    const std::string &_name, PciUpDownBridge &_bridge,
    UpDownBridgeRequestPort &_requestPort, Cycles _delay, int _resp_limit)
    : ResponsePort(_name),
      bridge(_bridge),
      requestPort(_requestPort),
      delay(_delay),
      ranges(),
      outstandingResponses(0),
      retryReq(false),
      respQueueLimit(_resp_limit),
      sendEvent([this] { trySendTiming(); }, _name)
{}

PciUpDownBridge::UpDownBridgeRequestPort::UpDownBridgeRequestPort(
    const std::string &_name, PciUpDownBridge &_bridge,
    UpDownBridgeResponsePort &_responsePort, Cycles _delay, int _req_limit)
    : RequestPort(_name),
      bridge(_bridge),
      responsePort(_responsePort),
      delay(_delay),
      reqQueueLimit(_req_limit),
      sendEvent([this] { trySendTiming(); }, _name)
{}

bool
PciUpDownBridge::UpDownBridgeResponsePort::respQueueFull() const
{
    return outstandingResponses == respQueueLimit;
}

bool
PciUpDownBridge::UpDownBridgeRequestPort::reqQueueFull() const
{
    return transmitList.size() == reqQueueLimit;
}

bool
PciUpDownBridge::UpDownBridgeRequestPort::recvTimingResp(PacketPtr pkt)
{
    // all checks are done when the request is accepted on the response
    // side, so we are guaranteed to have space for the response
    DPRINTF(PciUpDownBridge, "recvTimingResp: %s addr 0x%x\n",
            pkt->cmdString(), pkt->getAddr());

    DPRINTF(PciUpDownBridge, "Request queue size: %d\n", transmitList.size());

    // technically the packet only reaches us after the header delay,
    // and typically we also need to deserialise any payload (unless
    // the two sides of the bridge are synchronous)
    Tick receive_delay = pkt->headerDelay + pkt->payloadDelay;
    pkt->headerDelay = pkt->payloadDelay = 0;

    responsePort.schedTimingResp(pkt, bridge.clockEdge(delay) + receive_delay);

    return true;
}

bool
PciUpDownBridge::UpDownBridgeResponsePort::recvTimingReq(PacketPtr pkt)
{
    DPRINTF(PciUpDownBridge, "recvTimingReq: %s addr 0x%x\n", pkt->cmdString(),
            pkt->getAddr());

    panic_if(pkt->cacheResponding(), "Should not see packets where cache "
                                     "is responding");

    // we should not get a new request after committing to retry the
    // current one, but unfortunately the CPU violates this rule, so
    // simply ignore it for now
    if (retryReq)
        return false;

    DPRINTF(PciUpDownBridge, "Response queue size: %d outresp: %d\n",
            transmitList.size(), outstandingResponses);

    bool configError = isConfigError(pkt->getAddr());

    // if the request queue is full then there is no hope, unless it is a
    // configuration error which is directly responded.
    if (requestPort.reqQueueFull() && !configError) {
        DPRINTF(PciUpDownBridge, "Request queue full\n");
        retryReq = true;
    } else {
        // look at the response queue if we expect to see a response
        bool expects_response = pkt->needsResponse();
        if (expects_response) {
            if (respQueueFull()) {
                DPRINTF(PciUpDownBridge, "Response queue full\n");
                retryReq = true;
            } else {
                // ok to send the request with space for the response
                DPRINTF(PciUpDownBridge, "Reserving space for response\n");
                assert(outstandingResponses != respQueueLimit);
                ++outstandingResponses;

                // no need to set retryReq to false as this is already the
                // case
            }
        }

        if (!retryReq) {
            // technically the packet only reaches us after the header
            // delay, and typically we also need to deserialise any
            // payload (unless the two sides of the bridge are
            // synchronous)
            Tick receive_delay = pkt->headerDelay + pkt->payloadDelay;
            pkt->headerDelay = pkt->payloadDelay = 0;

            if (configError) {
                if (pkt->isRead()) {
                    // Response with PCI error code
                    uint8_t *pkt_data(pkt->getPtr<uint8_t>());
                    std::fill(pkt_data, pkt_data + pkt->getSize(), 0xFF);
                }

                pkt->makeTimingResponse();

                schedTimingResp(pkt, bridge.clockEdge(delay) + receive_delay);
            } else {
                requestPort.schedTimingReq(pkt, bridge.clockEdge(delay) +
                                                    receive_delay);
            }
        }
    }

    // remember that we are now stalling a packet and that we have to
    // tell the sending requestor to retry once space becomes available,
    // we make no distinction whether the stalling is due to the
    // request queue or response queue being full
    return !retryReq;
}

void
PciUpDownBridge::UpDownBridgeResponsePort::retryStalledReq()
{
    if (retryReq) {
        DPRINTF(PciUpDownBridge, "Request waiting for retry, now retrying\n");
        retryReq = false;
        sendRetryReq();
    }
}

void
PciUpDownBridge::UpDownBridgeRequestPort::schedTimingReq(PacketPtr pkt,
                                                         Tick when)
{
    // If we're about to put this packet at the head of the queue, we
    // need to schedule an event to do the transmit.  Otherwise there
    // should already be an event scheduled for sending the head
    // packet.
    if (transmitList.empty()) {
        bridge.schedule(sendEvent, when);
    }

    assert(transmitList.size() != reqQueueLimit);

    transmitList.emplace_back(pkt, when);
}

void
PciUpDownBridge::UpDownBridgeResponsePort::schedTimingResp(PacketPtr pkt,
                                                           Tick when)
{
    // If we're about to put this packet at the head of the queue, we
    // need to schedule an event to do the transmit.  Otherwise there
    // should already be an event scheduled for sending the head
    // packet.
    if (transmitList.empty()) {
        bridge.schedule(sendEvent, when);
    }

    transmitList.emplace_back(pkt, when);
}

void
PciUpDownBridge::UpDownBridgeRequestPort::trySendTiming()
{
    assert(!transmitList.empty());

    DeferredPacket req = transmitList.front();

    assert(req.tick <= curTick());

    PacketPtr pkt = req.pkt;

    DPRINTF(PciUpDownBridge, "trySend request addr 0x%x, queue size %d\n",
            pkt->getAddr(), transmitList.size());

    if (sendTimingReq(pkt)) {
        // send successful
        transmitList.pop_front();
        DPRINTF(PciUpDownBridge, "trySend request successful\n");

        // If there are more packets to send, schedule event to try again.
        if (!transmitList.empty()) {
            DeferredPacket next_req = transmitList.front();
            DPRINTF(PciUpDownBridge, "Scheduling next send\n");
            bridge.schedule(sendEvent,
                            std::max(next_req.tick, bridge.clockEdge()));
        }

        // if we have stalled a request due to a full request queue,
        // then send a retry at this point, also note that if the
        // request we stalled was waiting for the response queue
        // rather than the request queue we might stall it again
        responsePort.retryStalledReq();
    }

    // if the send failed, then we try again once we receive a retry,
    // and therefore there is no need to take any action
}

void
PciUpDownBridge::UpDownBridgeResponsePort::trySendTiming()
{
    assert(!transmitList.empty());

    DeferredPacket resp = transmitList.front();

    assert(resp.tick <= curTick());

    PacketPtr pkt = resp.pkt;

    DPRINTF(PciUpDownBridge, "trySend response addr 0x%x, outstanding %d\n",
            pkt->getAddr(), outstandingResponses);

    if (sendTimingResp(pkt)) {
        // send successful
        transmitList.pop_front();
        DPRINTF(PciUpDownBridge, "trySend response successful\n");

        assert(outstandingResponses != 0);
        --outstandingResponses;

        // If there are more packets to send, schedule event to try again.
        if (!transmitList.empty()) {
            DeferredPacket next_resp = transmitList.front();
            DPRINTF(PciUpDownBridge, "Scheduling next send\n");
            bridge.schedule(sendEvent,
                            std::max(next_resp.tick, bridge.clockEdge()));
        }

        // if there is space in the request queue and we were stalling
        // a request, it will definitely be possible to accept it now
        // since there is guaranteed space in the response queue
        if (!requestPort.reqQueueFull() && retryReq) {
            DPRINTF(PciUpDownBridge,
                    "Request waiting for retry, now retrying\n");
            retryReq = false;
            sendRetryReq();
        }
    }

    // if the send failed, then we try again once we receive a retry,
    // and therefore there is no need to take any action
}

void
PciUpDownBridge::UpDownBridgeRequestPort::recvReqRetry()
{
    trySendTiming();
}

void
PciUpDownBridge::UpDownBridgeResponsePort::recvRespRetry()
{
    trySendTiming();
}

Tick
PciUpDownBridge::UpDownBridgeResponsePort::recvAtomic(PacketPtr pkt)
{
    panic_if(pkt->cacheResponding(), "Should not see packets where cache "
                                     "is responding");

    if (isConfigError(pkt->getAddr())) {
        if (pkt->isRead()) {
            // Response with PCI error code
            uint8_t *pkt_data(pkt->getPtr<uint8_t>());
            std::fill(pkt_data, pkt_data + pkt->getSize(), 0xFF);
        }

        pkt->makeAtomicResponse();

        return delay * bridge.clockPeriod();
    }

    return delay * bridge.clockPeriod() + requestPort.sendAtomic(pkt);
}

Tick
PciUpDownBridge::UpDownBridgeResponsePort::recvAtomicBackdoor(
    PacketPtr pkt, MemBackdoorPtr &backdoor)
{
    if (isConfigError(pkt->getAddr())) {
        return delay * bridge.clockPeriod();
    }

    return delay * bridge.clockPeriod() +
           requestPort.sendAtomicBackdoor(pkt, backdoor);
}

void
PciUpDownBridge::UpDownBridgeResponsePort::recvFunctional(PacketPtr pkt)
{
    pkt->pushLabel(name());

    // check the response queue
    for (auto i = transmitList.begin(); i != transmitList.end(); ++i) {
        if (pkt->trySatisfyFunctional((*i).pkt)) {
            pkt->makeResponse();
            return;
        }
    }

    if (isConfigError(pkt->getAddr())) {
        if (pkt->isRead()) {
            // Response with PCI error code
            uint8_t *pkt_data(pkt->getPtr<uint8_t>());
            std::fill(pkt_data, pkt_data + pkt->getSize(), 0xFF);
        }

        pkt->makeResponse();

        return;
    }

    // also check the request port's request queue
    if (requestPort.trySatisfyFunctional(pkt)) {
        return;
    }

    pkt->popLabel();

    // fall through if pkt still not satisfied
    requestPort.sendFunctional(pkt);
}

void
PciUpDownBridge::UpDownBridgeResponsePort::recvMemBackdoorReq(
    const MemBackdoorReq &req, MemBackdoorPtr &backdoor)
{
    if (isConfigError(req.range().start())) {
        return;
    }

    requestPort.sendMemBackdoorReq(req, backdoor);
}

bool
PciUpDownBridge::UpDownBridgeRequestPort::trySatisfyFunctional(PacketPtr pkt)
{
    bool found = false;
    auto i = transmitList.begin();

    while (i != transmitList.end() && !found) {
        if (pkt->trySatisfyFunctional((*i).pkt)) {
            pkt->makeResponse();
            found = true;
        }
        ++i;
    }

    return found;
}

AddrRangeList
PciUpDownBridge::UpDownBridgeResponsePort::getAddrRanges() const
{
    return ranges;
}

AddrRangeList
PciUpDownBridge::UpSideResponsePort::getAddrRanges() const
{
    AddrRange configRange = bridge.upstream->getConfigAddrRange();
    AddrRangeList filteredRanges{ configRange };

    // Create a range list with the full configuration range and all
    // PCI side ranges without collision on configuration ranges.
    for (const AddrRange &r : ranges) {
        if (!r.isSubset(configRange)) {
            filteredRanges.push_back(r);
        }
    }

    return filteredRanges;
}

bool
PciUpDownBridge::UpSideResponsePort::isConfigError(Addr addr) const
{
    // Do not return an error for address that aren't configuration.
    if (!bridge.upstream->getConfigAddrRange().contains(addr)) {
        return false;
    }

    AddrRangeList ranges = requestPort.getAddrRanges();

    // A config error occurs only if none of ranges contains the address on the
    // PCI side.
    return std::none_of(
        ranges.cbegin(), ranges.cend(),
        [addr](AddrRange const &range) { return range.contains(addr); });
}

bool
PciUpDownBridge::DownSideResponsePort::isConfigError(Addr addr) const
{
    // Treat all configuration address as an error to not let the packet pass
    // to the memory side.
    return bridge.upstream->getConfigAddrRange().contains(addr);
}

void
PciUpDownBridge::DownSideRequestPort::recvRangeChange()
{
    AddrRangeList downRanges = getAddrRanges();

    // Avoid potential loop of range change.
    if (downRanges != responsePort.ranges) {
        responsePort.ranges = downRanges;
        responsePort.sendRangeChange();
    }
}

} // namespace gem5
