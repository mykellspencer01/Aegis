/*
    * Unit tests for MMO module
*/

#include <gtest/gtest.h>
#include "../Genres/MMO/MmoModule.h"
#include "../Serializer/BitRead.h"
#include "../Serializer/BitWrite.h"
#include <algorithm>

using namespace NetEngine::Genres;
using namespace NetEngine::Serializer;
using namespace NetEngine;

// * Helpers to build MMO_RPC_MOVE paylaod
static uint32_t BuildMovePayload(uint8_t* buffer, uint32_t bufferSize, float x, float z)
{
    BitWriter w(buffer, bufferSize);
    w.WriteBits(RPC_MMO_MOVE, 16);
    w.WriteFloat(x);
    w.WriteFloat(z);
    w.Flush();
    return w.GetBytesWritten();
}

// * build RPC_MMO_ZONE_REQ payload
static uint32_t BuildZoneRequestPayload(uint8_t* buffer, uint32_t bufferSize, uint32_t zoneID)
{
    BitWriter w(buffer, bufferSize);
    w.WriteBits(RPC_MMO_ZONE_REQ, 16);
    w.WriteUInt32(zoneID);
    w.Flush();
    return w.GetBytesWritten();
}

// * check if a peer ID is in a vector
static bool Contains(const std::vector<PeerID>& v, PeerID id)
{
    return std::find(v.begin(), v.end(), id) != v.end();
}

//! On Init

TEST(MmoModule, OnInit_ReturnsOk)
{
    MmoModule mmo;
    EXPECT_EQ(mmo.OnInit(), NetResult::Ok);
}

//! AOI Grid isolation

TEST(MmoModule, AOIPeers_IsoPeerReturnsEmpty)
{
    MmoModule mmo;
    mmo.OnInit();
    mmo.OnPeerConnected(1);
    //no other peers
    EXPECT_TRUE(mmo.GetAoiPeers(1).empty());
}

//! AOI Grid same-cell

TEST(MmoModule, SameCell_ReturnsOtherPeer)
{
    MmoModule mmo;
    mmo.OnInit();
    mmo.OnPeerConnected(1);
    mmo.OnPeerConnected(2);

    //Both peers at origin - same cell (0, 0)
    mmo.UpdatePlayerPos(1, 0.0f, 0.0f, 0);
    mmo.UpdatePlayerPos(2, 0.0f, 0.0f, 0);

    auto aoi = mmo.GetAoiPeers(1);
    EXPECT_TRUE(Contains(aoi, 2));
}

TEST(MmoModule, AOIPeers_NotIncludeSelf)
{
    MmoModule mmo;
    mmo.OnInit();
    mmo.OnPeerConnected(1);
    mmo.UpdatePlayerPos(1, 0.0f, 0.0f, 0);

    auto aoi = mmo.GetAoiPeers(1);
    EXPECT_FALSE(Contains(aoi, 1));
}

//! AOI Grid adjacent cell

TEST(MmoModule, AdjacentCell_ReturnsNeighbor)
{
    MmoModule mmo;
    mmo.OnInit();
    mmo.OnPeerConnected(1);
    mmo.OnPeerConnected(2);

    mmo.UpdatePlayerPos(1, 0.0f, 0.0f, 0);
    mmo.UpdatePlayerPos(2, 105.0f, 0.0f, 0);

    auto aoi1 = mmo.GetAoiPeers(1);
    auto aoi2 = mmo.GetAoiPeers(2);

    EXPECT_TRUE(Contains(aoi1, 2));
    EXPECT_TRUE(Contains(aoi2, 1));
}

//! AOI Grid - out of range (> 1 cell gap)

TEST(MmoModule, FarAwayPeer_NotReturned)
{
    MmoModule mmo;
    mmo.OnInit();
    mmo.OnPeerConnected(1);
    mmo.OnPeerConnected(2);

    mmo.UpdatePlayerPos(1, 0.0f, 0.0f, 0);
    mmo.UpdatePlayerPos(2, 350.0f, 0.0f, 0);

    auto aoi = mmo.GetAoiPeers(1);
    EXPECT_FALSE(Contains(aoi, 2));
}

//! Crossing cell boundary updates

TEST(MmoModule, CellCrossing_Updates)
{
    MmoModule mmo;
    mmo.OnInit();
    mmo.OnPeerConnected(1);
    mmo.OnPeerConnected(2);

    //peer 2 is far away
    mmo.UpdatePlayerPos(1, 0.0f, 0.0f, 0);
    mmo.UpdatePlayerPos(2, 350.0f, 0.0f, 0);
    EXPECT_FALSE(Contains(mmo.GetAoiPeers(1), 2));

    //peer 2 moves into peer 1's neighborhood
    mmo.UpdatePlayerPos(2, 50.0f, 0.0f, 0); // same cell as peer 1
    EXPECT_TRUE(Contains(mmo.GetAoiPeers(1), 2));
}

//! Zone peers

TEST(MmoModule, GetZonePeers_ReturnsCorrectZone)
{
    MmoModule mmo;
    mmo.OnInit();
    mmo.OnPeerConnected(1);
    mmo.OnPeerConnected(2);
    mmo.OnPeerConnected(3);

    mmo.UpdatePlayerPos(1, 0.0f, 0.0f, 1); //peer 1 in zone 1
    mmo.UpdatePlayerPos(2, 0.0f, 0.0f, 1); //peer 2 in zone 1
    mmo.UpdatePlayerPos(3, 0.0f, 0.0f, 2); //peer 3 in zone 2

    auto zone1 = mmo.GetZonePeers(1);
    auto zone2 = mmo.GetZonePeers(2);

    EXPECT_TRUE(Contains(zone1, 1));
    EXPECT_TRUE(Contains(zone1, 2));
    EXPECT_FALSE(Contains(zone1, 3));

    EXPECT_TRUE(Contains(zone2, 3));
    EXPECT_FALSE(Contains(zone2, 1));
}

//! Zone transfer callback

TEST(MmoModule, ZoneTransferCall_FiresOnZoneReq)
{
    MmoModule mmo;
    mmo.OnInit();
    mmo.OnPeerConnected(1);

    PeerID callbackPeer = PEER_NOUGHT;
    uint32_t callbackZone = 0;

    mmo.SetZoneTransferCallback([&](PeerID peer, uint32_t zone) {
        callbackPeer = peer;
        callbackZone = zone;
    });

    uint8_t buffer[16] {};
    uint32_t length = BuildZoneRequestPayload(buffer, sizeof(buffer), 42);
    BitReader r(buffer, length);
    mmo.OnPacketReceived(1, 3, r);

    EXPECT_EQ(callbackPeer, 1u);
    EXPECT_EQ(callbackZone, 42u);
}

//! Move RPC updates position

TEST(MmoModule, MoveRpc_UpdatesAOIMembers)
{
    MmoModule mmo;
    mmo.OnInit();
    mmo.OnPeerConnected(1);
    mmo.OnPeerConnected(2);

    mmo.UpdatePlayerPos(1, 0.0f, 0.0f, 1);
    mmo.UpdatePlayerPos(2, 500.0f, 0.0f, 1);

    uint8_t buffer[16] {};
    uint32_t length = BuildMovePayload(buffer, sizeof(buffer), 50.0f, 0.0f);
    BitReader r(buffer, length);
    mmo.OnPacketReceived(2, 3, r);

    EXPECT_TRUE(Contains(mmo.GetAoiPeers(1), 2));
}

//! Peer disconnect removes them from the grid

TEST(MmoModule, PeerDisconnect_RemovesFromAOI)
{
    MmoModule mmo;
    mmo.OnInit();
    mmo.OnPeerConnected(1);
    mmo.OnPeerConnected(2);

    mmo.UpdatePlayerPos(1, 0.0f, 0.0f, 0);
    mmo.UpdatePlayerPos(2, 0.0f, 0.0f, 0);
    ASSERT_TRUE(Contains(mmo.GetAoiPeers(1), 2));

    mmo.OnPeerDisconnected(2);
    EXPECT_FALSE(Contains(mmo.GetAoiPeers(1), 2));
}