/* (Remote Procedure Call Dispatcher)
    * Call registration and dispatch system
        - allows game code to register typed handler funcs
            + invoked automatically when a tagged, reliable packet arrives from the ntwrk
        - Each RPC is identified by an ID
            + callers register an std::function for each RPC they want to handle
            + when a pkt arrives on reliable channel
                > channelID == 1
                > dispatcher reads 16-bit RPC ID from the payload's front
                > calls matching handler
    * Thread Safety
        - Register and dispatch functions are protected
            + internal mutex
            + can be called safely from different threads
*/

#pragma once

#include "EngineTypes.h"
#include "../Serializer/BitRead.h"
#include "../Serializer/BitWrite.h"
#include <cstdint>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <vector>

namespace NetEngine {
namespace Replication {

    //RPC identifer type and rpc handler handler func signature
    using RpcID = uint16_t;
    using RpcHandler = std::function<void(PeerID sender, Serializer::BitReader& payload)>;

    class RpcDispatcher {

        public:
            RpcDispatcher() = default;
            NetResult RegisterRpc(RpcID id, RpcHandler handler);
            void UnregisteredRpc(RpcID id);
            NetResult Dispatch(PeerID sender, const uint8_t* payload, uint32_t payloadLength);
            NetResult SerializeCall(RpcID id, uint8_t* outBuffer, uint32_t bufferSize,
                                    std::function<void(Serializer::BitWriter&)> writeArgs,
                                    uint32_t& outLength);
            
        private:
            std::unordered_map<RpcID, RpcHandler> t_handlers; //registered rpc table
            mutable std::mutex t_mutex; // guard for t_handlers
    };
} //namepsace Replication
} //namespace NetEngine