/*
    * Unit tests for the snapshot manager
*/

#include <gtest/gtest.h>
#include "../Replication/SnapshotManagement.h"

using namespace Aegis::Replication;
using namespace Aegis::Serializer;
using namespace Aegis;

//* Minimal Hydra implementation for testing

struct TestObj : public Hydra
{
    float val = 0.0f;
    mutable float rcvdVal = 0.0f;

    void Serialize(BitWriter& w) const override
    {
        w.WriteFloat(val);
    }

    void Deserialize(BitReader& r) const override
    {
        rcvdVal = r.ReadFloat();
    }

    uint8_t GetPriority() const override { return 255; }; //always included
};

struct LowPriorityObj : public Hydra
{
    mutable uint8_t rcvdCount = 0;
    uint8_t counter = 0;

    void Serialize(BitWriter& w) const override { w.WriteUInt8(counter); }
    void Deserialize(BitReader& r) const override { rcvdCount = r.ReadUInt8(); }
    uint8_t GetPriority() const override { return 1; }
};

//! Registration

TEST(SnapshotManager, RegisterObj_ReturnsValidID)
{
    SnapshotManager mgr;
    TestObj obj;
    ObjectID id = mgr.RegisterObject(&obj, 255);
    EXPECT_NE(id, INVALID_OBJECT_ID);
}

TEST(SnapshotManager, RegisterObj_MultiObjs_UniqueIDs)
{
    SnapshotManager mgr;
    TestObj a, b, c;

    ObjectID idA = mgr.RegisterObject(&a, 255);
    ObjectID idB = mgr.RegisterObject(&b, 255);
    ObjectID idC = mgr.RegisterObject(&c, 255);

    EXPECT_NE(idA, idB);
    EXPECT_NE(idB, idC);
    EXPECT_NE(idA, idC);
}

//! Create Delta before capturing

TEST(SnapshotManager, CreateDelta_BeforeCapture_ReturnsNotInit)
{
    SnapshotManager mgr;
    TestObj obj;
    mgr.RegisterObject(&obj, 255);

    DeltaPacket delta;
    EXPECT_EQ(mgr.CreateDelta(0, 1, delta), NetResult::NotInitialized);
}

//! Delta contains changed obj

TEST(SnapshotManager, CreatedDelta_ChangedObjIncluded)
{
    SnapshotManager mgr;
    TestObj obj;
    obj.val = 1.0f;
    ObjectID id = mgr.RegisterObject(&obj, 255);

    mgr.CaptureSnapshot(1);
    obj.val = 2.0f;
    mgr.CaptureSnapshot(2);

    DeltaPacket delta;
    ASSERT_EQ(mgr.CreateDelta(1, 2, delta), NetResult::Ok);
    EXPECT_EQ(delta.fromTick, 1u);
    EXPECT_EQ(delta.toTick, 2u);
    ASSERT_EQ(delta.deltas.size(), 1u);
    EXPECT_EQ(delta.deltas[0].ObjectID, id);
    EXPECT_FALSE(delta.deltas[0].isDeleted);
}

//! Delta empty if nothing changed

TEST(SnapshotManager, CreateDelta_UnchangedEmpty)
{
    SnapshotManager mgr;
    TestObj obj;
    obj.val = 5.0f;
    mgr.RegisterObject(&obj, 255);

    mgr.CaptureSnapshot(1);
    mgr.CaptureSnapshot(2); //val unchanged

    DeltaPacket delta;
    ASSERT_EQ(mgr.CreateDelta(1, 2, delta), NetResult::Ok);
    EXPECT_TRUE(delta.deltas.empty());
}

//! New client rcvs full state of isNew deltas

TEST(SnapshotManager, CreateDelta_NewClient_FullState)
{
    SnapshotManager mgr;
    TestObj a, b;
    a.val = 1.0f;
    b.val = 2.0f;
    mgr.RegisterObject(&a, 255);
    mgr.RegisterObject(&b, 255);

    mgr.CaptureSnapshot(1);

    DeltaPacket delta;
    ASSERT_EQ(mgr.CreateDelta(0, 1, delta), NetResult::Ok);
    EXPECT_EQ(delta.deltas.size(), 2u);
    for(auto& d : delta.deltas)
        EXPECT_TRUE(d.isNew);
}

//! Unregistered object produces isDeleted delta

TEST(SnapshotManager, UnregisteredObj_ProducesDeletedDelta)
{
    SnapshotManager mgr;
    TestObj obj;
    obj.val = 3.0f;
    ObjectID id = mgr.RegisterObject(&obj, 255);

    mgr.CaptureSnapshot(1);
    mgr.UnregisterObject(id);
    mgr.CaptureSnapshot(2);

    DeltaPacket delta;
    ASSERT_EQ(mgr.CreateDelta(1, 2, delta), NetResult::Ok);

    bool foundDeleted = false;
    for(auto& d : delta.deltas)
        if(d.ObjectID == id && d.isDeleted) foundDeleted = true;
    EXPECT_TRUE(foundDeleted);
}

//! ApplyDelta deserializes into registered object

TEST(SnapshotManager, ApplyDelta_UpdatesObjectState)
{
    //Server captures change and creates delta
    SnapshotManager server;
    TestObj serverObj;
    serverObj.val = 10.0f;
    server.RegisterObject(&serverObj, 255);
    server.CaptureSnapshot(1);

    serverObj.val = 99.0f;
    server.CaptureSnapshot(2);

    DeltaPacket delta;
    ASSERT_EQ(server.CreateDelta(1, 2, delta), NetResult::Ok);
    ASSERT_FALSE(delta.deltas.empty());

    //Client registers the same object and applies delta
    SnapshotManager client;
    TestObj clientObj;
    clientObj.val = 10.0f;
    client.RegisterObject(&clientObj, 255);

    ASSERT_EQ(client.ApplyDelta(delta), NetResult::Ok);
    //rcvdVal is written by Deserialize via mutable member
    EXPECT_NEAR(clientObj.rcvdVal, 99.0f, 0.0001f);
}

//! Get latest tick
TEST(SnapshotManager, GetLatestTick_ReturnsCorrectTick)
{
    SnapshotManager mgr;
    TestObj obj;
    mgr.RegisterObject(&obj, 255);

    EXPECT_EQ(mgr.GetLatestTick(), 0u);

    mgr.CaptureSnapshot(5);
    EXPECT_EQ(mgr.GetLatestTick(), 5u);

    mgr.CaptureSnapshot(10);
    EXPECT_EQ(mgr.GetLatestTick(), 10u);
}

//! FindSnapshot returns nullptr before capture

TEST(SnapshotManager, FindSnapshot_ReturnsNull_BeforeCapture)
{
    SnapshotManager mgr;
    EXPECT_EQ(mgr.FindSnapshot(1), nullptr);
}

TEST(SnapshotManager, FindSnapshot_ReturnsCorrectSnapshot)
{
    SnapshotManager mgr;
    TestObj obj;
    mgr.RegisterObject(&obj, 255);
    mgr.CaptureSnapshot(7);

    const Snapshooter* snap = mgr.FindSnapshot(7);
    ASSERT_NE(snap, nullptr);
    EXPECT_EQ(snap -> tick, 7u);
}