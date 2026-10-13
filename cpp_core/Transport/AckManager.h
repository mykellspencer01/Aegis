/*
     * Summary: Sliding-window trACKer for reliable UDP delivery

     * ACK Manager maintains two-bit arrays per peer:
        - A "sent" window recording which seq. nums have been sent and are awaiting ACKs
        - A "received" window recording which seq. nums have been most recently seen
            + Used to build ackBits field in the outgoing packet headers 
            + remote slide learns which of its packets has been receiveed
*/

#pragma once

#include "../EngineTypes.h"
#include <array>
#include <cstdint>
#include <vector>

namespace Aegis {
namespace Transport {

// # of seq. nums tracked simultaneuously per direction
constexpr uint32_t ACK_WINDOW_SIZE = 256;

struct PendingPacket
{
    uint32_t dataLength; // Number of valid bytes in data
    uint16_t sequence; // seq. num of packet
    uint8_t data[NETENGINE_MAX_PACKET_SIZE]; // Full serialized packet bytes
    uint8_t retransmitCount; // # of times the packet has been re-sent
    double sentTimestamp; // Seconds since engine start

};

class AckManager // * One instance per user
{
    public:
        AckManager();

        // * Outgoing Side
        uint16_t TrackSent(const uint8_t* packetData, uint32_t length, double timeStamp);
        void ProcessAck(uint16_t ack, uint16_t ackBits);
        std::vector<PendingPacket> GetPendingRetransmits(double currentTime, double rttThreshold) const;
        
        // * Incoming Side
        void RecordReceived(uint16_t sequence);
        void AdvanceOutSequence(); //consume a seq # for a packet that does not need retransmit tracking
        bool IsDuplicate(uint16_t sequence) const;

        // * Accessors for building outgoing packet headers
        uint16_t GetRemoteAck() const;
        uint16_t GetRemoteAckBits() const;
        uint16_t GetNextOutSequence() const;

        void Reset();

    private:
        // * Internal Helpers
        //Wraps a sequence comparison to handle 16-bit rollover
        static bool SequenceIsNewer(uint16_t seq, uint16_t reference);

        // * State
        uint16_t t_nextOutSequence; //next seq. num to assign to an outgoing packet
        uint16_t t_remoteAck; //highest remote seq. received
        uint16_t t_remoteAckBits; //bitmask of the 16 seqs before t_remoteAck
        bool t_hasReceivedAny = false;

        // * Circular buffer of sent-but-unacked packets
        std::array<PendingPacket, ACK_WINDOW_SIZE> t_pendingPackets;
        // * Bitset recording which pending slots are occupied (1 = in-flight)
        std::array<bool, ACK_WINDOW_SIZE> t_slotOccupied;
};

} //namespace transport
} //namespace Aegis