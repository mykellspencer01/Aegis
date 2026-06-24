#include "MmoModule.h"
#include <cstdio>
#include <algorithm>

#ifdef NETENGINE_LOGGING
    #define NE_LOG(fmt, ...) std::printf("[MMO] " fmt "\n", ##__VA_ARGS__)
#else
    #define NE_LOG(fmt, ...) ((void)0)
#endif

namespace Aegis {
namespace Genres {

    MmoModule::MmoModule() {}

    NetResult MmoModule::OnInit()
    {
        t_players.clear();
        t_grid.clear();
        NE_LOG("MMO module initialized (Cell = %.0f Grid = %ux%u)", MMO_CELL_SIZE, MMO_GRID_H, MMO_GRID_W);
        return NetResult::Ok;
    }

    void MmoModule::OnTick(double dt) {}

    void MmoModule::OnPacketReceived(PeerID sender, uint8_t channel, Serializer::BitReader& reader)
    {
        uint16_t rpcID = static_cast<uint16_t>(reader.ReadBits(16));
        if(rpcID == RPC_MMO_MOVE) HandleMove(sender, reader);
        else if(rpcID == RPC_MMO_ZONE_REQ) HandleZoneRequest(sender, reader);
    }

    void MmoModule::OnPeerConnected(PeerID peer)
    {
        MmoPlayerRecord record{};
        record.peer = peer;
        record.worldX = 0.0f; record.worldZ = 0.0f;
        record.zoneID = 0;
        record.cell = WorldToCell(0.0f, 0.0f);
        t_players[peer] = record;
        t_grid[record.cell].insert(peer);
        NE_LOG("Peer %llu connected to MMO", (unsigned long long) peer);
    }

    void MmoModule::OnPeerDisconnected(PeerID peer)
    {
        auto disconPeer = t_players.find(peer);
        if(disconPeer != t_players.end())
        {
            t_grid[disconPeer -> second.cell].erase(peer);
            t_players.erase(disconPeer);
        }
        NE_LOG("Peer %llu disconnected from MMO", (unsigned long long) peer);
    }

    void MmoModule::OnShutdown() { t_players.clear(); t_grid.clear(); }

    void MmoModule::UpdatePlayerPos(PeerID peer, float x, float z, uint32_t zoneID)
    {
        auto targetPeer = t_players.find(peer);
        if(targetPeer == t_players.end()) return;

        MmoPlayerRecord& record = targetPeer -> second;
        GridCell oldCell = record.cell;
        GridCell newCell = WorldToCell(x, z);

        //update grid members if player crossed cell bounds
        if(!(oldCell == newCell))
        {
            t_grid[oldCell].erase(peer);
            t_grid[newCell].insert(peer);
            record.cell = newCell;
            NE_LOG("Peer %llu moved to cell (%d, %d)", (unsigned long long) peer, newCell.colIdx, newCell.rowIdx);
        }

        record.worldX = x;
        record.worldZ = z;
        record.zoneID = zoneID;
    }

    std::vector<PeerID> MmoModule::GetAoiPeers(PeerID peer) const
    {
        auto target = t_players.find(peer);
        if(target == t_players.end()) return {};

        std::vector<PeerID> result;
        for(const GridCell& nc : GetNeighborCells(target -> second.cell))
        {
            auto targetCell = t_grid.find(nc);
            if(targetCell == t_grid.end()) continue;

            for(PeerID pid : targetCell -> second)
                if(pid != peer) result.push_back(pid);
        }
        return result;
    }

    std::vector<PeerID> MmoModule::GetZonePeers(uint32_t zoneID) const
    {
        std::vector<PeerID> result;
        for(auto& [id, record] : t_players)
            if(record.zoneID == zoneID) result.push_back(id);
        return result;
    }

    void MmoModule::SetZoneTransferCallback(std::function<void(PeerID, uint32_t)> zt)
    {
        t_zoneCB = std::move(zt);
    }

    GridCell MmoModule::WorldToCell(float x, float z)
    {
        return {
            static_cast<int32_t>(std::floor(x / MMO_CELL_SIZE)),
            static_cast<int32_t>(std::floor(z / MMO_CELL_SIZE))
        };
    }

    std::vector<GridCell> MmoModule::GetNeighborCells(const GridCell& cell)
    {
        std::vector<GridCell> cells;
        cells.reserve(9);

        for(int az = -1; az <= 1; ++az)
            for(int ax = -1; ax <= 1; ++ax)
                cells.push_back({ cell.colIdx + ax, cell.rowIdx + az});
        
        return cells;
    }

    void MmoModule::HandleMove(PeerID sender, Serializer::BitReader& reader)
    {
        float x = reader.ReadFloat(), z = reader.ReadFloat();
        if(!reader.HasFailed()) UpdatePlayerPos(sender, x, z, t_players[sender].zoneID);
    }

    void MmoModule::HandleZoneRequest(PeerID sender, Serializer::BitReader& reader)
    {
        uint32_t zoneID = reader.ReadUInt32();
        if(!reader.HasFailed() && t_zoneCB) t_zoneCB(sender, zoneID); 
    }

} //namespace Genres
} //namespace Aegis