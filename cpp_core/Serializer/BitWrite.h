/*
    * @brief BitWrite packs values into a byte buffer at individual bits
        - Dramatically reduces bandwidth for fields that need fewer bits than a full int
            + Ex. Writing player HP value 0-100 in 7 bits instead of 8
            + Ex. Boolean in 1 bit instead of 1 byte
    * Endian
        - Bits packed LSB-first within each word
        - Flush() to write the word to the buffer in little-endian byte order
        - BitReader mirrors this so cross-platform round-trips are transparent
*/

#pragma once

#include "../EngineTypes.h"
#include <cstdint>
#include <cassert>

namespace Aegis {
namespace Serializer{

    // * Class writes individual bits (and convenience values) into a fixed-size byte buffer
    // Maintains 64-bit scratch register and internal bit cursor

    class BitWriter {
        public:
            BitWriter(uint8_t* buffer, uint32_t bufferSize);

            //Core Primitives
            NetResult WriteBits(uint32_t val, uint32_t numBits);
            NetResult WriteBool(bool val);
            NetResult WriteFloat(float val);
            NetResult WriteUInt32(uint32_t val);
            NetResult WriteUInt16(uint16_t val);
            NetResult WriteUInt8(uint8_t val);
            NetResult WriteString(const char* str);

            //Finalization
            void Flush();

            //State Accessors
            uint32_t GetBytesWritten() const;
            bool HasOverflowed() const;

        private:
            uint8_t* t_buffer;
            uint32_t t_bufferSize;
            uint64_t t_scratch;
            uint32_t t_scratchBits;
            uint32_t t_byteIndex;
            bool t_overflow;
            void FlushBytes();
    };
} //namespace Serializer
} //namespace Aegis