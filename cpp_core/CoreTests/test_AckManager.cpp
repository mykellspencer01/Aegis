/*
    * Unit tests for sliding-window trACKer

    * Tests:
        - TrackSent records a packet and returns it in pending retransmits after timeout
        - ProcessAck clears correct slot and removes it from pending retransmits
        - Duplicate detection returns true for already-rcvd sequences
        - Remote ack/ackBits accessors return correct value after RecordReceived calls
        - Sequence $ wraparound at 65535 -> 0 is handled correctly
        - Exponential back-off : retransmit count increases and deadline doubles
        - Reset clears all states
*/

#include <gtest/gtest.h>
#include "../../Transport/AckManager.h"

using namespace NetEngine::Transport;

//! Track Sent & Pending Retransmits

TEST(AckManager, TrackSent_AppearsInRetransmits)
{
    AckManager mgr;
    uint8_t data[4] = {1, 2, 3, 4};

    //send at time 0.0, check at time 1.0 w/ a 0.1 threshold
    uint16_t seq = mgr.TrackSent(data, sizeof(data), 0.0);

    auto pending = mgr.GetPendingRetransmits(1.0, 0.1);
    ASSERT_EQ(pending.size(), 1u);
    EXPECT_EQ(pending[0].sequence, seq);
    EXPECT_EQ(pending[0].dataLength, 4u);
}

TEST(AckManager, TrackSent_DoesNotAppearBeforeThreshold)
{
    AckManager mgr;
    uint8_t data[4] = {0xAA, 0xBB, 0xCC, 0xDD};

    //Sent a 0.0, checked at 0.05
    mgr.TrackSent(data, sizeof(data), 0.0);
    auto pending = mgr.GetPendingRetransmits(0.05, 0.1);
    EXPECT_TRUE(pending.empty());
}

//! ProcessAck Clears the slot

TEST(AckManager, ProcessAck_ClearsSlot)
{
    AckManager mgr;
    uint8_t data[4] = {1, 2, 3, 4};
    uint16_t seq = mgr.TrackSent(data, sizeof(data), 0.0);

    //Confirm pending
    ASSERT_EQ(mgr.GetPendingRetransmits(1.0, 0.1).size(), 1u);

    //clear slot
    mgr.ProcessAck(seq, 0);
    EXPECT_TRUE(mgr.GetPendingRetransmits(2.0, 0.1).empty());
}

TEST(AckManager, ProcessAck_AckBitsMaskClears)
{
    AckManager mgr;
    uint8_t data[4] = {1, 2, 3, 4};

    //Send 2 packets
    uint16_t seq0 = mgr.TrackSent(data, sizeof(data), 0.0);
    uint16_t seq1 = mgr.TrackSent(data, sizeof(data), 0.0);

    mgr.ProcessAck(seq1, 0x0001);
    EXPECT_TRUE(mgr.GetPendingRetransmits(2.0, 0.1).empty());
}

//! Duplicate Detection

TEST(AckManager, IsDuplicate_ReturnsTrueForReceived)
{
    AckManager mgr;
    mgr.RecordReceived(42);
    EXPECT_TRUE(mgr.IsDuplicate(42));
}

TEST(AckManager, IsDuplicate_ReturnsFalseForUnreceived)
{
    AckManager mgr;
    mgr.RecordReceived(42);
    EXPECT_FALSE(mgr.IsDuplicate(41));
    EXPECT_FALSE(mgr.IsDuplicate(43));
}

//! Remote Ack Accessors

TEST(AckManager, RemoteAck_UpdatesToHighestReceived)
{
    AckManager mgr;
    mgr.RecordReceived(10);
    mgr.RecordReceived(11);
    mgr.RecordReceived(13); // gap at 12
    EXPECT_EQ(mgr.GetRemoteAck(), 13u);
}

TEST(AckManager, RemoteAckBits_ReflectsReceivedHistory)
{
    AckManager mgr;
    mgr.RecordReceived(10);
    mgr.RecordReceived(11); // ack = 11, bit0 = 1. seq10 rcvd

    uint16_t bits = mgr.GetRemoteAckBits();
    EXPECT_TRUE(bits & 0x0001); // bit 0 = (ack - 1) = 10, which was rcvd
}

//! Sequence Wraparound

TEST(AckManager, SequenceWrapAround_HandledCorrectly)
{
    AckManager mgr;
    mgr.RecordReceived(65535);
    mgr.RecordReceived(0); // wraps around

    //After wraparound, 0 should be most recent ack
    EXPECT_EQ(mgr.GetRemoteAck(), 0u);

    //65535 should be recorded in ackbits (ack - 1 = 0 - 1 = 65535)
    EXPECT_TRUE(mgr.GetRemoteAckBits() & 0x0001);

    // Neither should be a duplicate of 1
    EXPECT_FALSE(mgr.IsDuplicate(1));
}

//! Reset and Clear

TEST(AckManager, Reset_ClearsAllStates)
{
    AckManager mgr;
    uint8_t data[4] = {1, 2, 3, 4};
    mgr.TrackSent(data, sizeof(data), 0.0);
    mgr.RecordReceived(5);

    mgr.Reset();

    //After reset: no pending retransmits, remote ack back to 0
    EXPECT_TRUE(mgr.GetPendingRetransmits(1.0, 0.1).empty());
    EXPECT_EQ(mgr.GetRemoteAck(), 0u);
    EXPECT_EQ(mgr.GetNextOutSequence(), 0u);
}