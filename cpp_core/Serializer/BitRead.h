/*
    * Reads from a fixed byte buffer at individual bits
        - Must be used w/ buffer produced by BitWrite using the same field order and bit-widths
        - Mismatches will cause corrupted data returns
*/

#pragma once

#include "../EngineTypes.h"
#include <cstdint>
#include <cstring>

namespace Aegis {
namespace Serializer {

    class BitReader {
        public:
            BitReader(const uint8_t* buffer, uint32_t bufferSize);
            uint32_t ReadBits(uint32_t numBits);
            bool ReadBool();
            float ReadFloat();
            uint32_t ReadUInt32();
            uint16_t ReadUInt16();
            uint8_t ReadUInt8();
            void ReadString(char* outBuffer, uint32_t bufferSize);
            bool HasFailed() const;
            uint32_t GetBitsRead() const;

        private:
            const uint8_t* t_buffer;
            uint32_t t_bufferBits;
            uint64_t t_scratch;
            uint32_t t_scratchBits;
            uint32_t t_bitIndex;
            uint32_t t_byteIndex;
            bool t_failed;
            void Refill();
    };
} //namespace Serializer
} //namespace Aegis