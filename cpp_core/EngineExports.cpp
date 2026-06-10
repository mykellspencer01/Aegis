/*
    * FLAT C ABI Export Layer
*/

#include "EngineTypes.h"
#include "Transport/ReliableChannel.h"
#include "Replication/SnapshotManagement.h"
#include "Replication/RpcDispatcher.h"
#include "Packets/CryptoProvider.h"
#include "Genres/GenreModule.h"
#include "Genres/FPS/FpsModule.h"
#include "Genres/RTS/RtsModule.h"
#include "Genres/MMO/MmoModule.h"

#include <cstring>
#include <memory>

static NetEngine::Transport::ReliableChannel e_channel; //primar transport layer
static NetEngine::Replication::SnapshotManager e_snapshots; //world-state snapshot and delta replication layer
static NetEngine::Replication::RpcDispatcher e_rpc; //RPC handler registry and dispatcher
static NetEngine::Crypto::CryptoProvider e_crypto; //packet encryption provider
static bool e_initialized = false;

//Genre module instances
static std::unique_ptr<NetEngine::Genres::FpsModule> e_fps;
static std::unique_ptr<NetEngine::Genres::RtsModule> e_rts; 
static std::unique_ptr<NetEngine::Genres::MmoModule> e_mmo;

//Internal receive callback
static void OnPacketReceived(const NetEngine::Transport::IncomingPacket& pkt)
{
    if(pkt.channelID == 1)
    {
        e_rpc.Dispatch(pkt.fromPeer, pkt.payload, pkt.payloadLength);
        return;
    }

    if(pkt.channelID == 2)
    {
        NetEngine::Serializer::BitReader reader(pkt.payload, pkt.payloadLength);
        auto* mod = NetEngine::Genres::RegistryModule::Instance()
            .Get(static_cast<GenreID>(pkt.channelID));
        if(mod) mod -> OnPacketReceived(pkt.fromPeer, pkt.channelID, reader);
    }
}

// * Exported C ABI - funcs C# DLLimport binds to
extern "C" {

    //Core lifecycle
    NETENGINE_API int32_t NetEngine_Init(uint16_t localPort, uint8_t EngineType)
    {
        if(e_initialized) return static_cast<int32_t>(NetResult::AlreadyInitialized);

        //Register all genre modules into registry
        auto& registry = NetEngine::Genres::RegistryModule::Instance();
        e_fps = std::make_unique<NetEngine::Genres::FpsModule>();
        e_rts = std::make_unique<NetEngine::Genres::RtsModule>();
        e_mmo = std::make_unique<NetEngine::Genres::MmoModule>();

        registry.Register(std::make_unique<NetEngine::Genres::FpsModule>());
        registry.Register(std::make_unique<NetEngine::Genres::RtsModule>());
        registry.Register(std::make_unique<NetEngine::Genres::MmoModule>());

        //Open transport channel
        NetResult result = e_channel.Init(localPort, OnPacketReceived);
        if(result != NetResult::Ok) return static_cast<int32_t>(result);

        e_initialized = true;
        return static_cast<int32_t>(NetResult::Ok);
    }

    NETENGINE_API void NetEngine_Shutdown()
    {
        if(!e_initialized) return;

        NetEngine::Genres::RegistryModule::Instance().ShutdownAll();
        e_channel.Shutdown();
        e_initialized = false;
    }

    NETENGINE_API void NetEngine_Tick(double deltaSeconds)
    {
        if(!e_initialized) return;

        e_channel.Tick(deltaSeconds);
        NetEngine::Genres::RegistryModule::Instance().TickAll(deltaSeconds);
    }

    NETENGINE_API u_int16_t NetEngine_GetLocalPort()
    {
        return e_channel.GetLocalPort();
    }

    // * Peer Management
    NETENGINE_API int32_t NetEngine_AddPeer(uint64_t peerID, const char* ip, uint16_t port)
    {
        if(!e_initialized) return static_cast<int32_t>(NetResult::NotInitialized);
        if(!ip) return static_cast<int32_t>(NetResult::InvalidArg);

        NetEngine::Transport::RemoteEndpoint endpoint {};
        strncpy(endpoint.ip, ip, sizeof(endpoint.ip) - 1);
        endpoint.port = port;

        NetResult result = e_channel.AddPeer(peerID, endpoint);

        //notify genre modules
        if(result == NetResult::Ok) NetEngine::Genres::RegistryModule::Instance().NotifyPeerConnected(peerID);

        return static_cast<int32_t>(result);
    }

    NETENGINE_API void NetEngine_RemovePeer(uint64_t peerID)
    {
        if(!e_initialized) return;
        e_channel.RemovePeer(peerID);

        NetEngine::Genres::RegistryModule::Instance().NotifyPeerDisconnected(peerID);
    }

    // * Sending
    NETENGINE_API int32_t NetEngine_Send(uint64_t peerID, uint8_t channelID, const uint8_t* data, uint32_t length)
    {
        if(!e_initialized) return static_cast<int32_t>(NetResult::NotInitialized);
        return static_cast<int32_t>(e_channel.Send(peerID, channelID, data, length));
    }

    // * Genre Selection
    NETENGINE_API int32_t NetEngine_SetGenre(uint8_t genreID)
    {
        if(!e_initialized) return static_cast<int32_t>(NetResult::NotInitialized);

        auto* mod = NetEngine::Genres::RegistryModule::Instance().Get(static_cast<GenreID>(genreID));
        if(!mod) return static_cast<int32_t>(NetResult::GenreModuleNotFound);

        return static_cast<int32_t>(mod -> OnInit());
    }

    // * Crypto
    NETENGINE_API void NetEngine_SetSessionKey(const uint8_t* key)
    {
        if(!key) return;
        e_crypto.SetSeshKey(key);
    }

    NETENGINE_API int32_t NetEngine_HasSessionKey()
    {
        return e_crypto.HasKey() ? 1 : 0;
    }

    // * Snapshot Replication
    NETENGINE_API void NetEngine_CaptureSnapshot(uint32_t tick)
    {
        if(!e_initialized) return;
        e_snapshots.CaptureSnapshot(tick);
    }

    NETENGINE_API uint32_t NetEngine_GetLatestTick()
    {
        return e_snapshots.GetLatestTick();
    }
} //extern "C"