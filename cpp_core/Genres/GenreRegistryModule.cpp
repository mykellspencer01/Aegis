#include "GenreModule.h"
#include <cstdio>

#ifdef NETENGINE_LOGGING
    #define NE_LOG_GMR(fmt, ...) std::printf("[GenreModuleRegistry] " fmt "\n", ##__VA_ARGS__)
#else
    #define NE_LOG_GMR(fmt, ...) ((void)0)
#endif

namespace Aegis {
namespace Genres {

    RegistryModule& RegistryModule::Instance()
    {
        static RegistryModule rm_instance;
        return rm_instance;
    }

    NetResult RegistryModule::Register(std::unique_ptr<GenreModule> module)
    {
        if(!module) return NetResult::InvalidArg;

        uint8_t key = static_cast<uint8_t>(module -> GetGenreID());
        if(t_modules.count(key)) return NetResult::AlreadyInitialized;

        NE_LOG_GMR("Registered Module: %s", module -> GetName());
        t_modules.emplace(key, std::move(module));

        return NetResult::Ok;
    }

    GenreModule* RegistryModule::Get(GenreID id) const
    {
        auto mods = t_modules.find(static_cast<uint8_t>(id));
        return(mods != t_modules.end()) ? mods -> second.get() : nullptr;
    }

    NetResult RegistryModule::InitAll()
    {
        for(auto& [k, m] : t_modules)
        {
            NetResult result = m -> OnInit();
            if(result != NetResult::Ok)
            {
                NE_LOG_GMR("Module %s OnInit() failed: %d", m -> GetName(), static_cast<int>(result));
                return result;
            }
        }
        return NetResult::Ok;
    }

    void RegistryModule::TickAll(double deltaSec)
    {
        for(auto& [k, m] : t_modules)
            m -> OnTick(deltaSec);
    }

    void RegistryModule::ShutdownAll()
    {
        for(auto& [k, m] : t_modules)
            m -> OnShutdown();
        t_modules.clear();
    }

    void RegistryModule::NotifyPeerConnected(PeerID peer)
    {
        for(auto& [k, m] : t_modules) m -> OnPeerConnected(peer);
    }

    void RegistryModule::NotifyPeerDisconnected(PeerID peer)
    {
        for(auto& [k, m] : t_modules) m -> OnPeerDisconnected(peer);
    }
} //namespace Genres
} //namespace Aegis