/*
    These tests open two ReliableChannel instances on localhost loopback ports; 
    exercises UDP I/O between them

    * Tests
        - Init returns ok and assigns a local port
        - Double-init returns AlreadyInitialized
        - Send to unknown peer returns UnknownPeer
        - Unreliable packet (channel 0) travels from A to B and a callback fires
        - Payload bytes rcvd intact
        - Sender's peer ID is correctly reported to reciever
        - Multiple packets in the same tick are all rcvd
        - Shutdown cleans up w/o crashing
*/

#include <gtest/gtest.h>
#include "../Transport/ReliableChannel.h"
#include <thread>
#include <chrono>
#include <atomic>
#include <cstring>

using namespace Aegis::Transport;
using namespace Aegis;

static constexpr int LOOPBACK_WAIT_MS = 15; //ms to wait for loopback delivery before calling Tick

// * Helper to build a pair of connected channels on localhost
//caller is responsible for shutting down

static void MakeLoopbackPair(ReliableChannel& a_chan, ReliableChannel& b_chan,
    PacketReceivedCallback cbA, PacketReceivedCallback cbB)
{
    ASSERT_EQ(a_chan.Init(0, cbA), NetResult::Ok); //port 0 - OS assigned
    ASSERT_EQ(b_chan.Init(0, cbB), NetResult::Ok); 

    RemoteEndpoint epA {}, epB {};
    strncpy(epA.ip, "127.0.0.1", sizeof(epA.ip) - 1);
    epA.port = a_chan.GetLocalPort();
    strncpy(epB.ip, "127.0.0.1", sizeof(epB.ip) - 1);
    epB.port = b_chan.GetLocalPort();

    //A knows about B (peerID = 2) and B knows about A (peerID = 1)
    ASSERT_EQ(a_chan.AddPeer(2, epB), NetResult::Ok);
    ASSERT_EQ(b_chan.AddPeer(1, epA), NetResult::Ok);
}

//! Init

TEST(ReliableChannel, Init_ReturnsOk)
{
    ReliableChannel ch;
    EXPECT_EQ(ch.Init(0, [](const IncomingPacket&){}), NetResult::Ok);
    EXPECT_GT(ch.GetLocalPort(), 0u);
    ch.Shutdown();
}

TEST(ReliableChannel, Init_DoubleInit_ReturnAlreadyInitialized)
{
    ReliableChannel ch;
    ch.Init(0, [](const IncomingPacket&){});
    EXPECT_EQ(ch.Init(0, [](const IncomingPacket& ){}), NetResult::AlreadyInitialized);
    ch.Shutdown();
}

//! Send to Unknown Peer

TEST(ReliableChannel, Send_UnknownPeer_ReturnsUnknownPeer)
{
    ReliableChannel ch;
    ch.Init(0, [](const IncomingPacket&){});

    const uint8_t data[4] = {1, 2, 3, 4};
    EXPECT_EQ(ch.Send(999, 0, data, sizeof(data)), NetResult::UnknownPeer);
    ch.Shutdown();
}

//! Unreliable loopback send/receive

TEST(ReliableChannel, Loopback_UnreliableSend_CallbackFires)
{
    ReliableChannel a_chan, b_chan;
    bool received = false;

    MakeLoopbackPair(a_chan, b_chan,[](const IncomingPacket&){}, //A receives nothing in this test
        [&](const IncomingPacket&) { received = true; });
    
    const uint8_t msg[4] = {0x12, 0x34, 0x56, 0x78};
    ASSERT_EQ(a_chan.Send(2, 0, msg, sizeof(msg)), NetResult::Ok);

    std::this_thread::sleep_for(std::chrono::milliseconds(LOOPBACK_WAIT_MS));
    b_chan.Tick(0.016);

    EXPECT_TRUE(received);
    a_chan.Shutdown();
    b_chan.Shutdown();
}

//! Regression: every pakcet used to carry seq 0, rcvr dropped all but the first as dupes

TEST(ReliableChannel, ManySequentialPackets_AllDelivered)
{
    ReliableChannel a_chan, b_chan;
    std::atomic<int> received { 0 };
    MakeLoopbackPair(a_chan, b_chan, [](const IncomingPacket&) {}, [&](const IncompingPacket&) { ++received; });

    const int kCount = 50;
    uint8_t payload[16] = { 7 };
    for(int i = 0; i < kCount; ++i)
    {
        ASSERT_EQ(a_chan.Send(2, 0, payload, sizeof(payload)), NetResult::Ok);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        b.Tick(i * 0.01);
    }
    EXPECT_EQ(received.load(), kCount);
    a_chan.Shutdown(); b_chan.Shutdown();
}