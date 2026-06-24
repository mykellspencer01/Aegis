/*
    RTS games use lockstep simulation: 
        - instead of replicating the world state
            + every client runs on identical deteerministic sim
        - ntwrk layer only transmits player commands, not positions
    
    * How lockstep works here
        - Commands (unit orders) are collected per-frame on each client
        - Each client sends its commands to server before the next sim step begins
        - Once commands from ALL clients are rcvd for a given turn
            + server broadcasts the complete command to everyone
        - Every client advances the sim by one turn simultaneously

    * Verifying Determinism CRC
        - After each sim turn
            + every client computes a CRC-32 of the entire sim state
            + sends to server
        - If any CRC mismatches:
            + server detects a desync
                > can trigger a full-state resync or
                > disconnect the offending client
*/

#pragma once

#include "GenreModule.h"
#include <unordered_map>
#include <vector>
#include <functional>

namespace Aegis {
namespace Genres {

    constexpr uint16_t RPC_RTS_COMMANDS = 0x0301;
    constexpr uint16_t RPC_RTS_TURN_SYNC = 0x0302;
    constexpr uint16_t RPC_RTS_CRC_REPORT = 0x0303;

    struct RtsCommand {
        uint8_t type; //command type (move, attack, jump, etc.)
        uint32_t targetID; 
        float worldX; //world space X coord
        float worldZ; //world space Z coord
    };

    struct TurnData {
        uint32_t turn;
        std::unordered_map<PeerID, std::vector<RtsCommand>> commandsByPeer;
    };

    class RtsModule : public GenreModule {

        public:
            RtsModule();
            GenreID GetGenreID() const override { return GenreID::RTS; }
            const char* GetName() const override { return "RTS"; }
            NetResult OnInit() override;
            void OnTick(double dt) override;
            void OnPacketReceived(PeerID sender, uint8_t channel, Serializer::BitReader& reader) override;
            void OnPeerConnected(PeerID peer) override;
            void OnPeerDisconnected(PeerID peer) override;
            void OnShutdown() override;

            //RTS-specific API
            void SetTurnReadyCallback(std::function<void(const TurnData&)> tr);
            void SetDesyncCallback(std::function<void(uint32_t, PeerID, uint32_t, uint32_t)> ds);
            void ReportCRC(uint32_t turn, uint32_t crc);
            uint32_t GetCurrentTurn() const { return t_currentTurn; }
        
        private:
        //accumulated commands waiting for this turn
            TurnData t_pendingTurn;
            uint32_t t_currentTurn;
            uint32_t t_expectedPeerCount;

        //CRC reports rcvd per turn: turn -> (peer -> crc)
        std::unordered_map<uint32_t , std::unordered_map<PeerID, uint32_t>> t_crcReports;
        std::function<void(const TurnData&)> t_turnReady;
        std::function<void(uint32_t, PeerID, uint32_t, uint32_t)> t_desyncCall;

        void HandleCommands(PeerID sender, Serializer::BitReader& reader);
        void HandleCrcReport(PeerID sender, Serializer::BitReader& reader);
        void BroadcastTurn(const TurnData& turn);
        void CheckCrcs(uint32_t turn);
    };
} //namespace Genres
} //namespace Aegis