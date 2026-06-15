#include "BitWrite.h"
#include <cstring>
#include <cstdio>

namespace NetEngine {
namespace Serializer{

    BitWriter::BitWriter(uint8_t* buffer, uint32_t bufferSize)
        : t_buffer(buffer),
        t_bufferSize(bufferSize),
        t_scratch(0),
        t_scratchBits(0),
        t_byteIndex(0),
        t_overflow(false)
    {}

    // * Core Primitives
    NetResult BitWriter::WriteBits(uint32_t val, uint32_t numBits)
    {
        if(t_overflow) return NetResult::BufferOverflow;
        if(numBits == 0 || numBits > 32) return NetResult::InvalidArg;

        //Mask any bits above numBits to avoid corruption fields
        uint64_t masked = static_cast<uint64_t>(val) & ((numBits == 32)
            ? 0xFFFFFFFFull
            : ((1ull << numBits) - 1));
        
        //Append bits
        t_scratch |= (masked << t_scratchBits);
        t_scratchBits += numBits;

        FlushBytes();
        return t_overflow ? NetResult::BufferOverflow : NetResult::Ok;
    }

    NetResult BitWriter::WriteBool(bool val)
    {
        return WriteBits(val ? 1u : 0u, 1);
    }

    NetResult BitWriter::WriteFloat(float val)
    {
        //Bit-cast float -> uint32_t
        uint32_t asInteger;
        std::memcpy(&asInteger, &val, sizeof(asInteger));
        return WriteBits(asInteger, 32);
    }

    NetResult BitWriter::WriteUInt32(uint32_t val) { return WriteBits(val, 32); }
    NetResult BitWriter::WriteUInt16(uint16_t val) { return WriteBits(val, 16); }
    NetResult BitWriter::WriteUInt8(uint8_t val) { return WriteBits(val, 8); }

    NetResult BitWriter::WriteString(const char* str)
    {
        if(!str) return NetResult::InvalidArg;

        uint32_t length = 0;
        while(str[length] && length < 255) ++length;

        NetResult result = WriteUInt8(static_cast<uint8_t>(length));
        if(result != NetResult::Ok) return result;

        for(uint32_t i = 0; i < length; i++)
        {
            result = WriteUInt8(static_cast<uint8_t>(str[i]));
            if(result != NetResult::Ok) return result;
        }
        return NetResult::Ok;
    }

    // * Finalization
    void BitWriter::Flush()
    {
        if(t_scratchBits > 0)
        {
            if(t_byteIndex < t_bufferSize) t_buffer[t_byteIndex++] = static_cast<uint8_t>(t_scratch & 0xFF);
            else t_overflow = true;

            t_scratch >>= 8;
            t_scratchBits -= 8;
        }
    }

    // * State Accessors
    uint32_t BitWriter::GetBytesWritten() const { return t_byteIndex; }
    bool BitWriter::HasOverflowed() const { return t_overflow; }

    // * Internal Helpers
    void BitWriter::FlushBytes()
    {
        while(t_scratchBits >= 8)
        {
            if(t_byteIndex >= t_bufferSize)
            {
                t_overflow = true;
                return;
            }

            t_buffer[t_byteIndex++] = static_cast<uint8_t>(t_scratch & 0xFF);
            t_scratch >>= 8;
            t_scratchBits -= 8;
        } 
    }
}
}