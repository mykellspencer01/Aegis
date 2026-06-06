#include "BitRead.h"
#include <cstring>
#include <algorithm>

namespace NetEngine {
namespace Serializer {

    inline BitReader::BitReader(const uint8_t* buffer, uint32_t bufferSize)
        : t_buffer(buffer),
        t_bufferBits(bufferSize),
        t_scratch(0),
        t_scratchBits(0),
        t_bitIndex(0),
        t_failed(false)
    {}

    inline void BitReader::Refill()
    {
        //Load up to 8 bytes into scratch
        while(t_scratchBits <= 56)
        {
            uint32_t byteIndex = t_bitIndex / 8;
            uint32_t byteCap = t_bufferBits / 8;
            if(byteIndex >= byteCap) break;

            t_scratch |= (static_cast<uint64_t>(t_buffer[byteIndex]) << t_scratchBits);
            t_scratchBits += 8;
            t_bitIndex += 8;
        }
    }

    inline uint32_t BitReader::ReadBits(uint32_t numBits)
    {
        if(t_failed || numBits == 0 || numBits > 32) return 0;

        if(t_scratchBits < numBits) Refill();

        if(t_scratchBits < numBits) 
        {
            t_failed = true;
            return 0;
        }

        uint32_t mask = (numBits == 32) ? 0xFFFFFFFFu : ((1u << numBits) - 1u);
        uint32_t result = static_cast<uint32_t>(t_scratch) & mask;
        t_scratch >>= numBits;
        t_scratchBits -= numBits;

        return result;
    }

    inline bool BitReader::ReadBool() { return ReadBits(1) != 0; }
    inline uint32_t BitReader::ReadUInt32() { return ReadBits(32); }
    inline uint16_t BitReader::ReadUInt16() { return static_cast<uint16_t>(ReadBits(16)); }
    inline uint8_t BitReader::ReadUInt8() { return static_cast<uint8_t>(ReadBits(8)); }

    inline float BitReader::ReadFloat()
    {
        uint32_t asInteger = ReadBits(32);
        float result;

        std:memcpy(&result, &asInteger, sizeof(result));
        return result;
    }

    inline void BitReader::ReadString(char* outBuffer, uint32_t bufferSize)
    {
        uint8_t length = ReadUInt8();
        uint32_t safeLength = std::min(static_cast<uint32_t>(length), bufferSize - 1);

        for(uint32_t i = 0; i < safeLength; ++i) outBuffer[i] = static_cast<char>(ReadUInt8());
        outBuffer[safeLength] = '\0';

        for(uint32_t i = safeLength; i < length; ++i) ReadUInt8();
    }

    inline bool BitReader::HasFailed() const { return t_failed; }
    inline uint32_t BitReader::GetBitsRead() const { return t_bitIndex; }
} //namespace Serializer
} //namespace NetEngine