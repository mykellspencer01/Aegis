#pragma once

#include <cstdint>
#include <cstddef>

/*
    Design Notes:
        - Fixed-width integers are used to structs are identical on both 32 and 64-bit builds
            + <cstdint>
        - struct marshalling on bridge invocations
        - strings are represented as const char* at ABI bounds
            + std::string used internally only
        - NetEngine API will expand to the dllexport and dllimport attribs
            + Dictates if a symbol is being exposed by a library or consumed by an executable
            + dllexport is inside the DLL project source code; dllimport is client-side source code
*/


// ABI (Application Binary Interface) visibility
// Functions tagged with NETENGINE_API are exported from the shared library and visible to C# DLLimport binding
#if defined(_WIN32) || defined(_WIN64)
    #ifdef NETENGINE_EXPORTS //CMAKE sets this when building lib
        #define NETENGINE_API __declspec(dllexport)
    #else
        #define NETENGINE_API __declspec(dllimport)
    #endif
#else
    #define NETENGINE_API __attribute__((visibility("default")))
#endif


/*
    Max Sizes:
        - Kept as compile-time constants
            + stack-allocated buffers in hot-path code do not need heap allocation
*/

// Max serialized byte size of a UDP data payload
constexpr uint32_t NETENGINE_MAX_PACKET_SIZE = 1400;
// Max number of tracked peers per session, simultaneously:
constexpr uint32_t NETENGINE_MAX_PLAYERS = 256;
// Max number of registered RPC handlers
constexpr uint32_t NETENGINE_MAX_RPC_HANDLERS = 512;
// Max session length of session identifier string (including null terminator)
constexpr uint32_t NETENGINE_SESSION_ID = 64;


/*
    Result/Error Codes:
        - Negative vals are errors
        - Zero is success
        - Positive values are info/status codes
        - Mapped 1-to-1 in the C# enum
*/

enum class NetResult : int32_t
{
    //Success
    Ok = 0,

    //Informational
    Pending = 1,
    Duplicate = 2, // Packet accepted, but is a known dupe (ACK sent already)

    //Errors
    InvalidArg = -1, //null or out-of-range arg
    SocketError = -2, 
    Timeout = -3,
    BufferOverflow = -4, //write would exceed allowed buffer cap
    CryptoFail = -5, //encryption, decryption, or HMAC verif. fail
    SerializeError = -6, //bitwriter-reader encountered malformed data
    UnknownPeer = -7, // Peer GUID not found in current session
    SessionFull = -8, 
    NotInitialized = -9, // Engine subsystem not yet initialized (call Init first)
    GenreModuleNotFound = -10, // Requested genre module has not been registered
    AlreadyInitialized = -11 // Init called twice without intervening shutdown
};

/*
    Identifying Peers:
        - PeerID is 64-bit opaque handle
            + Assigned by transport layer post-handshake
        - Zero (PEER_NOUGHT) means "no peers"
*/

using PeerID = uint64_t;
constexpr PeerID PEER_NOUGHT = 0;

/*
    - Packet header written at start of each datagram
    - Size = 12 bytes
    - Fields are stored in network byte-order (big-endian)
    - Serializer handles byte-swaps
*/

#pragma pack(push, 1)
struct PacketHeader
{
    uint32_t magic;    // guards against rand UDP traffic
    uint16_t sequenceNum;   // continuously increasing sequence number (wraps)
    uint16_t ackBits;   // Bitmask of the 16 packets before the ack field
    uint16_t ack;   // Last received sequence number from remote peer
    uint8_t flags;  // Bit-field: reliable(0), encrypted(1), compressed(2)
    uint8_t channelID;  // Logical channel (0 = unreliable, 1 = reliable, 2-255 = game)
};
#pragma pack(pop)

static_assert(sizeof(PacketHeader) == 12, "PacketHeader layout changed - update C# struct too");

// Embedding magic number to reject stray datagrams
constexpr uint32_t NETENGINE_PACKET_MAGIC = 41456701; // "AEg" + version in big-endian

// PacketFlags bit positions within PacketHeader::flags
constexpr uint8_t PKT_FLAG_RELIABLE = 0x01; // Packet requires ACK and retransmit
constexpr uint8_t PKT_FLAG_ENCRYPTED = 0x02; // Packet payload is AES-128 encrypted
constexpr uint8_t PKT_FLAG_COMPRESSED = 0x04; // Packet payload is bitstream-compressed


//GenreID - identifies which genre module handles game-specific logic
enum class GenreID : uint16_t
{
    None = 0,
    FPS = 1,
    RTS = 2,
    MMO = 3,
    Custom = 4,
};

//Engine Type - identifies which game engine the C# layer is driving
//Passed to NetEngine_Init so engine-specific adapter paths are selected

enum class EngineType : uint16_t
{
    None = 0,
    Unity = 1,
    Unreal = 2,
    Custom = 3,
};