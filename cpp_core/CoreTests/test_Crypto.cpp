/*
    * Unit tests for CryptoProvider
*/

#include <gtest/gtest.h>
#include "../Packets/CryptoProvider.h"
#include <cstring>

using namespace NetEngine::Crypto;
using namespace NetEngine;

//!* Fixed 128-bit test key (dont use in production)
static const uint8_t TEST_KEY[AES128_KEY_BYTES] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
};

//! Has Key

TEST(CryptoProvider, HasKey_TrueAfterSet)
{
    CryptoProvider cp;
    cp.SetSeshKey(TEST_KEY);
    EXPECT_TRUE(cp.HasKey());
}

//! Encrpyt/Decrypt round trip

TEST(CryptoProvider, RoundTrip_BasicPayload)
{
    CryptoProvider cp;
    cp.SetSeshKey(TEST_KEY);

    const char* text = "NetEngine Packet Payload";
    uint32_t textLength = static_cast<uint32_t>(strlen(text));
    uint8_t cypher[256] {};
    uint32_t cypherLength = 0;
    uint8_t recovered[256] {};
    uint32_t recoveredLength = 0;

    ASSERT_EQ(
        cp.Encrypt(1, reinterpret_cast<const uint8_t*>(text), textLength,
            cypher, cypherLength),
        NetResult::Ok
    );
    EXPECT_EQ(cypherLength, textLength + HMAC_TAG_BYTES);
    ASSERT_EQ(
        cp.Decrypt(1, cypher, cypherLength, recovered, recoveredLength),
        NetResult::Ok
    );

    EXPECT_EQ(recoveredLength, textLength);
    EXPECT_EQ(memcmp(recovered, text, textLength), 0);
}

TEST(CryptoProvider, RoundTrip_SingleByte)
{
    CryptoProvider cp;
    cp.SetSeshKey(TEST_KEY);

    uint8_t text[1] = {0x11};
    uint8_t cypher[32] {};
    uint8_t out[32] {};
    uint32_t cypherLength = 0, outLength = 0;

    ASSERT_EQ(cp.Encrypt(99, text, 1, cypher, cypherLength), NetResult::Ok);
    ASSERT_EQ(cp.Decrypt(99, cypher, cypherLength, out, outLength), NetResult::Ok);
    EXPECT_EQ(outLength, 1u);
    EXPECT_EQ(out[0], 0x11);
}

//! Differenct Sequences produce different cipher text

TEST(CryptoProvider, DiffSequences_ProduceDiffCipher)
{
    CryptoProvider cp;
    cp.SetSeshKey(TEST_KEY);

    const uint8_t text[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint8_t cypher1[64] {}, cypher2[64] {};
    uint32_t length1 = 0, length2 = 0;

    cp.Encrypt(1, text, sizeof(text), cypher1, length1);
    cp.Encrypt(2, text, sizeof(text), cypher2, length2);

    EXPECT_NE(memcmp(cypher1, cypher2, length1), 0);
}

//! Tempered cipher text - HMAC must catch it

TEST(CryptoProvider, TemperedCipher_FailsDecrypt)
{
    CryptoProvider cp;
    cp.SetSeshKey(TEST_KEY);

    const uint8_t temp[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};
    uint8_t cypher[64] {};
    uint32_t cypherLength = 0;

    cp.Encrypt(5, temp, sizeof(temp), cypher, cypherLength);
    cypher[0] ^= 0x01; //flip one bit in body

    uint8_t out[64] {};
    uint32_t outLength = 0;
    EXPECT_EQ(cp.Decrypt(5, cypher, cypherLength, out, outLength), NetResult::CryptoFail);
}

TEST(CryptoProvider, TamperedHmacTag_FailsDecrypt)
{
    CryptoProvider cp;
    cp.SetSeshKey(TEST_KEY);

    const uint8_t temp[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint8_t cypher[64] {};
    uint32_t cypherLength {};

    cp.Encrypt(10, temp, sizeof(temp), cypher, cypherLength);
    cypher[cypherLength - 1] ^= 0xFF; //flip last byte of HMAC tag

    uint8_t out[64];
    uint32_t outLength = 0;
    EXPECT_EQ(cp.Decrypt(10, cypher, cypherLength, out, outLength), NetResult::CryptoFail);
}

//! Decrypt without key

TEST(CryptoProvider, DecryptWithoutKey_CryptoFails)
{
    CryptoProvider cp; // no session key called

    const uint8_t fakeCypher[16] {};
    uint8_t out[16] {};
    uint32_t outLength = 0;

    EXPECT_EQ(cp.Decrypt(1, fakeCypher, sizeof(fakeCypher), out, outLength), NetResult::CryptoFail);
}

//! Wrong sequence number: IV mismatch that HMAC catches

TEST(CryptoProvider, WrongSequence_FailsDecrypt)
{
    CryptoProvider cp;
    cp.SetSeshKey(TEST_KEY);

    const uint8_t temp[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    uint8_t cypher[64] {};
    uint32_t cypherLength = 0;

    cp.Encrypt(7, temp, sizeof(temp), cypher, cypherLength);

    uint8_t out[64] {};
    uint32_t outLength = 0;

    //TODO: Go through the logic of EXPECT_NE/EXPECT_EQ and CryptoFail/Ok
    //TODO: print out test information
    //Decrypt with wrong sequence
    EXPECT_NE(cp.Decrypt(256, cypher, cypherLength, out, outLength), NetResult::CryptoFail);
}