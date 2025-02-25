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

#ifndef __DEV_PCI_HOST_BRIDGE_HH__
#define __DEV_PCI_HOST_BRIDGE_HH__

#include "mem/port.hh"
#include "params/PciHostBridge.hh"
#include "sim/clocked_object.hh"

namespace gem5
{

class PciHost;

/**
 * The PCI host bridge is responsible to bridge memory packets between the
 * system bus and the PCI main bus as well as delivering PCI interrupts to
 * the CPU. A host bridge is essentially a two way bridge.
 *
 * A PciHost must be associated with the bridge via PciHostBridge::setHost().
 * The range provided by PciHost::getConfigAddrRange() will be bridged from
 * the system bus to the PCI bus, if a device exists for it. Otherwise, the
 * bridge will response with the PCI error code.
 *
 * The bridge should placed on default port of the PCI bus to forward DMA
 * packets to the system bus. It will forward all memory packet from system bus
 * to PCI bus for addresses that are known on the PCI bus (via
 * RequestPort::recvRangeChange()).
 */
class PciHostBridge : public ClockedObject
{
  private:
    /**
     * A deferred packet stores a packet along with its scheduled
     * transmission time
     */
    class DeferredPacket
    {
      public:
        const Tick tick;
        const PacketPtr pkt;

        DeferredPacket(PacketPtr _pkt, Tick _tick) : tick(_tick), pkt(_pkt) {}
    };

    // Forward declaration to allow the response port to have a pointer
    class HostBridgeRequestPort;

    /**
     * The response port base to be used on both side of the bridge.
     *
     * The response port has a set of address ranges that it
     * is responsible for. The response port also has a buffer for the
     * responses not yet sent.
     */
    class HostBridgeResponsePort : public ResponsePort
    {
        friend PciHostBridge;

      protected:
        /** The PCI host bridge to which this port belongs. */
        PciHostBridge &bridge;

        /** Request port on the other side of the bridge. */
        HostBridgeRequestPort &requestPort;

        /** Minimum request delay though this bridge. */
        const Cycles delay;

        /** Address ranges to pass through the bridge */
        AddrRangeList ranges;

        /**
         * Response packet queue. Response packets are held in this
         * queue for a specified delay to model the processing delay
         * of the bridge. We use a deque as we need to iterate over
         * the items for functional accesses.
         */
        std::deque<DeferredPacket> transmitList;

        /** Counter to track the outstanding responses. */
        unsigned int outstandingResponses;

        /** If we should send a retry when space becomes available. */
        bool retryReq;

        /** Max queue size for reserved responses. */
        unsigned int respQueueLimit;

        /**
         * Upstream caches need this packet until true is returned, so
         * hold it for deletion until a subsequent call
         */
        std::unique_ptr<Packet> pendingDelete;

        /**
         * Is this side blocked from accepting new response packets.
         *
         * @return true if the reserved space has reached the set limit
         */
        bool respQueueFull() const;

        /**
         * Handle send event, scheduled when the packet at the head of
         * the response queue is ready to transmit (for timing
         * accesses only).
         */
        void trySendTiming();

        /** Send event for the response queue. */
        EventFunctionWrapper sendEvent;

      public:
        /**
         * Constructor for the HostBridgeResponsePort.
         *
         * @param _name the port name including the owner
         * @param _bridge the structural owner
         * @param _memSidePort the request port on the other
         *                       side of the bridge
         * @param _delay the delay in cycles from receiving to sending
         * @param _resp_limit the size of the response queue
         */
        HostBridgeResponsePort(const std::string &_name,
                               PciHostBridge &_bridge,
                               HostBridgeRequestPort &_requestPort,
                               Cycles _delay, int _resp_limit);

        /**
         * Queue a response packet to be sent out later and also schedule
         * a send if necessary.
         *
         * @param pkt a response to send out after a delay
         * @param when tick when response packet should be sent
         */
        void schedTimingResp(PacketPtr pkt, Tick when);

        /**
         * Retry any stalled request that we have failed to accept at
         * an earlier point in time. This call will do nothing if no
         * request is waiting.
         */
        void retryStalledReq();

      protected:
        /** When receiving a timing request from the peer port,
            pass it to the bridge. */
        bool recvTimingReq(PacketPtr pkt) override;

        /** When receiving a retry request from the peer port,
            pass it to the bridge. */
        void recvRespRetry() override;

        /** When receiving an Atomic request from the peer port,
            pass it to the bridge. */
        Tick recvAtomic(PacketPtr pkt) override;

        /** When receiving an Atomic backdoor request from the peer port,
            pass it to the bridge. */
        Tick recvAtomicBackdoor(PacketPtr pkt,
                                MemBackdoorPtr &backdoor) override;

        /** When receiving a Functional request from the peer port,
            pass it to the bridge. */
        void recvFunctional(PacketPtr pkt) override;

        /** When receiving a Functional backdoor request from the peer port,
            pass it to the bridge. */
        void recvMemBackdoorReq(const MemBackdoorReq &req,
                                MemBackdoorPtr &backdoor) override;

        /** When receiving a address range request the peer port,
            pass it to the bridge. */
        AddrRangeList getAddrRanges() const override;

        /** Check if a given address access is made to the configuration of a
            unkown device */
        virtual bool isConfigError(Addr addr) const = 0;
    };

    /**
     * The request port base that to be used on both side of the bridge.
     * They will be linked to the corresponding reponse port to provide the
     * two way bridge. The request port has a buffer for the requests not yet
     * sent.
     */
    class HostBridgeRequestPort : public RequestPort
    {
      protected:
        /** The bridge to which this port belongs. */
        PciHostBridge &bridge;

        /** The response port on the other side of the bridge. */
        HostBridgeResponsePort &responsePort;

      private:
        /** Minimum delay though this bridge. */
        const Cycles delay;

        /**
         * Request packet queue. Request packets are held in this
         * queue for a specified delay to model the processing delay
         * of the bridge.  We use a deque as we need to iterate over
         * the items for functional accesses.
         */
        std::deque<DeferredPacket> transmitList;

        /** Max queue size for request packets */
        const unsigned int reqQueueLimit;

        /**
         * Handle send event, scheduled when the packet at the head of
         * the outbound queue is ready to transmit (for timing
         * accesses only).
         */
        void trySendTiming();

        /** Send event for the request queue. */
        EventFunctionWrapper sendEvent;

      public:
        /**
         * Constructor for the HostBridgeRequestPort.
         *
         * @param _name the port name including the owner
         * @param _bridge the structural owner
         * @param _cpuSidePort the response port on the other side of
         * the bridge
         * @param _delay the delay in cycles from receiving to sending
         * @param _req_limit the size of the request queue
         */
        HostBridgeRequestPort(const std::string &_name, PciHostBridge &_bridge,
                              HostBridgeResponsePort &_responsePort,
                              Cycles _delay, int _req_limit);

        /**
         * Is this side blocked from accepting new request packets.
         *
         * @return true if the occupied space has reached the set limit
         */
        bool reqQueueFull() const;

        /**
         * Queue a request packet to be sent out later and also schedule
         * a send if necessary.
         *
         * @param pkt a request to send out after a delay
         * @param when tick when response packet should be sent
         */
        void schedTimingReq(PacketPtr pkt, Tick when);

        /**
         * Check a functional request against the packets in our
         * request queue.
         *
         * @param pkt packet to check against
         *
         * @return true if we find a match
         */
        bool trySatisfyFunctional(PacketPtr pkt);

      protected:
        /** When receiving a timing request from the peer port,
            pass it to the bridge. */
        bool recvTimingResp(PacketPtr pkt) override;

        /** When receiving a retry request from the peer port,
            pass it to the bridge. */
        void recvReqRetry() override;
    };

    /**
     * Specific implementation for the memory side response port.
     */
    class MemSideResponsePort : public HostBridgeResponsePort
    {
      public:
        /**
         * Constructor for the MemSideResponsePort.
         *
         * @param _name the port name including the owner
         * @param _bridge the structural owner
         * @param _cpuSidePort the response port on the other side of
         * the bridge
         * @param _delay the delay in cycles from receiving to sending
         * @param _req_limit the size of the Response queue
         */
        MemSideResponsePort(const std::string &_name, PciHostBridge &_bridge,
                            HostBridgeRequestPort &_responsePort,
                            Cycles _delay, int _req_limit)
            : HostBridgeResponsePort(_name, _bridge, _responsePort, _delay,
                                     _req_limit)
        {}

      protected:
        /** When receiving a address range request the peer port,
            pass it to the bridge. */
        AddrRangeList getAddrRanges() const override;

        /** Check if a given address access is made to the configuration of a
            unkown device */
        bool isConfigError(Addr addr) const override;
    };

    /**
     * Specific implementation for the PCI side response port.
     */
    class PciSideResponsePort : public HostBridgeResponsePort
    {
      public:
        /**
         * Constructor for the PciSideResponsePort.
         *
         * @param _name the port name including the owner
         * @param _bridge the structural owner
         * @param _cpuSidePort the response port on the other side of
         * the bridge
         * @param _delay the delay in cycles from receiving to sending
         * @param _req_limit the size of the Response queue
         */
        PciSideResponsePort(const std::string &_name, PciHostBridge &_bridge,
                            HostBridgeRequestPort &_responsePort,
                            Cycles _delay, int _req_limit)
            : HostBridgeResponsePort(_name, _bridge, _responsePort, _delay,
                                     _req_limit)
        {}

      protected:
        /** Check if a given address access is made to the configuration of a
            unkown device */
        bool isConfigError(Addr addr) const override;
    };

    /**
     * Specific implementation for the PCI side request port.
     */
    class PciSideRequestPort : public HostBridgeRequestPort
    {
      public:
        /**
         * Constructor for the PciSideRequestPort.
         *
         * @param _name the port name including the owner
         * @param _bridge the structural owner
         * @param _cpuSidePort the response port on the other side of
         * the bridge
         * @param _delay the delay in cycles from receiving to sending
         * @param _req_limit the size of the request queue
         */
        PciSideRequestPort(const std::string &_name, PciHostBridge &_bridge,
                           HostBridgeResponsePort &_responsePort,
                           Cycles _delay, int _req_limit)
            : HostBridgeRequestPort(_name, _bridge, _responsePort, _delay,
                                    _req_limit)
        {}

      protected:
        void recvRangeChange() override;
    };

  protected:
    // Bridge PCI -> Memory
    HostBridgeRequestPort memRequestPort;
    PciSideResponsePort pciResponsePort;

    // Bridge memory -> PCI
    PciSideRequestPort pciRequestPort;
    MemSideResponsePort memResponsePort;

    PciHost *host;

  public:
    Port &getPort(const std::string &if_name,
                  PortID idx = InvalidPortID) override;

    void init() override;

    void
    setHost(PciHost *host)
    {
        this->host = host;
    }

    PciHostBridge(const PciHostBridgeParams &p);
    virtual ~PciHostBridge();
};

} // namespace gem5

#endif //__DEV_PCI_HOST_BRIDGE_HH__
