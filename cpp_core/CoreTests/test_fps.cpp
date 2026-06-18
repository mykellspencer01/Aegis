/*
    * Unit tests for the FPS module
*/

#include <gtest/gtest.h>
#include "../Genres/FPS/FpsModule.h"
#include "../Serializer/BitWrite.h"
#include "../Serializer/BitRead.h"

using namespace NetEngine::Genres;
using namespace NetEngine::Serializer;
using namespace NetEngine;

//* Helper to build a hitscan payload
static uint32_t BuildHitscanPayload(
    uint8_t* buffer, uint32_t bufferSize,
    float ox, float oy, float oz,
    float dx, float dy, float dz,
    float clientTime, uint8_t weaponID)
{
    BitWriter w(buffer, bufferSize);
    w.WriteBits(RPC_RPS_HIT_SCAN, 16);
    w.WriteFloat(ox); w.WriteFloat(oy); w.WriteFloat(oz);
    w.WriteFloat(dx); w.WriteFloat(dy); w.WriteFloat(dz);
    w.WriteFloat(clientTime);
    w.WriteUInt8(weaponID);
    w.Flush();

    return w.GetBytesWritten();
}

//! On Init

TEST(FpsModule, OnInit_ReturnsOk)
{
    FpsModule fps;
    EXPECT_EQ(fps.OnInit(), NetResult::Ok);
}

TEST(FpsModule, OnInit_DoubleInit_ReturnsOk)
{
    FpsModule fps;
    fps.OnInit();
    EXPECT_EQ(fps.OnInit(), NetResult::Ok);
}

//! HitDetect callback fires on valid RPC

TEST(FpsModule, HitScan_CallbackFires)
{
    FpsModule fps;
    fps.OnInit();

    //Record one frame w/ one player
    PlayerState ps {};
    ps.peerID = 1;
    ps.posX = 0.0f; ps.posY = 0.0f; ps.posZ = 0.0f;
    ps.isAlive = true;
    fps.RecordHistoryFrame({ ps }, 1.0);

    bool callbackFired = false;
    fps.SetHitDetectCallback(
        [&](float, float, float, float, float, float,
        uint8_t, const std::vector<PlayerState>&,
        PeerID& victim, uint8_t& damage) -> bool
    {
        callbackFired = true;
        victim = 1;
        damage = 25;
        return true;
    });

    uint8_t buffer[64] {};
    uint32_t length = BuildHitscanPayload(
        buffer, sizeof(buffer),
        0.0f, 0.0f, -5.0f,
        0.0f, 0.0f, 1.0f,
        1.0f, 1
    );

    BitReader r(buffer, length);
    fps.OnPacketReceived(99, 2, r);
    EXPECT_TRUE(callbackFired);
}

//! Callback receives rewound frame nearest to client's timestamp

TEST(FpsModule, HitScan_CallbackRcvs_RewoundFrame)
{
    FpsModule fps;
    fps.OnInit();

    //frame at t = 1.0: player at x = 10
    PlayerState early {};
    early.peerID = 1;
    early.posX = 10.0f; early.posY = 0.0f; early.posZ = 0.0f;
    early.isAlive = true;
    fps.RecordHistoryFrame({ early }, 1.0);

    //fram at t.  2.0: player at x = 50
    PlayerState late {};
    late.peerID = 1;
    late.posX = 50.0f; late.posY = 0.0f; late.posZ = 0.0f;
    late.isAlive = true;
    fps.RecordHistoryFrame({ late }, 2.0);

    float rcvdPosX = -1.0f;
    fps.SetHitDetectCallback(
        [&](float, float, float, float, float, float,
        uint8_t, const std::vector<PlayerState>& states,
        PeerID& v, uint8_t& d) -> bool
    {
        if(!states.empty()) rcvdPosX = states[0].posX;
        v = PEER_NOUGHT; d =0;
        return false;
    });

    //Fire at t = 1.0
    //Engine should rewind to early frame
    uint8_t buffer[64] {};
    uint32_t length = BuildHitscanPayload(
        buffer, sizeof(buffer),
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f,
        1.0f, 1
    );
    BitReader r(buffer, length);
    fps.OnPacketReceived(99, 2, r);
    
    EXPECT_NEAR(rcvdPosX, 10.0f, 0.1f);
}

//! Malformed payload - HasFailed prevents crashing

TEST(FpsModule, HitScan_MalformedPayload_NoCrash)
{
    FpsModule fps;
    fps.OnInit();

    fps.SetHitDetectCallback(
        [&](float, float, float, float, float, float,
        uint8_t, const std::vector<PlayerState>& states,
        PeerID& v, uint8_t& d) -> bool { return false; }
    );

    //Only RPC iD, no args
    uint8_t buffer[2] {};
    BitWriter w(buffer, sizeof(buffer));
    w.WriteBits(RPC_RPS_HIT_SCAN, 16);
    w.Flush();

    BitReader r(buffer, w.GetBytesWritten());
    EXPECT_NO_THROW(fps.OnPacketReceived(1, 2, r));
}

//! Peer lifecycle

TEST(FpsModule, PeerConnected_NoCrash)
{
    FpsModule fps;
    fps.OnInit();

    EXPECT_NO_THROW(fps.OnPeerConnected(1));
    EXPECT_NO_THROW(fps.OnPeerConnected(2));
}

TEST(FpsModule, PeerDisconnected_NoCrash)
{
    FpsModule fps;
    fps.OnInit();
    fps.OnPeerDisconnected(1);
    EXPECT_NO_THROW(fps.OnPeerDisconnected(1));
}

TEST(FpsModule, OnShutdown_NoCrash)
{
    FpsModule fps;
    fps.OnInit();
    fps.RecordHistoryFrame({}, 1.0f);
    EXPECT_NO_THROW(fps.OnShutdown());
}