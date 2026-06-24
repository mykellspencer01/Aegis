/*
    * AES-128-CTR packet encryption and HMAC-SHA256 auth

    * CryptoProvider wraps OpenSSL's EVP API to provide:
        - AES-128-CTR stream (en/de)cryption of pky payloads
            + CTR mode is chosen because:
                > No padding needed
                > Decryption errors don't cascade across other bytes
                > (En/de)cryption use same code path
        - HMAC-SHA256 message auth codes
            + 8-byte, truncated HMAC is appended to each encrypted pkt
            + Recipients verify HMAC before decrypting; drops forged pkts
                > does not waste CPU on decryption
    * Key Management
        - Engine derives a unique session key per peer during handshake
            + key is passed to a set key func on both sides before exchanging pkts
            + 16-byte IV derived per-paacket from pkt seq #; prevents replay attks
                > does so without transmitting a separate IV field
    * Build Dependency
        - Requires OpenSSL >= 1.1.0
        - Controlled by the CMake option NETENGINE_USE_OPENSSL (ON by default)
            + when OFF, Encrypt()/Decrypt() are no-ops
                > useful for LAN/dev builds
*/

#pragma once

#include "../EngineTypes.h"
#include <cstdint>
#include <array>

namespace Aegis {
namespace Crypto {

    //AES-128 key in bytes
    constexpr uint32_t AES128_KEY_BYTES = 16;

    //HMAC truncation length 
    constexpr uint32_t HMAC_TAG_BYTES = 8;

    class CryptoProvider {

        public:
            CryptoProvider();
            ~CryptoProvider();
            void SetSeshKey(const uint8_t* key);
            bool HasKey() const;
            NetResult Encrypt(uint16_t seq, const uint8_t* plainText, 
                uint32_t plainTextLength, uint8_t* outCypher, uint32_t& outLength);
            NetResult Decrypt(uint16_t seq, const uint8_t* cypherText, 
                uint32_t cypherTextLength, uint8_t* outPlain, uint32_t& outLength);
                
        private:
            //IV = (first 14 bytes of key) XOR (sequence) 
            std::array<uint8_t, AES128_KEY_BYTES> DeriveIV(uint16_t seq) const; //derive 16-byte per-pkt IV
            std::array<uint8_t, AES128_KEY_BYTES> t_key; //128-bit session key
            bool t_hasKey;
    };
} //namespace Crypto
} //namespace Aegis