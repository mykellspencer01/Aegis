//Abstract interface every genre-specific network module must implement
/*
    * Architecture
        - Engine core handles:
            + transport
            + serialization
            + replication
            + crypto
        - Genre-specific concerns live in swappable modules
            + FPS lag compensation
            + RTS lockstep CRC
            + MMO AOI grids
        - Module is registered once at startup
            - engine calls its OnTick() every ntwrk frame
            - dispatches relevant incoming pkts to its hook

    * Adding a new genre (Future updates)
        - Create a class that derives from GenreModule
        - Implement all pure-virtual methods
        - Register an instance with a genre registry
        - Add a genre ID entry in EngineTypes.h
    C# matchmaking API sets active genre at session start
*/

#pragma once

#include "EngineTypes.h"
#include "../Serializer/BitWrite.h"
#include "../Serializer/BitRead.h"
#include <memory>
#include <unordered_map>

namespace NetEngine {
namespace Genres {

    //all methods are called from the engine's ntwrk thread
    class GenreModule {
        
        public:
            virtual ~GenreModule() = default;
            virtual GenreID GetGenreID() const = 0;
            virtual const char* GetName() const = 0;
            virtual NetResult OnInit() = 0;
            virtual void OnTick(double deltaSec) = 0;
            virtual void OnPacketReceived(PeerID sender, uint8_t channelID, 
                Serializer::BitReader& reader) = 0;
            virtual void OnPeerConnected(PeerID peer) = 0;
            virtual void OnPeerDisconnected(PeerID peer) = 0;
            virtual void OnShutdown() = 0;
    };

    //* Registering Genre Modules
    class RegistryModule {

        public:
            static RegistryModule& Instance(); //return process-lifetime singleton
            NetResult Register(std::unique_ptr<GenreModule> module);
            GenreModule* Get(GenreID id) const;
            NetResult InitAll();
            void TickAll(double deltaSec);
            void ShutdownAll();
            void NotifyPeerConnected(PeerID peer);
            void NotifyPeerDisconnected(PeerID peer);
        
        private:
            RegistryModule() = default;
            std::unordered_map<uint8_t, std::unique_ptr<GenreModule>> t_modules;
    };

    
} //namespace Genres
} //namespace NetEngine