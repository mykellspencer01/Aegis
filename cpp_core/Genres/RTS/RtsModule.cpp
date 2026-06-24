#include "RtsModule.h"
#include <cstdio>
#include <numeric>

#ifdef NETENGINE_LOGGING
    #define NE_LOG(fmt, ...) std::printf("[RTS] " fmt "\n", ##__VA_ARGS__)
#else
    #define NE_LOG(fmt, ...) ((void)0)
#endif

namespace Aegis {
namespace Genres {

    RtsModule::RtsModule() : t_currentTurn(0), t_expectedPeerCount(0) {}

    NetResult RtsModule::OnInit()
    {
        t_currentTurn = 0;
        t_expectedPeerCount = 0;
        t_pendingTurn.turn = 1;
        t_pendingTurn.commandsByPeer.clear();
        NE_LOG("RTS module initialized.");

        return NetResult::Ok;
    };

    void RtsModule::OnTick(double dt) {}

    void RtsModule::OnPacketReceived(PeerID sender, uint8_t channel, Serializer::BitReader& reader)
    {
        uint16_t rpcID = static_cast<uint16_t>(reader.ReadBits(16));
        if(rpcID == RPC_RTS_COMMANDS) HandleCommands(sender, reader);
        else if(rpcID == RPC_RTS_CRC_REPORT) HandleCrcReport(sender, reader);
    }

    void RtsModule::OnPeerConnected(PeerID peer)
    {
        ++t_expectedPeerCount;
        NE_LOG("Peer %llu joined RTS session (total %u)", (unsigned long long)peer, t_expectedPeerCount);
    }

    void RtsModule::OnPeerDisconnected(PeerID peer)
    {
        if(t_expectedPeerCount > 0) --t_expectedPeerCount;
        t_pendingTurn.commandsByPeer.erase(peer);
        NE_LOG("Peer %llu left RTS session", (unsigned long long)peer);
    }

    void RtsModule::OnShutdown() { NE_LOG("RTS module shutdown."); }

    void RtsModule::SetTurnReadyCallback(std::function<void(const TurnData&)> tr)
    {
        t_turnReady = std::move(tr);
    }

    void RtsModule::SetDesyncCallback(std::function<void(uint32_t, PeerID, uint32_t, uint32_t)> ds)
    {
        t_desyncCall = std::move(ds);
    }

    void RtsModule::ReportCRC(uint32_t turn, uint32_t crc)
    {
        //TODO: in full development, sends to server
        NE_LOG("Local CRC report turn = %u, CRC = 0x%08X", turn, crc);
    }

    void RtsModule::HandleCommands(PeerID sender, Serializer::BitReader& reader)
    {
        uint32_t turn = reader.ReadUInt32();
        uint8_t count = reader.ReadUInt8();
        if(reader.HasFailed()) return;

        std::vector<RtsCommand> cmds;
        cmds.reserve(count);

        for(uint8_t i = 0; i < count; ++i)
        {
            RtsCommand cmd;
            cmd.type = reader.ReadUInt8();
            cmd.targetID = reader.ReadUInt32();
            cmd.worldX = reader.ReadFloat();
            cmd.worldZ = reader.ReadFloat();

            cmds.push_back(cmd);
        }
        if(reader.HasFailed()) return;

        t_pendingTurn.commandsByPeer[sender] = std::move(cmds);
        NE_LOG("Rcvd cmds for turn %u from peer %llu (%u/%u ready)", turn,
            (unsigned long long)sender, (uint32_t) t_pendingTurn.commandsByPeer.size(), t_expectedPeerCount);
        
        //once all peers have submitted cmds, go to next turn
        if(t_pendingTurn.commandsByPeer.size() >= t_expectedPeerCount)
        {
            t_pendingTurn.turn = turn;
            if(t_turnReady) t_turnReady(t_pendingTurn);
            ++t_currentTurn;
            t_pendingTurn.commandsByPeer.clear();
            t_pendingTurn.turn = t_currentTurn + 1;
        }
    }

    void RtsModule::HandleCrcReport(PeerID sender, Serializer::BitReader& reader)
    {
        uint32_t turn = reader.ReadUInt32();
        uint32_t crc = reader.ReadUInt32();
        if(reader.HasFailed()) return;

        t_crcReports[turn][sender] = crc;
        CheckCrcs(turn);
    }

    void RtsModule::CheckCrcs(uint32_t turn)
    {
        auto& crcs = t_crcReports[turn];
        if(crcs.size() < t_expectedPeerCount) return;

        uint32_t expected = crcs.begin() -> second;
        for(auto& [peer, crc] : crcs)
        {
            if(crc != expected)
            {
                NE_LOG("Desync at turn %u! Peer %llu: 0x%08X vs 0x%08X",
                    turn, (unsigned long long) peer, expected, crc);
                if(t_desyncCall) t_desyncCall(turn, peer, expected, crc);
            }
        }
        t_crcReports.erase(turn);
    }
} //namespace Genres
} //namespace Aegis