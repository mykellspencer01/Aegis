/*
    MMOs have 2 unique ntwrk challenges:

    * Areo-of-Interest (AOI) grids
        = MMO w. hundreds of simultaneous players:
            + prohibitive to replicate each player to each other player
        - AOI grid divides world into uniform grid of cells
            + each player subs to 3x3 neighborhood of cells
        - Replication updates are only sent to players whose AOI overlaps source obj
    * Zone/Channel Streaming
        = world is split into named zones (ex. dungeon instances, landmarks)
            + players can transition between zones
        - module handles handoff: sends a zone transfer request
        - serve coordinates state transfers between zone servers
*/

#pragma once

#include "GenreModule.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>
#include <cmath>

namespace NetEngine {
namespace Genres {

    constexpr uint16_t RPC_MMO_MOVE = 0x0401;
    constexpr uint16_t RPC_MMO_ZONE_REQ = 0x0402;
    constexpr uint16_t RPC_MMO_ZONE_ACK = 0x0403;
    constexpr uint16_t RPC_MMO_AOI_UPDATE = 0x0404;

    constexpr float MMO_CELL_SIZE = 100.0f;

    constexpr uint32_t MMO_GRID_W = 200;
    constexpr uint32_t MMO_GRID_H = 200;

    struct GridCell {
        int32_t colIdx;
        int32_t rowIdx;
        bool operator == (const GridCell& op) const 
            { return colIdx == op.colIdx && rowIdx == op.rowIdx; }
    };

    struct GridHash {
        std::size_t operator()(const GridCell& cell) const noexcept
        {
            return std::hash<int64_t>()(((int64_t)cell.colIdx << 32) | (uint32_t)cell.rowIdx);
        }
    };

    struct MmoPlayerRecord {
        PeerID peer;
        float worldX, worldZ; //current world pos
        uint32_t zoneID; //current zone
        GridCell cell; //current AOI cell
    };

    class MmoModule : public GenreModule {

        public:
            MmoModule();

            GenreID GetGenreID() const override { return GenreID::MMO; }
            NetResult OnInit() override;
            void OnTick(double dt) override;
            void OnPacketReceived(PeerID sender, uint8_t channel,
                        Serializer::BitReader& reader) override;
            void OnPeerConnected(PeerID peer) override;
            void OnPeerDisconnected(PeerID peer) override;
            void OnShutdown() override;
                
            //MMO-specific API
            std::vector<PeerID> GetAoiPeers(PeerID peer) const;
            std::vector<PeerID> GetZonePeers(uint32_t zoneID) const;
            void UpdatePlayerPos(PeerID peer, float x, float z, uint32_t zoneID);
            void SetZoneTransferCallback(std::function<void(PeerID, uint32_t)> zt);

        private:
            std::unordered_map<PeerID, MmoPlayerRecord> t_players;
            //AOI grid: cell -> set of peers occupying cell
            std::unordered_map<GridCell, std::unordered_set<PeerID>, GridHash> t_grid; 
            std::function<void(PeerID, uint32_t)> t_zoneCB;

            static GridCell WorldToCell(float x, float z); //converts world coords to a grid cell
            static std::vector<GridCell> GetNeighborCells(const GridCell& cell); //returns 9-cell neighborhood
            //TODO: cell neighborhood can be scaled to varying sizes (4x4, 5x5, etc)

            void HandleMove(PeerID sender, Serializer::BitReader& reader);
            void HandleZoneRequest(PeerID sender, Serializer::BitReader& reader);
    };
} //namespace Genres
} //namespace NetEngine