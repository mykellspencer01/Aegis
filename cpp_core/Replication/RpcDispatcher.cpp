#include "RpcDispatcher.h"
#include <cstdio>

#ifdef NETENGINE_LOGGING
    #define NE_LOG(fmt, ...) std::printf("[RpcDispatcher] " fmt "\n", ##__VA_ARGS__)
#else
    #define NE_LOG(fmt, ...) ((void)0)
#endif

namespace Aegis {
namespace Replication {

    NetResult RpcDispatcher::RegisterRpc(RpcID id , RpcHandler handler)
    {
        if(id == 0 || !handler) return NetResult::InvalidArg;

        std::lock_guard<std::mutex> lk(t_mutex);
        t_handlers[id] = std::move(handler);
        NE_LOG("RPC %u registered", id);
        return NetResult::Ok;
    }

    NetResult RpcDispatcher::Dispatch(PeerID sender, const uint8_t* payload, uint32_t payloadLength)
    {
        if(!payload || payloadLength < 2) return NetResult::InvalidArg;

        Serializer::BitReader reader(payload, payloadLength);
        RpcID id = static_cast<RpcID>(reader.ReadBits(16));

        RpcHandler handler;
        {
            std::lock_guard<std::mutex> lk(t_mutex);
            auto h = t_handlers.find(id);
            if(h == t_handlers.end())
            {
                NE_LOG("No handler for RPC %u from peer %llu", id, (unsigned long long)sender);
                return NetResult::GenreModuleNotFound;
            }
            handler = h -> second;
        }
        handler(sender, reader);
        return NetResult::Ok;
    }

    NetResult RpcDispatcher::SerializeCall(
        RpcID id, uint8_t* outBuffer, uint32_t bufferSize,
        std::function<void(Serializer::BitWriter&)> writeArgs,
        uint32_t& outLength)
    {
        Serializer::BitWriter writer(outBuffer, bufferSize);
        writer.WriteBits(id, 16);
        if(writeArgs) writeArgs(writer);
        writer.Flush();

        outLength = writer.GetBytesWritten();
        return writer.HasOverflowed() ? NetResult::BufferOverflow : NetResult::Ok;
    }

    void RpcDispatcher::UnregisteredRpc(RpcID id)
    {
        std::lock_guard<std::mutex> lk(t_mutex);
        t_handlers.erase(id);
    }
} //namespace Replication
} //namespace Aegis