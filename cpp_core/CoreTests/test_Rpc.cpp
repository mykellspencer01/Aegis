/*
    * Unit tests for the RPC dispatcher
*/

#include <gtest/gtest.h>
#include "../Replication/RpcDispatcher.h"
#include "../Serializer/BitWrite.h"
#include "../Serializer/BitRead.h"

using namespace NetEngine::Replication;
using namespace NetEngine::Serializer;
using namespace NetEngine;

//! Register RPC validation 

TEST(RpcDispatcher, Register_NullHandler_ReturnsInvalidArg)
{
    RpcDispatcher rpc;
    EXPECT_EQ(rpc.RegisterRpc(1, nullptr), NetResult::InvalidArg);
}

TEST(RpcDispatcher, Register_ZeroID_ReturnsInvalidArg)
{
    RpcDispatcher rpc;
    EXPECT_EQ(rpc.RegisterRpc(0, [](PeerID, BitReader&){}), NetResult::InvalidArg);
}

TEST(RpcDispatcher, Register_ValidHandler_ReturnsOk)
{
    RpcDispatcher rpc;
    EXPECT_EQ(rpc.RegisterRpc(1, [](PeerID, BitReader&){}), NetResult::Ok);
}

//! Dispatch calls correct handler

TEST(RpcDispatcher, Dispatch_Calls_RegisterHandler)
{
    RpcDispatcher rpc;
    bool called = false;

    rpc.RegisterRpc(42, [&](PeerID, BitReader&) { called = true; });

    uint8_t buffer[4] {};
    BitWriter w(buffer, sizeof(buffer));
    w.WriteBits(42, 16);
    w.Flush();

    ASSERT_EQ(rpc.Dispatch(1, buffer, w.GetBytesWritten()), NetResult::Ok);
    EXPECT_TRUE(called);
}

TEST(RpcDispatcher, Dispatch_UnknownID_Returns_GenreModuleNotFound)
{
    RpcDispatcher rpc;

    uint8_t buffer[4] {};
    BitWriter w(buffer, sizeof(buffer));
    w.WriteBits(999, 16); //no handler for 999
    w. Flush();

    EXPECT_EQ(rpc.Dispatch(1, buffer, w.GetBytesWritten()), NetResult::GenreModuleNotFound);
}

TEST(RpcDispatcher, Dispatch_ShortPayload_ReturnsInvalidArg)
{
    RpcDispatcher rpc;
    uint8_t buffer[1] = {0x01}; //need at least 2 bytes for rpc ID
    EXPECT_EQ(rpc.Dispatch(1, buffer, 1), NetResult::InvalidArg);
}

TEST(RpcDispatcher, Dispatch_NullPayload_ReturnsInvalidArg)
{
    RpcDispatcher rpc;
    EXPECT_EQ(rpc.Dispatch(1, nullptr, 4), NetResult::InvalidArg);
}

//! Handler receives correct sender peerID

TEST(RpcDispatcher, Dispatch_HandlerReceives_CorrectSender)
{
    RpcDispatcher rpc;
    PeerID receivedSender = PEER_NOUGHT; // your zero-peer constant

    rpc.RegisterRpc(7, [&](PeerID sender, BitReader&) { receivedSender = sender; });

    uint8_t buffer[4] {};
    BitWriter w(buffer, sizeof(buffer));
    w.WriteBits(7, 16);
    w.Flush();

    rpc.Dispatch(12345, buffer, w.GetBytesWritten());
    EXPECT_EQ(receivedSender, 12345);
}

//! Handler reads correct arg values

TEST(RpcDispatcher, Dispatch_HandlerReadsArgs)
{
    RpcDispatcher rpc;
    uint32_t receivedA = 0;
    float receivedB = 0.0f;
    bool receivedC = false;

    rpc.RegisterRpc(10, [&](PeerID, BitReader& r) {
        receivedA = r.ReadUInt32();
        receivedB = r.ReadFloat();
        receivedC = r.ReadBool();
    });

    uint8_t buffer[16] {};
    BitWriter w(buffer, sizeof(buffer));

    w.WriteBits(10, 16); //rpc ID
    w.WriteUInt32(9876); //arg A
    w.WriteFloat(3.14f); //arg B
    w.WriteBool(true); //arg C
    w.Flush();

    ASSERT_EQ(rpc.Dispatch(1, buffer, w.GetBytesWritten()), NetResult::Ok);
    EXPECT_EQ(receivedA, 9876u);
    EXPECT_NEAR(receivedB, 3.14f, 0.0001);
    EXPECT_TRUE(receivedC);
}

//! Unregistered Rpc

TEST(RpcDispatcher, UnregisteredRpc_HandlerNotCalled_AfterRemoval)
{
    RpcDispatcher rpc;
    bool called = false;

    rpc.RegisterRpc(5, [&](PeerID, BitReader&) { called = true; });
    rpc.UnregisteredRpc(5);

    uint8_t buffer[4] {};
    BitWriter w(buffer, sizeof(buffer));
    w.WriteBits(5, 16);
    w.Flush();

    rpc.Dispatch(1, buffer, w.GetBytesWritten());
    EXPECT_FALSE(called);
}

//! Serialize Call

TEST(RpcDispatcher, SerializeCall_WritesIDAndArgs)
{
    RpcDispatcher rpc;
    uint8_t buffer[16] {};
    uint32_t outLength = 0;

    NetResult result = rpc.SerializeCall(
        55,
        buffer, sizeof(buffer),
        [](BitWriter& w) {
            w.WriteUInt8(50);
            w.WriteFloat(7.77f);
        },
        outLength
    );

    ASSERT_EQ(result, NetResult::Ok);
    EXPECT_GT(outLength, 0u);

    BitReader r(buffer, outLength);
    EXPECT_EQ(r.ReadBits(16), 55u);
    EXPECT_EQ(r.ReadUInt8(), 50u);
    EXPECT_NEAR(r.ReadFloat(), 7.77f, 0.0001f);
    EXPECT_FALSE(r.HasFailed());
}

//! Full Round-trip

TEST(RpcDispatcher, FullRoundTrip_SerializeAndDispatch)
{
    RpcDispatcher rpc;
    uint32_t receeivedVal = 0;

    rpc.RegisterRpc(20, [&](PeerID, BitReader& r) {
        receeivedVal = r.ReadUInt32();
    });

    uint8_t buffer[16] {};
    uint32_t outLength = 0;
    rpc.SerializeCall(
        20, buffer, sizeof(buffer),
        [](BitWriter& w) { w.WriteUInt32(0x12345678); },
        outLength
    );

    ASSERT_EQ(rpc.Dispatch(1, buffer, outLength), NetResult::Ok);
    EXPECT_EQ(receeivedVal, 0x12345678u);
}