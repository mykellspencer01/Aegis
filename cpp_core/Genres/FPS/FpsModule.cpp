#include "FpsModule.h"
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstring>

#ifdef NETENGINE_LOGGING
    #define NE_LOG(fmt, ...) std::printf("[FPS] " fmt "\n", ##__VA_ARGS__)
#else
    #define NE_LOG(fmt, ...) ((void)0)
#endif

namespace NetEngine {
namespace Genres {

    FpsModule::FpsModule() : t_serverTime(0.0) {}

    NetResult FpsModule::OnInit()
    {
        t_history.clear();
        t_serverTime = 0.0;
        NE_LOG("FPS module initialized.");
        return NetResult::Ok;
    }

    void FpsModule::OnPeerConnected(PeerID peer)
    {
        NE_LOG("Peer %llu connected to FPS session", (unsigned long long)peer);
    }

    void FpsModule::OnPeerDisconnected(PeerID peer)
    {
        NE_LOG("Peer %llu disconnected from FPS session", (unsigned long long)peer);
    }

    void FpsModule::RecordHistoryFrame(const std::vector<PlayerState>& states, double timestamp)
    {
        HistoryFrame frame;
        frame.timestamp = timestamp;
        frame.players = states;
        t_history.push_back(std::move(frame));

        while(t_history.size() > MAX_HISTORY_FRAMES) t_history.pop_front();
    }

    void FpsModule::SetHitDetectCallback(HitDetector HD)
    {
        t_hitDetect = std::move(HD);
    }

    const HistoryFrame* FpsModule::FindFrame(double timestamp) const
    {
        if(t_history.empty()) return nullptr;

        const HistoryFrame* best = &t_history.front();
        double bestDelta = std::abs((best -> timestamp) - timestamp);

        for(const auto& hist : t_history)
        {
            double t = std::abs(hist.timestamp - timestamp);
            if(t < bestDelta)
            {
                bestDelta = t;
                best = &hist;
            }
        }
        return best;
    }

    void FpsModule::HandleHitScan(PeerID sender, Serializer::BitReader& reader)
    {
        float origX = reader.ReadFloat(), origY = reader.ReadFloat(), origZ = reader.ReadFloat();
        float dirX = reader.ReadFloat(), dirY = reader.ReadFloat(), dirZ = reader.ReadFloat();
        float cliientTime = reader.ReadFloat();
        uint8_t weaponID = reader.ReadFloat();

        if(reader.HasFailed()) return;

        NE_LOG("HitScan from peer %llu at time %.3f", (unsigned long long)sender, cliientTime);

        //Rewind world state to the client's rendered timestamp
        const HistoryFrame* frame = FindFrame(static_cast<double>(cliientTime));
        if(!frame || !t_hitDetect) return;

        PeerID victim = PEER_NOUGHT;
        uint8_t damage = 0;
        bool hit = t_hitDetect(origX, origY, origZ, dirX, dirY, dirZ, 
            weaponID, frame -> players, victim, damage);
        
        NE_LOG("Hitscan result: hit = %d. Victim is %llu. Damaged for %u.",
            hit, (unsigned long long)victim, damage);
    }

    //TODO: Send RPC_FPS_HIT_RESULT back to sender via ReliableChannel
} //namespace Genres
} //namespace NetEngine