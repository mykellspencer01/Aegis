/*
    @brief Top-level transport combining UDPSocket + AckManager into a unified, per-session un/reliable channel

    * ReliableChannel is the object the engine calls for all network I/O
    * Internally owns:
        + One UDPSocket for raw send/receive
        + One AckManager per connected peer for reliable delivery tracking
        + A thread-safe send/receive queue so the update and worker threads can push packets without racing
    * Channel Semantics:
        + channelID == 0 -> unreliable, unordered (position updates)
        + channelID == 1 -> reliable, ordered (game events)
        + channelID == 2 -> genre-module-defined (such as fps events)
    * Thread model:
        + Internally synchronized with std::mutex
        + Engine's network thread calls Tick() at a fixed rate
            - drains incoming datagrams
            - dispatches acks
            - performs restransmits
        + Other threads call Send() safely at any time
*/

#pragma once

#include "UDPSocket.h"
#include "AckManager.h"
#include "../EngineTypes.h"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <functional>
#include <memory>

namespace Aegis {
namespace Transport {

    //* Fully decoded packet delivered to the engine's receive callback 
    struct IncomingPacket {
        PeerID fromPeer; // logical peer that sent the packet
        uint8_t channelID; // channel the packet arrived on
        uint8_t payload[NETENGINE_MAX_PACKET_SIZE]; //decrypted, serialized bytes
        uint32_t payloadLength; // # of valid bytes in payload
    };
    
    //Signature of the function called by ReliableChannel when a valid packet is received, decrypted, and de-duped
    using PacketReceivedCallback = std::function<void(const IncomingPacket&)>;

    
     // * Unified Send/Receive Interface for the engine
    class ReliableChannel
    {
        public:
            ReliableChannel();
            ~ReliableChannel();
            NetResult Init(uint16_t localPort, PacketReceivedCallback callback);
            void Shutdown(); // closes socket and frees all peer states
            NetResult AddPeer(PeerID id, const RemoteEndpoint& endpoint);
            void RemovePeer(PeerID id); //removes a peer and discards its ack window
            NetResult Send(PeerID peer, uint8_t channelID, const uint8_t* data, uint32_t length); // send packet to specified peer
            void Tick(double nowSeconds); // drives transport layer for a tick. Receives all pennding datagrams, processes acks, fires retransmits for overdue reliable packets
            uint16_t GetLocalPort() const; //returns local UDP port the channel is bound to

        private:
            //* Per-peer state aggregation
            struct PeerState {
                RemoteEndpoint endpoint; //peer's UDP address
                AckManager manager; // sliding-window trACKer for this peer
            };

            // Internal Helpers
            uint32_t BuildPacket(PeerState& peer,
                                uint8_t channelID,
                                bool reliable,
                                const uint8_t* payload,
                                uint32_t payloadLength,
                                uint8_t* outBuffer,
                                double nowSeconds);
              
            void HandleDatagram(const RemoteEndpoint& fromAddr,
                                const uint8_t* raw,
                                uint32_t rawLength);

            //Member Variables
            UDPSocket t_socket; 
            PacketReceivedCallback t_callback;
            std::unordered_map<PeerID, PeerState> t_peers;
            std::unordered_map<std::string, PeerID> t_addrToPeer;
            mutable std::mutex t_mutex; 
            bool t_initialized;
            double t_rttThreshold;
            double t_now = 0.0; // last time passed to Tick(); timestamps sends for retransmit tracking
            
            uint8_t t_sendBuffer[NETENGINE_MAX_PACKET_SIZE];
    };
} // namespace Transport
} // namespace Aegis