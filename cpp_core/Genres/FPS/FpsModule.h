/*
    * FPS games require 2 features beyond the core:

    * Lag Compensation
        - when a client takes an shooting action, the action's detection runs on the server
            + the client saw the world at an earlier time (RTT)
            + w/o lag compensation
                > server would check action against the current positions of targets
                > causing all to miss
        - FpsModule maintains a rolling history buffer of past world states
            + when a HIT_SCAN RPC arrives
                > server rewinds to the client's render timestamp
                > server runs hit-detection
                > server then fast-forwards back to the present
    * Hit-scan RPC Protocol
        RPC_FPS_HIT_SCAN (channel 2):
            [float originX, float originY, float originZ]
            [float dirX, float dirY, float dirZ]
            [float clientTimeStamp]
            [uint8 weaponID]
        RPC_FPS_HIT_RESULT (channel 2, server -> client)
            [bool hit]
            [uint32 targetObjID] - if hit
            [uint8 damage] - if hit
*/

#pragma once

#include "GenreModule.h"
#include <functional>
#include <deque>
#include <vector>
#include <unordered_map>
#include <cstring>

namespace Aegis {
namespace Genres {

    //RPC ids reserved for fps module
    constexpr uint16_t RPC_RPS_HIT_SCAN = 0x0201;
    constexpr uint16_t RPC_FPS_HIT_RESULT = 0x0202;

    struct PlayerState
    {
        PeerID peerID; //owner peer
        float posX, posY, posZ; //world position
        float yaw, pitch; //View angle in degrees
        bool isAlive;
    };

    struct HistoryFrame
    {
        double timestamp;
        std::vector<PlayerState> players;
    };

    class FpsModule : public GenreModule
    {
        public:
            FpsModule();

            //Genre module interface
            GenreID GetGenreID() const override { return GenreID::FPS; }
            const char* GetName() const override { return "FPS"; }
            NetResult OnInit() override;
            void OnTick(double dt) override;
            void OnPacketReceived(PeerID sender, uint8_t channel,
                        Serializer::BitReader& reader) override;
            void OnPeerConnected(PeerID peer) override;
            void OnPeerDisconnected(PeerID peer) override;
            void OnShutdown() override;
        
            //FPS-specific API (called by server game code each tick)
            void RecordHistoryFrame(const std::vector<PlayerState>& states, double timestamp);
            using HitDetector = std::function<bool(
                float, float, float, //origin
                float, float, float, //direction
                uint8_t, //weapon id
                const std::vector<PlayerState>&,
                PeerID&, //out victim/target
                uint8_t& //out damage
            )>;
            void SetHitDetectCallback(HitDetector HD);
        
        private:
            static constexpr uint32_t MAX_HISTORY_FRAMES = 60; //covers 1 second lag max hist
            std::deque<HistoryFrame> t_history; //history ring buffer
            HitDetector t_hitDetect; //callback supplied by game code
            double t_serverTime; //accumulated server time for tick tracking
            const HistoryFrame* FindFrame(double timestamp) const; 
            void HandleHitScan(PeerID sender, Serializer::BitReader& reader); //processes incoming hitscan from client
    };
} //namespace Genres
} //namespace Aegis