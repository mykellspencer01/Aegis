#include "CryptoProvider.h"
#include <cstring>
#include <cstdio>

#ifdef NETENGINE_USE_OPENSSL
    #include <openssl/evp.h>
    #include <openssl/hmac.h>
    #include <openssl/err.h>
#endif

#ifdef NETENGINE_LOGGING
    #define NE_LOG(fmt, ...) std::printf("[Crypto] " fmt "\n", ##__VA_ARGS__)
#else
    #define NE_LOG(fmt, ...) ((void)0)
#endif

namespace NetEngine {
namespace Crypto {
    //Constructor/Destructor

    CryptoProvider::~CryptoProvider() = default;

    CryptoProvider::CryptoProvider() : t_hasKey(false)
    {
        t_key.fill(0);
    }

    void CryptoProvider::SetSeshKey(const uint8_t* key)
    {
        if(!key) return;
        std::memcpy(t_key.data(), key, AES128_KEY_BYTES);
        t_hasKey = true;
        NE_LOG("Session key set.");
    }

    bool CryptoProvider::HasKey() const { return t_hasKey; }

    // * Deriving the IV
    std::array<uint8_t, AES128_KEY_BYTES> CryptoProvider::DeriveIV(uint16_t seq) const
    {
        std::array<uint8_t, AES128_KEY_BYTES> iv = t_key;

        // XOR two seq bytes into first two bytes of IV
        // a lightweight scheme; production system would use a nonce construction (ex. ChaCha20-Poly1305)
        iv[0] ^= static_cast<uint8_t>(seq & 0xFF);
        iv[1] ^= static_cast<uint8_t>((seq >> 8) & 0xFF);

        //mix into bytes 8 and 9 so single-byte seqs differ more
        //iv[8] ^= static_cast<uint8_t>(seq & 0xFF);
        //iv[9] ^= static_cast<uint8_t>((seq >> 8) & 0xFF);
        return iv;
    }

    //* Encryption
    NetResult CryptoProvider::Encrypt(uint16_t seq, const uint8_t* plainText, 
                uint32_t plainTextLength, uint8_t* outCypher, uint32_t& outLength)
    {
        outLength = 0;

        #ifndef NETENGINE_USE_OPENSSL
            std::memcpy(outCypher, plainText, plainTextLength); //No-op passthru for builds w/o OpenSSL
            std::memset(outCypher + plainTextLength, 0, HMAC_TAG_BYTES); //append dummy HMAC tag (all 0s)
            outLength = plainTextLength + HMAC_TAG_BYTES;
            return NetResult::Ok;

        #else
            if(!t_hasKey) return NetResult::CryptoFail;
            auto iv = DeriveIV(seq);

            //AES128 encryption
            EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();

            if(!ctx) return NetResult::CryptoFail;

            int ok = EVP_EncryptInit_ex(ctx, EVP_aes_128_ctr(), nullptr, t_key.data(), iv.data());
            if(!ok)
            {
                EVP_CIPHER_CTX_free(ctx);
                NE_LOG("EVP_EncryptInit_ex failed.");
                return NetResult::CryptoFail;
            }

            int outLength1 = 0, outLength2 = 0;
            EVP_EncryptUpdate(ctx, outCypher, &outLength1, plainText, static_cast<int>(plainTextLength));
            EVP_EncryptFinal_ex(ctx, outCypher + outLength1, &outLength2);
            EVP_CIPHER_CTX_free(ctx);

            uint32_t cypherLength = static_cast<uint32_t>(outLength1 + outLength2);

            // HMAC-SHA256 over the cypher text, truncated to HMAC_TAG_BYTES
            unsigned int hmacLength = 0;
            uint8_t hmacFull[32]; //SHA256 output is 32 bytes
            HMAC(EVP_sha256(), t_key.data(), static_cast<int>(AES128_KEY_BYTES), 
                outCypher, cypherLength, hmacFull, &hmacLength);
            
            std::memcpy(outCypher + cypherLength, hmacFull, HMAC_TAG_BYTES);
            outLength = cypherLength + HMAC_TAG_BYTES;
            return NetResult::Ok;
        #endif
    }

    //* Decryption
    NetResult CryptoProvider::Decrypt(uint16_t seq, const uint8_t* cypherText, 
        uint32_t cypherTextLength, uint8_t* outPlain, uint32_t& outLength)
    {
        outLength = 0;

        if(cypherTextLength < HMAC_TAG_BYTES) return NetResult::CryptoFail;

        #ifndef NETENGINE_USE_OPENSSL
            uint32_t plainLength = cypherTextLength - HMAC_TAG_BYTES;
            std::memcpy(outPlain, cypherText, plainLength);
            outLength = plainLength;
            return NetResult::Ok;

        #else
            if(!t_hasKey) return NetResult::CryptoFail;

            uint32_t actualCypherLength = cypherTextLength - HMAC_TAG_BYTES;
            const uint8_t* receivedTag = cypherText + actualCypherLength;

            //Verifyc HMAC address
            unsigned int hmacLength = 0;
            uint8_t hmacFull[32];
            HMAC(EVP_sha256(), t_key.data(), static_cast<int>(AES128_KEY_BYTES), 
                cypherText, actualCypherLength, hmacFull, &hmacLength);

            //constant-time comparison to prevent timing attacks
            int tagMismatch = 0;
            for(uint32_t i = 0; i < HMAC_TAG_BYTES; ++i)
                tagMismatch |= (hmacFull[i] ^ receivedTag[i]);
            
            if(tagMismatch != 0)
            {
                NE_LOG("HMAC verification failed to seq %u", seq);
                return NetResult::CryptoFail;
            }

            //AES-128-CTR decryption
            EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
            if(!ctx) return NetResult::CryptoFail;

            auto iv = DeriveIV(seq);

            EVP_DecryptInit_ex(ctx, EVP_aes_128_ctr(), nullptr, t_key.data(), iv.data());

            int outLength1 = 0, outLength2 = 0;
            EVP_DecryptUpdate(ctx, outPlain, &outLength1, cypherText, static_cast<int>(actualCypherLength));
            EVP_DecryptFinal_ex(ctx, outPlain + outLength1, &outLength2);
            EVP_CIPHER_CTX_free(ctx);

            outLength = static_cast<uint32_t>(outLength1 + outLength2);
            return NetResult::Ok;
        #endif
    }
} //namespace Crypto
} //namespace NetEngine