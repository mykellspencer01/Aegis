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

static Aegis::Transport::ReliableChannel e_channel; //primar transport layer
static Aegis::Replication::SnapshotManager e_snapshots; //world-state snapshot and delta replication layer
static Aegis::Replication::RpcDispatcher e_rpc; //RPC handler registry and dispatcher
static Aegis::Crypto::CryptoProvider e_crypto; //packet encryption provider
static bool e_initialized = false;

//Genre module instances
static std::unique_ptr<Aegis::Genres::FpsModule> e_fps;
static std::unique_ptr<Aegis::Genres::RtsModule> e_rts; 
static std::unique_ptr<Aegis::Genres::MmoModule> e_mmo;

// Managed receive callback bridge
using ManagedReceivedCallback = void(*)(
    uint64_t peerID, uint8_t channelID,
    const uint8_t* payload, uint32_t payloadLength);

static ManagedReceivedCallback e_managedCallback = nullptr;

//Internal receive callback
static void OnPacketReceived(const Aegis::Transport::IncomingPacket& pkt)
{
    if(pkt.channelID == 1)
    {
        e_rpc.Dispatch(pkt.fromPeer, pkt.payload, pkt.payloadLength);
        return;
    }

    if(pkt.channelID == 2)
    {
        Aegis::Serializer::BitReader reader(pkt.payload, pkt.payloadLength);
        auto* mod = Aegis::Genres::RegistryModule::Instance()
            .Get(static_cast<GenreID>(pkt.channelID));
        if(mod) mod -> OnPacketReceived(pkt.fromPeer, pkt.channelID, reader);
    }

    //forward each rcvd packet to managed layer, regardless of channel
    if(e_managedCallback != nullptr)
        e_managedCallback(pkt.fromPeer, pkt.channelID, pkt.payload, pkt.payloadLength);
}

// * Exported C ABI - funcs C# DLLimport binds to
extern "C" {

    //Core lifecycle
    NETENGINE_API int32_t Aegis_Init(uint16_t localPort, uint8_t EngineType)
    {
        if(e_initialized) return static_cast<int32_t>(NetResult::AlreadyInitialized);

        //Register all genre modules into registry
        auto& registry = Aegis::Genres::RegistryModule::Instance();
        e_fps = std::make_unique<Aegis::Genres::FpsModule>();
        e_rts = std::make_unique<Aegis::Genres::RtsModule>();
        e_mmo = std::make_unique<Aegis::Genres::MmoModule>();

        registry.Register(std::make_unique<Aegis::Genres::FpsModule>());
        registry.Register(std::make_unique<Aegis::Genres::RtsModule>());
        registry.Register(std::make_unique<Aegis::Genres::MmoModule>());

        //Open transport channel
        NetResult result = e_channel.Init(localPort, OnPacketReceived);
        if(result != NetResult::Ok) return static_cast<int32_t>(result);

        e_initialized = true;
        return static_cast<int32_t>(NetResult::Ok);
    }

    NETENGINE_API void Aegis_SetReceiveCallback(ManagedReceivedCallback callback)
    {
        e_managedCallback = callback;
    }

    NETENGINE_API void Aegis_Shutdown()
    {
        if(!e_initialized) return;

        Aegis::Genres::RegistryModule::Instance().ShutdownAll();
        e_channel.Shutdown();
        e_initialized = false;
    }

    NETENGINE_API void Aegis_Tick(double deltaSeconds)
    {
        if(!e_initialized) return;

        e_channel.Tick(deltaSeconds);
        Aegis::Genres::RegistryModule::Instance().TickAll(deltaSeconds);
    }

    NETENGINE_API u_int16_t Aegis_GetLocalPort()
    {
        return e_channel.GetLocalPort();
    }

    // * Peer Management
    NETENGINE_API int32_t Aegis_AddPeer(uint64_t peerID, const char* ip, uint16_t port)
    {
        if(!e_initialized) return static_cast<int32_t>(NetResult::NotInitialized);
        if(!ip) return static_cast<int32_t>(NetResult::InvalidArg);

        Aegis::Transport::RemoteEndpoint endpoint {};
        strncpy(endpoint.ip, ip, sizeof(endpoint.ip) - 1);
        endpoint.port = port;

        NetResult result = e_channel.AddPeer(peerID, endpoint);

        //notify genre modules
        if(result == NetResult::Ok) Aegis::Genres::RegistryModule::Instance().NotifyPeerConnected(peerID);

        return static_cast<int32_t>(result);
    }

    NETENGINE_API void Aegis_RemovePeer(uint64_t peerID)
    {
        if(!e_initialized) return;
        e_channel.RemovePeer(peerID);

        Aegis::Genres::RegistryModule::Instance().NotifyPeerDisconnected(peerID);
    }

    // * Sending
    NETENGINE_API int32_t Aegis_Send(uint64_t peerID, uint8_t channelID, const uint8_t* data, uint32_t length)
    {
        if(!e_initialized) return static_cast<int32_t>(NetResult::NotInitialized);
        return static_cast<int32_t>(e_channel.Send(peerID, channelID, data, length));
    }

    // * Genre Selection
    NETENGINE_API int32_t Aegis_SetGenre(uint8_t genreID)
    {
        if(!e_initialized) return static_cast<int32_t>(NetResult::NotInitialized);

        auto* mod = Aegis::Genres::RegistryModule::Instance().Get(static_cast<GenreID>(genreID));
        if(!mod) return static_cast<int32_t>(NetResult::GenreModuleNotFound);

        return static_cast<int32_t>(mod -> OnInit());
    }

    // * Crypto
    NETENGINE_API void Aegis_SetSessionKey(const uint8_t* key)
    {
        if(!key) return;
        e_crypto.SetSeshKey(key);
    }

    NETENGINE_API int32_t Aegis_HasSessionKey()
    {
        return e_crypto.HasKey() ? 1 : 0;
    }

    // * Snapshot Replication
    NETENGINE_API void Aegis_CaptureSnapshot(uint32_t tick)
    {
        if(!e_initialized) return;
        e_snapshots.CaptureSnapshot(tick);
    }

    NETENGINE_API uint32_t Aegis_GetLatestTick()
    {
        return e_snapshots.GetLatestTick();
    }
} //extern "C"