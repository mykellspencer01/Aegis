using System;
namespace Aegis.Managed.EngineAdapters
{
    public class UnityAdapter : AegisAdapter
    {
        //human-readable name
        public string engineName => "Unity";
        public byte engineTypeByte => 1;

        //Internal state
        private Action<double>? _onTick;


        //register tick callback
        public void RegisterTickCallback(Action<double> onTick)
        {
            _onTick = onTick;
        }

        //shutdown
        public void OnEngineShutdown()
        {
            _onTick = null;
        }

        //Unity-specific API
        public void Tick(double deltaTime)
        {
            _onTick?.Invoke(deltaTime);
        }
    }
}