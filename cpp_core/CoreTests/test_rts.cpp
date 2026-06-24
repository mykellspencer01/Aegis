/*
    * Unit tests for RTS module
*/

#include <gtest/gtest.h>
#include "../Genres/RTS/RtsModule.h"
#include "../Serializer/BitRead.h"
#include "../Serializer/BitWrite.h"

using namespace Aegis::Genres;
using namespace Aegis::Serializer;
using namespace Aegis;

// * Helper func to build RPC_RTS_COMMANDS payload
static uint32_t BuildCommandsPayload(
    uint8_t* buffer, uint32_t bufferSize,
    uint32_t turn, const std::vector<RtsCommand>& cmds)
{
    BitWriter w(buffer, bufferSize);
    w.WriteBits(RPC_RTS_COMMANDS, 16);
    w.WriteUInt32(turn);
    w.WriteUInt8(static_cast<uint8_t>(cmds.size()));

    for(auto& c : cmds)
    {
        w.WriteUInt8(c.type);
        w.WriteUInt32(c.targetID);
        w.WriteFloat(c.worldX);
        w.WriteFloat(c.worldZ);
    }
    w.Flush();
    return w.GetBytesWritten();
}

// * Helper to build RPC_RTS_CRC_REPORT payload
static uint32_t BuildCrcPayload(uint8_t* buffer, uint32_t bufferSize, uint32_t turn, uint32_t crc)
{
    BitWriter w(buffer, bufferSize);
    w.WriteBits(RPC_RTS_CRC_REPORT, 16);
    w.WriteUInt32(turn);
    w.WriteUInt32(crc);
    w.Flush();
    return w.GetBytesWritten();
}

//! On Init

TEST(RtsModule, OnInit_ReturnsOk)
{
    RtsModule rts;
    EXPECT_EQ(rts.OnInit(), NetResult::Ok);
}

TEST(RtsModule, OnInit_ResetsCurrentTurn)
{
    RtsModule rts;
    rts.OnInit();
    EXPECT_EQ(rts.GetCurrentTurn(), 0u);
}

//! Turn Callbacks

TEST(RtsModule, TurnCallback_NoFireAfter_OnePeerOfTwo)
{
    RtsModule rts;
    rts.OnInit();
    rts.OnPeerConnected(1);
    rts.OnPeerConnected(2);

    bool fired = false;
    rts.SetTurnReadyCallback([&](const TurnData&) { fired = true; });

    uint8_t buffer[32] {};
    uint32_t length = BuildCommandsPayload(buffer, sizeof(buffer), 1, {});

    BitReader r(buffer, length);
    rts.OnPacketReceived(1, 2, r);
    EXPECT_FALSE(fired);
}

TEST(RtsModule, TurnCallback_FiresWhenAllPeersSubmit)
{
    RtsModule rts;
    rts.OnInit();
    rts.OnPeerConnected(1);
    rts.OnPeerConnected(2);

    bool fired = false;
    rts.SetTurnReadyCallback([&](const TurnData&) { fired = true; });

    uint8_t buffer[32] {};
    uint32_t length;

    length = BuildCommandsPayload(buffer, sizeof(buffer), 1, {});
    BitReader r1(buffer, length);
    rts.OnPacketReceived(1, 2, r1);
    EXPECT_FALSE(fired);

    length = BuildCommandsPayload(buffer, sizeof(buffer), 1, {});
    BitReader r2(buffer, length);
    rts.OnPacketReceived(2, 2, r2);
    EXPECT_TRUE(fired);
}

TEST(RtsModule, TurnCallback_FiresExactlyOnce)
{
    RtsModule rts;
    rts.OnInit();
    rts.OnPeerConnected(1);
    rts.OnPeerConnected(2);

    int fireCount = 0;
    rts.SetTurnReadyCallback([&](const TurnData&) { ++fireCount; });

    uint8_t buffer[32] {};
    uint32_t length;

    length = BuildCommandsPayload(buffer, sizeof(buffer), 1, {});
    BitReader r1(buffer, length);
    rts.OnPacketReceived(1, 2, r1);

    length = BuildCommandsPayload(buffer, sizeof(buffer), 1, {});
    BitReader r2(buffer, length);
    rts.OnPacketReceived(2, 2, r2);

    EXPECT_EQ(fireCount, 1);
}

//! TurnData contains correct command fields

TEST(RtsModule, TurnCallback_ContainsCommandData)
{
    RtsModule rts;
    rts.OnInit();
    rts.OnPeerConnected(1);

    TurnData rcvd;
    rts.SetTurnReadyCallback([&](const TurnData& td) { rcvd = td; });

    RtsCommand cmd{};
    cmd.type = 3;
    cmd.targetID = 7;
    cmd.worldX = 100.0f;
    cmd.worldZ = 100.0f;

    uint8_t buffer[64] {};
    uint32_t length = BuildCommandsPayload(buffer, sizeof(buffer), 1, {cmd});
    BitReader r(buffer, length);
    rts.OnPacketReceived(1, 2, r);

    ASSERT_TRUE(rcvd.commandsByPeer.count(1));
    auto& cmds = rcvd.commandsByPeer.at(1);
    ASSERT_EQ(cmds.size(), 1u);
    EXPECT_EQ(cmds[0].type, 3u);
    EXPECT_EQ(cmds[0].targetID, 7u);
    EXPECT_NEAR(cmds[0].worldX, 100.0f, 0.001f);
    EXPECT_NEAR(cmds[0].worldZ, 100.0f, 0.001f);
}

//! GetCurrentTurn increments after each turn

TEST(RtsModule, GetCurrentTurn_IncrementsAfterTurn)
{
    RtsModule rts;
    rts.OnInit();
    rts.OnPeerConnected(1);
    rts.SetTurnReadyCallback([](const TurnData&){});

    EXPECT_EQ(rts.GetCurrentTurn(), 0u);

    uint8_t buffer[32] {};
    uint32_t length = BuildCommandsPayload(buffer, sizeof(buffer), 1, {});
    BitReader r(buffer, length);
    rts.OnPacketReceived(1, 2, r);

    EXPECT_EQ(rts.GetCurrentTurn(), 1u);
}

//! Desync detection

TEST(RtsModule, DesyncCallback_FiresOnMismatch)
{
    RtsModule rts;
    rts.OnInit();
    rts.OnPeerConnected(1);
    rts.OnPeerConnected(2);

    bool desyncFired = false;
    rts.SetDesyncCallback(
        [&](uint32_t, PeerID, uint32_t, uint32_t) { desyncFired = true; }
    );

    uint8_t buffer[32] {};
    uint32_t length;

    length = BuildCrcPayload(buffer, sizeof(buffer), 1, 0xAAAAAAAA);
    BitReader r1(buffer, length); rts.OnPacketReceived(1, 2, r1);

    length = BuildCrcPayload(buffer, sizeof(buffer), 1, 0xBBBBBBBB);
    BitReader r2(buffer, length); rts.OnPacketReceived(2, 2, r2);

    EXPECT_TRUE(desyncFired);
}

TEST(RtsModule, DesyncCallback_DoesNotFireOnMatch)
{
    RtsModule rts;
    rts.OnInit();
    rts.OnPeerConnected(1);
    rts.OnPeerConnected(2);

    bool desyncFired = false;
    rts.SetDesyncCallback(
        [&](uint32_t, PeerID, uint32_t, uint32_t) { desyncFired = true; }
    );

    uint8_t buffer[32] {};
    uint32_t length;

    length = BuildCrcPayload(buffer, sizeof(buffer), 1, 0x12345678);
    BitReader r1(buffer, length); rts.OnPacketReceived(1, 2, r1);

    length = BuildCrcPayload(buffer, sizeof(buffer), 1, 0x12345678);
    BitReader r2(buffer, length); rts.OnPacketReceived(2, 2, r2);

    EXPECT_FALSE(desyncFired);
}

//! Peer disconnected decreases expected count

TEST(RtsModule, PeerDisconnected_AdvanceTurn)
{
    RtsModule rts;
    rts.OnInit();
    rts.OnPeerConnected(1);
    rts.OnPeerConnected(2);
    rts.OnPeerDisconnected(2); //only 1 peer now expected

    bool fired = false;
    rts.SetTurnReadyCallback([&](const TurnData&) { fired = true; });
    
    uint8_t buffer[32] {};
    uint32_t length = BuildCommandsPayload(buffer, sizeof(buffer), 1, {});
    BitReader r(buffer, length);
    rts.OnPacketReceived(1, 2, r);

    EXPECT_TRUE(fired);
}