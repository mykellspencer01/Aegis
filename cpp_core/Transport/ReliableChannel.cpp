#include "ReliableChannel.h"
#include <cstring>
#include <cassert>
#include <cstdio>
#include <sstream>

#ifdef NETENGINE_LOGGING
    #define NE_LOG(fmt, ...) std::printf("[ReliableChannel] " fmt "\n", ##__VA_ARGS__)
#else
    #define NE_LOG(fmt, ...) ((void)0)
#endif

namespace Aegis {
namespace Transport {

    //Constructor/Destructor
    ReliableChannel::ReliableChannel() : t_socket(0), t_initialized(false), t_rttThreshold(0.1) {}
    ReliableChannel::~ReliableChannel() { Shutdown(); }

    //Init/Shutdown
    NetResult ReliableChannel::Init(uint16_t localPort, PacketReceivedCallback callback) 
    {
        if(t_initialized)
        {
            NE_LOG("Init() called on already-initialized channel");
            return NetResult::AlreadyInitialized;
        }
        if(!callback)
        {
            NE_LOG("Init() requires a valid receieve callback.");
            return NetResult::InvalidArg;
        }
        t_callback = std::move(callback);

        //Reconstruct socket with requested port (t_socket defaults to port 0)
        //Recreate by placement or call Open() directly 
        t_socket = UDPSocket(localPort);
        NetResult result = t_socket.Open();
        if(result != NetResult::Ok)
        {
            NE_LOG("Failed to open UDP socket on port %u", localPort);
            return result;
        }

        t_initialized = true;
        NE_LOG("Channel initialized on port %u", t_socket.GetLocalPort());
        return NetResult::Ok;
    }

    void ReliableChannel::Shutdown() 
    {
        if(!t_initialized) return;

        std::lock_guard<std::mutex> lk(t_mutex);
        t_peers.clear();
        t_addrToPeer.clear();
        t_socket.Close();
        t_initialized = false;
        NE_LOG("Channel shut down.");
    }

    // Managing Peers
    NetResult ReliableChannel::AddPeer(PeerID id, const RemoteEndpoint& endpoint)
    {
        std::lock_guard<std::mutex> lk(t_mutex);

        if(t_peers.size() >= NETENGINE_MAX_PLAYERS) return NetResult::SessionFull;

        PeerState state;
        state.endpoint = endpoint;
        t_peers.emplace(id, std::move(state));

        //Build "ip:port" key for reverse lookip (HandleDatagram)
        std::ostringstream key;
        key << endpoint.ip << ":" << endpoint.port;
        t_addrToPeer[key.str()] = id;

        NE_LOG("Peer %llu added (%s:%u)", (unsigned long long)id, endpoint.ip, endpoint.port);
        return NetResult::Ok;
    }

    void ReliableChannel::RemovePeer(PeerID id) 
    {
        std::lock_guard<std::mutex> lk(t_mutex);
        auto p = t_peers.find(id);
        if (p == t_peers.end()) return;

        //Remove reverse lookup entry
        std::ostringstream key;
        key << p -> second.endpoint.ip << ":" << p -> second.endpoint.port;
        t_addrToPeer.erase(key.str());

        t_peers.erase(p);
        NE_LOG("Peer %llu removed", (unsigned long long) id);
    }

    //Send
    NetResult ReliableChannel::Send(PeerID peer, uint8_t channelID, const uint8_t* data, uint32_t length) 
    {
        if(!t_initialized) return NetResult::NotInitialized;

        std::lock_guard<std::mutex> lk(t_mutex);

        auto p = t_peers.find(peer);
        if(p == t_peers.end())
        {
            NE_LOG("Send() to unknown peer %llu", (unsigned long long)peer);
            return NetResult::UnknownPeer;
        }

        PeerState& state = p -> second;
        bool reliable = (channelID == 1); //channel 1 is the reliable one

        // TODO: Build packet can encrypt + compress here on later implementations
        //write a minimal header
        uint32_t packetLength = BuildPacket(state, channelID, reliable, data, length, t_sendBuffer, 0.0);
        int32_t sent = t_socket.Send(state.endpoint, t_sendBuffer, packetLength);
        return (sent >= 0) ? NetResult::Ok : NetResult::SocketError;
    }

    //* Tick - receive loop and retransmit
    void ReliableChannel::Tick(double currentTime)
    {
        if(!t_initialized) return;

        static uint8_t recvBuffer[NETENGINE_MAX_PACKET_SIZE];

        //Drain available datagrams
        while(true)
        {
            RemoteEndpoint from{};
            int32_t received = t_socket.Receive(from, recvBuffer, sizeof(recvBuffer));
            if(received <= 0) break;
            HandleDatagram(from, recvBuffer, static_cast<uint32_t>(received));
        }

        //Retransmit overdue packets
        {
            std::lock_guard<std::mutex> lk(t_mutex);
            for(auto& [id, state] : t_peers)
            {
                auto retransmits = state.manager.GetPendingRetransmits(currentTime, t_rttThreshold);
                for(const auto& packet : retransmits)
                {
                    t_socket.Send(state.endpoint, packet.data, packet.dataLength);
                    NE_LOG("Retransmit sequence %u to peer %llu", packet.sequence, (unsigned long long)id);
                }
            }
        }
    }

    uint16_t ReliableChannel::GetLocalPort() const
    {
        return t_socket.GetLocalPort();
    }

    //* Build Packet header assembly and payload goes into outBuffer
    uint32_t ReliableChannel::BuildPacket(PeerState& peer,
                                        uint8_t channelID,
                                        bool reliable,
                                        const uint8_t* payload,
                                        uint32_t payloadLength,
                                        uint8_t* outBuffer,
                                        double currentTime)
    {
        PacketHeader hdr{};
        hdr.magic = NETENGINE_PACKET_MAGIC;
        hdr.channelID = channelID;
        hdr.sequenceNum = peer.manager.GetNextOutSequence();
        hdr.ack = peer.manager.GetRemoteAck();
        hdr.ackBits = peer.manager.GetRemoteAckBits();
        hdr.flags = reliable ? PKT_FLAG_RELIABLE : 0;
        hdr.channelID = channelID;

        //Write header (big endian)
        uint32_t offset = 0;
        auto writeU32 = [&](uint32_t a)
        {
            outBuffer[offset++] = (a >> 24) & 0xFF;
            outBuffer[offset++] = (a >> 16) & 0xFF;
            outBuffer[offset++] = (a >> 8) & 0xFF;
            outBuffer[offset++] = a & 0xFF;
        };

        auto writeU16 = [&](uint16_t b)
        {
            outBuffer[offset++] = (b >> 8) & 0xFF;
            outBuffer[offset++] = b & 0xFF;
        };

        writeU32(hdr.magic);
        writeU16(hdr.sequenceNum);
        writeU16(hdr.ack);
        writeU16(hdr.ackBits);
        outBuffer[offset++] = hdr.flags;
        outBuffer[offset++] = hdr.channelID;

        assert(offset == sizeof(PacketHeader));

        //Appending the payload
        uint32_t safeLength = std::min(payloadLength, static_cast<uint32_t>(NETENGINE_MAX_PACKET_SIZE - sizeof(PacketHeader)));
        std::memcpy(outBuffer + offset, payload, safeLength);
        offset += safeLength;

        return offset;

    }
    
    //* Handle Datagram for parsing raw bytes recvd from socket
    void ReliableChannel::HandleDatagram(const RemoteEndpoint& fromAddr, const uint8_t* raw, uint32_t rawLength)
    {
        if(rawLength < sizeof(PacketHeader)) 
        {
            NE_LOG("Datagram too short: %u bytes", rawLength);
            return;
        }

        //Parse Header
        uint32_t offset = 0;
        auto readU32 = [&]() -> uint32_t
        {
            uint32_t c = (uint32_t(raw[offset]) << 24) |
                         (uint32_t(raw[offset + 1]) << 16) |
                         (uint32_t(raw[offset + 2]) << 8) |
                         raw[offset + 3];
            offset += 4;
            return c;
        };

        auto readU16 = [&]() -> uint16_t
        {
            uint16_t d = (uint16_t(raw[offset]) << 8) | raw[offset + 1];
            offset += 2;
            return d;
        };

        PacketHeader hdr{};
        hdr.magic = readU32();
        hdr.sequenceNum = readU16();
        hdr.ack = readU16();
        hdr.ackBits = readU16();
        hdr.flags = raw[offset++];
        hdr.channelID = raw[offset++];

        NE_LOG("Received magic = 0x%08X Expected 0x%08X from %s:%u",
            hdr.magic, NETENGINE_PACKET_MAGIC, fromAddr.ip, fromAddr.port);

        //Validate magic. Drop packets from unknown sources
        if(hdr.magic != NETENGINE_PACKET_MAGIC){
            NE_LOG("Magic mismatch - dropping");
            return;
        }

        //Resolve sender to a Peer ID
        std::ostringstream key;
        key << fromAddr.ip << ":" << fromAddr.port;
        NE_LOG("Looking up key: %s", key.str().c_str());

        PeerID senderID = PEER_NOUGHT;
        {
            std::lock_guard<std::mutex> lk(t_mutex);
            auto p = t_addrToPeer.find(key.str());

            //debugging
            NE_LOG("Map contains %zu entries", t_addrToPeer.size());
            for(auto& [key, peer] : t_addrToPeer)
                NE_LOG(" Key = '%s' -> peer %llu", key.c_str(), (unsigned long long)peer);
            auto temp = t_addrToPeer.find(key.str());
            NE_LOG("Find result: %s", (temp == t_addrToPeer.end()) ? "NOT FOUND" : "FOUND");

            if(p == t_addrToPeer.end()) return;
            senderID = p -> second;

            PeerState& state = t_peers.at(senderID);

            if(state.manager.IsDuplicate(hdr.sequenceNum)) return;
            NE_LOG("IsDuplicate check: %s", state.manager.IsDuplicate(hdr.sequenceNum) ? "DUPED - dropping" : "NO DUPE");
            state.manager.RecordReceived(hdr.sequenceNum);

            state.manager.ProcessAck(hdr.ack, hdr.ackBits); //process ack info this packet carries about sent pkts
        }

        //Fire app callback
        IncomingPacket pkt{};
        pkt.fromPeer = senderID;
        pkt.channelID = hdr.channelID;
        pkt.payloadLength = rawLength - static_cast<uint32_t>(sizeof(PacketHeader));
        std::memcpy(pkt.payload, raw + sizeof(PacketHeader), pkt.payloadLength);

        if(t_callback) t_callback(pkt);
    }
} //namespace Transport
} //namespace Aegis


