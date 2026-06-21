/*
    * Abstract interface that each engine-specific adapter must implement

    ! Aegis Adapter only deals with the engine lifecycle bridhe
        * Registering tick callbacks
        * Handling engine shutdown
    ! Does not handle session management, key exchanges, or genre selection
*/

using System;

namespace Aegis.Managed.EngineAdapters
{
    //* Abstraction over host game engine's lifecycle and tick registration
    public interface AegisAdapter
    {
        string engineName { get; }
        byte engineTypeByte { get; }
        void RegisterTickCallback(Action<double> onTick);
        void OnEngineShutdown();
    }
}