/*
    * Delta-snapshot replication model
        - On each ntwrk tick, the server calls a function
            + serialize current state of all objects
            + turn the objects into a snapshot struct
        - On each client, a function will compare the latest snapshot to the last ack'd snapshot
            + produces a compact DeltaPacket
            + Contains only fields that have been altered
        - Clients recieve DeltaPackets and call a function to apply them
            + reconstructs current state by layering deltas on top of their last-known baseline
    * Object Registration
        - Any C++ object that wants to be replicated must:
            + implement replicability
            + register itself with a registration function
        - Management assigns a stable object ID used in all deltas
    * Bandwidth Control
        - Object priority for controlling how an object appears in delta packets
        - High-priority objects (for ex. the local player) are included every tick
        - Low-priority objects are included less frequently using an accumulator
*/

#pragma once

#include "../EngineTypes.h"
#include "../Serializer/BitWrite.h"
#include "../Serializer/BitRead.h"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>

namespace Aegis {
namespace Replication {

    using ObjectID = uint32_t;
    constexpr ObjectID INVALID_OBJECT_ID = 0;
    constexpr uint32_t MAX_SNAPSHOT_OBJS = 4096;

    //Hydra Replication Class 
    //Each object must implement this interface
    class Hydra {
        public:
            virtual ~Hydra() = default;
            virtual void Serialize(Serializer::BitWriter& writer) const = 0;
            virtual void Deserialize(Serializer::BitReader& reader) const = 0;
            virtual uint8_t GetPriority() const { return 128; }
    };

    //* Snapshot - serialized world state
    struct ObjectSnapshooter {
        Replication::ObjectID ObjectID; // which object ("Replication::" for portibility fixing with CLANG)
        uint8_t data[NETENGINE_MAX_PACKET_SIZE]; //serialized bytes
        uint32_t dataLength; // valid byte count
    };

    struct Snapshooter {
        uint32_t tick = 0xFFFFFFFFu; //server tick #; sentinel marks an unwritten slot
        std::vector<ObjectSnapshooter> objects; //one entry per registered object
    };

    //* Delta Packet - compact diff between the two snapshots
    struct ObjectDelta {
        Replication::ObjectID ObjectID;
        bool isNew;
        bool isDeleted;
        uint8_t data[NETENGINE_MAX_PACKET_SIZE]; // delta payload bytes
        uint32_t dataLength;
    };

    struct DeltaPacket {
        uint32_t fromTick; //baseline tick (last ack'd snapshot from client)
        uint32_t toTick; // this packet's tick
        std::vector<ObjectDelta> deltas; // changed objects
    };

    //* Snapshot Manager
    class SnapshotManager {
        public:
            SnapshotManager();
            
            ObjectID RegisterObject(Hydra* obj, uint8_t priority = 128);
            void UnregisterObject(ObjectID id);

            //Server Side
            void CaptureSnapshot(uint32_t tick);
            NetResult CreateDelta(uint32_t lastClientAckdTick, //generate delta packet for this specific client
                uint32_t currentTick, 
                DeltaPacket& outPacket) const;
            
            //Client Side
            NetResult ApplyDelta(const DeltaPacket& packet); //applies received delta packet to update local object states
            uint32_t GetLatestTick() const; // returns most recent snapshot tick
            const Snapshooter* FindSnapshot(uint32_t tick) const;

        private:
            //per-object registry entry
            struct Entry{
                Hydra* obj;
                uint8_t priority;
                float accumulator; // priority accumulator; object only included when the float is >= 1.0
                bool isPendingDelete;
            };
            
            std::unordered_map<ObjectID, Entry> t_objects;
            ObjectID t_nextID;

            //Buffer of recent snapshots
            static constexpr uint32_t SNAPSHOT_HIST = 32;
            Snapshooter t_history[SNAPSHOT_HIST];
            uint32_t t_historyCount;
            uint32_t t_latestTick;
            
    };
} //namespace Replication
} //namespace Aegis
