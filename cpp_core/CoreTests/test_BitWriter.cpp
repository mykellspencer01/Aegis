/*
    * Unit tests for Bit(Write/Read) serializer subsystem

    * Tests:
        - Basic-round trip to assert equality with type fields
        - Bool packing correctly round-trip
        - Float round-trip: IEEE 754 bit-cast survives serialization
        - String round-trip encodes and decodes correctly
        - Overflow detection
        - Underflow detection
        - Bit-width masking writes only the requested number of bits
        - Multi-field (mixed) field types are packed tghtly w/o corruption
*/

#include <gtest/gtest.h>
#include "../Serializer/BitRead.h"
#include "../Serializer/BitWrite.h"

using namespace NetEngine::Serializer;

//! Basic round trip

TEST(BitWriter, RoundTrip_BasicFields)
{
    uint8_t buffer[64] {};
    BitWriter w(buffer, sizeof(buffer));

    //Varying field sizes
    w.WriteBits(42, 8);
    w.WriteBool(true);
    w.WriteBool(false);
    w.WriteFloat(3.14f);
    w.WriteUInt16(1234);
    w.WriteUInt8(255);
    w.Flush();

    ASSERT_FALSE(w.HasOverflowed());

    BitReader r(buffer, w.GetBytesWritten());
    EXPECT_EQ(r.ReadBits(8), 42u);
    EXPECT_EQ(r.ReadBool(), true);
    EXPECT_EQ(r.ReadBool(), false);
    EXPECT_NEAR(r.ReadFloat(), 3.14f, 0.001f);
    EXPECT_EQ(r.ReadUInt16(), 1234u);
    EXPECT_EQ(r.ReadUInt8(), 255u);
    EXPECT_FALSE(r.HasFailed());
}

//! Uint32 full range

TEST(BitWriter, RoundTrip_UInt32)
{
    uint8_t buffer[8] {};
    BitWriter w(buffer, sizeof(buffer));
    w.WriteUInt32(0x12345678);
    w.Flush();

    BitReader r(buffer, w.GetBytesWritten());
    EXPECT_EQ(r.ReadUInt32(), 0x12345678u);
    EXPECT_FALSE(r.HasFailed());      
} 

//! String round trip

TEST(BitWriter, RoundTrip_String)
{
    uint8_t buffer[64] {};
    BitWriter w(buffer, sizeof(buffer));
    w.WriteString("AegisNet");
    w.Flush();

    char out[16] {};
    BitReader r(buffer, w.GetBytesWritten());
    r.ReadString(out, sizeof(out));
    EXPECT_STREQ(out, "AegisNet");
    EXPECT_FALSE(r.HasFailed());
}

TEST(BitWriter, RoundTrip_EmptyString)
{
    uint8_t buffer[16] {};
    BitWriter w(buffer, sizeof(buffer));
    w.WriteString("");
    w.Flush();

    char out[16] {};
    BitReader r(buffer, w.GetBytesWritten());
    r.ReadString(out, sizeof(out));
    EXPECT_STREQ(out, "");
    EXPECT_FALSE(r.HasFailed());
}

//! Bit-width masking

TEST(BitWriter, BitWidthMasking)
{
    uint8_t buffer[8] {};
    BitWriter w(buffer, sizeof(buffer));

    //write 0xFF into 4-bit field; lower 4 bits should come back
    w.WriteBits(0xFF, 4);
    w.Flush();

    BitReader r(buffer, w.GetBytesWritten());
    EXPECT_EQ(r.ReadBits(4), 0xFu);
    EXPECT_FALSE(r.HasFailed());
}

//! Overflow detection

TEST(BitWriter, Overflow_SetsFlag)
{
    uint8_t buffer[2] {};
    BitWriter w(buffer, sizeof(buffer));
    w.WriteUInt32(0x12345678);
    w.Flush();
    EXPECT_TRUE(w.HasOverflowed());
}

TEST(BitWriter, Overflow_DoesNotWritePastBuffer)
{
    //Sentinel bytes after the buffer must remain untouched
    uint8_t region[5] {};
    region[4] = 0xAB;

    BitWriter w(region, 4);
    w.WriteUInt32(0x12345678);
    w.WriteUInt8(0xFF);
    w.Flush();

    EXPECT_TRUE(w.HasOverflowed());
    EXPECT_EQ(region[4], 0xAB);
}

//! Underflow detection

TEST(BitReader, Underflow_SetsFailedFlag)
{
    uint8_t buffer[1] = {0xAB};
    BitReader r(buffer, 1);
    r.ReadBits(8);
    r.ReadBits(1);
    EXPECT_TRUE(r.HasFailed());
}

//! Multi-field tight packing

TEST(BitWriter, MultiField_TightPacking)
{
    uint8_t buffer[16] {};
    BitWriter w(buffer, sizeof(buffer));

    //Mix of odd-bit widths for cross-byte packing
    w.WriteBits(3, 2); //2 bits
    w.WriteBits(15, 4); //4 bits
    w.WriteBool(true); //1 bit
    w.WriteBits(100, 7); //7 bits
    w.WriteUInt8(77); //8 bits
    w.Flush();

    BitReader r(buffer, w.GetBytesWritten());
    EXPECT_EQ(r.ReadBits(2), 3u);
    EXPECT_EQ(r.ReadBits(4), 15u);
    EXPECT_EQ(r.ReadBool(), true);
    EXPECT_EQ(r.ReadBits(7), 100u);
    EXPECT_EQ(r.ReadUInt8(), 77u);
    EXPECT_FALSE(r.HasFailed());
}
