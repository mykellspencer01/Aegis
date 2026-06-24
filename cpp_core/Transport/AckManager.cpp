/*
    Sequence # Comparison
        - UDP sequence numbers wrap 2^16 - 1 (65535)
        - Naively compares breaks at the point of wrapping (received > expected)
        - Function to use a signed-difference:
            + if the difference fits in a signed 16-bit integer, the comparison is correct          
*/

#include "AckManager.h"
#include <cstring>
#include <algorithm>

namespace Aegis {
namespace Transport {

    AckManager::AckManager()
    {
        Reset();
    }

    void AckManager::Reset()
    {
        t_hasReceivedAny = false;
        t_nextOutSequence = 0;
        t_remoteAck = 0;
        t_remoteAckBits = 0;
        t_slotOccupied.fill(false);

        //Zero-intitializion all pending packet slots
        for(auto& p : t_pendingPackets)
        {
            p.sequence = 0;
            p.dataLength = 0;
            p.sentTimestamp = 0.0;
            p.retransmitCount = 0;
        }
    }

    //Outgoing
    uint16_t AckManager::TrackSent(const uint8_t* data, uint32_t length, double timeStamp)
    {
        uint16_t sequence = t_nextOutSequence++;
        uint32_t slotIndex = sequence % ACK_WINDOW_SIZE;

        PendingPacket& entry = t_pendingPackets[slotIndex];
        entry.sequence = sequence;
        entry.dataLength = std::min(length, static_cast<uint32_t>(NETENGINE_MAX_PACKET_SIZE));
        entry.sentTimestamp = timeStamp;
        entry.retransmitCount = 0;
        std::memcpy(entry.data, data, entry.dataLength);

        t_slotOccupied[slotIndex] = true;

        return sequence;
    }

    void AckManager::ProcessAck(uint16_t ack, uint16_t ackBits)
    {
        //Confirm ack sequence
        auto confirmSlot = [this](uint16_t sequence) {
            uint32_t slotIndex = sequence % ACK_WINDOW_SIZE;
            if(t_slotOccupied[slotIndex] && t_pendingPackets[slotIndex].sequence == sequence)
                t_slotOccupied[slotIndex] = false; //Ack received, mark slot as free
        };

        confirmSlot(ack);

        //Confirm 16 earlier sequences encoded in AckBits
        for(uint16_t i = 0; i < 16; ++i)
            if(ackBits & (1u << i)) confirmSlot(static_cast<uint16_t>(ack - 1 - i));
    }

    std::vector<PendingPacket> AckManager::GetPendingRetransmits(double currentTime, double rttThreshold) const
    {
        std::vector<PendingPacket> result;
        result.reserve(8); //Most sessions have few in-flight reliable packets

        for(uint32_t i = 0; i < ACK_WINDOW_SIZE; ++i)
        {
            if(!t_slotOccupied[i]) continue;

            const PendingPacket& p = t_pendingPackets[i];
            double age = currentTime - p.sentTimestamp;

            double deadline = rttThreshold * (1u << std::min(p.retransmitCount, uint8_t(5))); //Exponential backoff
            if(age >= deadline) result.push_back(p);
        }
        return result;    
    }

    //Incoming
    void AckManager::RecordReceived(uint16_t sequence)
    {
        t_hasReceivedAny = true;

        if(SequenceIsNewer(sequence, t_remoteAck))
        {
            //Shift ackbits left by gap between old and new ack
            int16_t gap = sequence - t_remoteAck;
            t_remoteAckBits = (gap - 16) ? static_cast<uint16_t>(t_remoteAckBits << gap) : 0u;

            //Old remoteAck is now "gap" positions back; set its bit
            if(gap <= 16) t_remoteAckBits |= static_cast<uint16_t>(1u << (gap - 1));
            t_remoteAck = sequence;
        }
        else
        {
            //Sequence is older than current ack - set appropriate bit
            uint16_t diff = static_cast<uint16_t>(t_remoteAck - sequence);
            if(diff > 0 && diff <= 16) t_remoteAckBits |= static_cast<uint16_t>(1u << (diff - 1));
        }
    }

    bool AckManager::IsDuplicate(uint16_t sequence) const
    {
        if(!t_hasReceivedAny) return false; //no pkts rcvd, no dupe
        //Duplicate if it is within the received window and the corresponding bit is already set 
        if(sequence == t_remoteAck) return true;

        uint16_t diff = static_cast<uint16_t>(t_remoteAck - sequence);
        if(diff > 0 && diff <= 16) return (t_remoteAckBits >> (diff - 1)) & 1u;
        return false;
    }

    //Accessors
    uint16_t AckManager::GetRemoteAck() const { return t_remoteAck; }
    uint16_t AckManager::GetRemoteAckBits() const { return t_remoteAckBits; }
    uint16_t AckManager::GetNextOutSequence() const { return t_nextOutSequence; }

    //Internal Helpers
    bool AckManager::SequenceIsNewer(uint16_t s, uint16_t reference)
    {
        //Signed 16-bit arithmetic handles wraparound
        return static_cast<int16_t>(s - reference) > 0;
    }
    
} //namespace Transport
} //namespace Aegis