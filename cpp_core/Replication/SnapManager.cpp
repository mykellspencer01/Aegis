#include "SnapshotManagement.h"
#include <cstring>
#include <algorithm>
#include <cstdio>

#ifdef NETENGINE_LOGGING
    #define NE_LOG(fmt, ...) std::printf("[Replication] " fmt "\n", ##__VA_ARGS__)
#else
    #define NE_LOG(fmt, ...) ((void) 0)
#endif

namespace Aegis{
namespace Replication{

    SnapshotManager::SnapshotManager()
        : t_nextID(1),
        t_historyCount(0),
        t_latestTick(0)
    {}

    //* Object Registry
    ObjectID SnapshotManager::RegisterObject(Hydra* obj, uint8_t priority)
    {
        if(!obj || t_objects.size() >= MAX_SNAPSHOT_OBJS) return INVALID_OBJECT_ID;

        ObjectID id = t_nextID++;
        Entry E;
        E.obj = obj;
        E.priority = priority;
        E.accumulator = 1.0f;
        E.isPendingDelete = false;
        t_objects.emplace(id, E);
        NE_LOG("Object %u registered (priority %u)", id, priority);

        return id;
    }

    void SnapshotManager::UnregisterObject(ObjectID id)
    {
        auto ufo = t_objects.find(id);
        if(ufo != t_objects.end())
        {
            ufo -> second.isPendingDelete = true;
            NE_LOG("Object %u is pending ultimate destruction", id);
        }
    }

    //* Server: Capturing the snapshot
    void SnapshotManager::CaptureSnapshot(uint32_t tick)
    {
        //selecting ring buffer slot
        uint32_t slot = tick % SNAPSHOT_HIST;
        Snapshooter& snap = t_history[slot];
        snap.tick = tick;
        snap.objects.clear();
        snap.objects.reserve(t_objects.size());

        static uint8_t tempBuffer[NETENGINE_MAX_PACKET_SIZE];

        for(auto& [id, entry] : t_objects)
        {
            if(entry.isPendingDelete) continue;

            //Priority accumulator
            //High priority always hame accum >= 1
            //low-priority objects accumulate over time until flushing
            entry.accumulator += entry.priority;
            if(entry.accumulator < 1.0f) continue;
            entry.accumulator = 0.0f;

            //Serialize object into tempBuffer
            Serializer::BitWriter w(tempBuffer, sizeof(tempBuffer));
            entry.obj -> Serialize(w);
            w.Flush();

            ObjectSnapshooter os;
            os.ObjectID = id;
            os.dataLength = w.GetBytesWritten();
            std::memcpy(os.data, tempBuffer, os.dataLength);
            snap.objects.push_back(os);
        }

        //Purge isPendingDelete objects
        for(auto purge = t_objects.begin(); purge != t_objects.end();)
        {
            purge = purge -> second.isPendingDelete ? t_objects.erase(purge) : std::next(purge);
        }

        t_latestTick = tick;
        if(t_historyCount < SNAPSHOT_HIST) ++t_historyCount;
        NE_LOG("Snapshot captured tick = %u objects = %zu", tick, snap.objects.size());
    }

    //* Server - generate deltas
    NetResult SnapshotManager::CreateDelta(uint32_t lastClientAckdTick, uint32_t currentTick, DeltaPacket& outPacket) const
    {
        const Snapshooter* current = FindSnapshot(currentTick);
        if(!current) return NetResult::NotInitialized;

        outPacket.fromTick = lastClientAckdTick;
        outPacket.toTick = currentTick;
        outPacket.deltas.clear();
        
        const Snapshooter* baseline = (lastClientAckdTick > 0) ? FindSnapshot(lastClientAckdTick) : nullptr;

        for(const auto& currentObj : current -> objects)
        {
            ObjectDelta deltaObj;
            deltaObj.ObjectID = currentObj.ObjectID;
            deltaObj.isDeleted = false;

            //check if object existed in baseline
            const ObjectSnapshooter* baselineObj = nullptr;
            if(baseline)
            {
                for(const auto& b : baseline -> objects)
                    if(b.ObjectID == currentObj.ObjectID) { baselineObj = &b; break; }
            }

            deltaObj.isNew = (baselineObj == nullptr);

            //simple delta, if data vs baseline is different -> include it
            bool isChanged = deltaObj.isNew 
                            || (currentObj.dataLength != baselineObj -> dataLength)
                            || (std::memcmp(currentObj.data, baselineObj -> data, currentObj.dataLength) != 0);
            
            if(!isChanged) continue;

            deltaObj.dataLength = currentObj.dataLength;
            std::memcpy(deltaObj.data, currentObj.data, currentObj.dataLength);
            outPacket.deltas.push_back(deltaObj);
        }

        if(baseline) //include isDeleted entries for objs in baseline but not in current
        {
            for(const auto& baselineObj : baseline -> objects)
            {
                bool isFound = false;
                for(const auto& d : outPacket.deltas)
                    if(d.ObjectID == baselineObj.ObjectID) { isFound = true; break; }
                for(const auto& currentObj : current -> objects)
                    if(currentObj.ObjectID == baselineObj.ObjectID) { isFound = true; break; }
                
                if(!isFound)
                {
                    ObjectDelta deltaObj;
                    deltaObj.ObjectID = baselineObj.ObjectID;
                    deltaObj.isNew = false;
                    deltaObj.isDeleted = true;
                    deltaObj.dataLength = 0;
                    outPacket.deltas.push_back(deltaObj);
                }
            }
        }

        NE_LOG("Delta created tick %u -> %u: %zu changes", lastClientAckdTick, currentTick, outPacket.deltas.size());
        return NetResult::Ok;
    }

    //* Client - Apply Deltas
    NetResult SnapshotManager::ApplyDelta(const DeltaPacket& packet)
    {
        for(const auto& delta : packet.deltas)
        {
            auto i = t_objects.find(delta.ObjectID);
            if(i == t_objects.end()) continue;

            if(delta.isDeleted) { i -> second.isPendingDelete = true; continue; }

            Serializer::BitReader reader(delta.data, delta.dataLength);
            i -> second.obj -> Deserialize(reader);

            if(reader.HasFailed())
            {
                NE_LOG("Deserializer failed for object %u", delta.ObjectID);
                return NetResult::SerializeError;
            }
        }
        return NetResult::Ok;
    }

    uint32_t SnapshotManager::GetLatestTick() const { return t_latestTick; }

    //* Internal Helpers
    const Snapshooter* SnapshotManager::FindSnapshot(uint32_t tick) const
    {
        //never match uninitialized slot
        //Slots only valud when CaptureSnapshot() writes them, tricked by historyCount
        // w/o: caller passing tick == 0 would hit a zero-init slot and rcv a ptr to trash data
        if(t_historyCount == 0) return nullptr;

        for(uint32_t i = 0; i < SNAPSHOT_HIST; ++i)
            if(t_history[i].tick == tick) return &t_history[i];
        
        return nullptr;
    }
} //namespace Replication
} //namespace Aegis