/*
    Concrete AegisAdapter for callers driving the engine manually
        > console apps
        > headless servers
        > custom engines
        > Any context where there is no update loop to hook into
*/

using System;
using System.Diagnostics;

namespace Aegis.Managed.EngineAdapters
{
    public class CustomAdapter : AegisAdapter
    {
        //implement interface
        public string engineName => "Custom";
        public byte engineTypeByte => 3;

        //Internal state
        private Action<double>? _onTick;
        private readonly Stopwatch stopwatch = new();
        private bool hasRunFirstFrame;
        private bool isRunning;

        public void RegisterTickCallback(Action<double> onTick)
        {
            _onTick = onTick;
        }

        public void OnEngineShutdown()
        {
            isRunning = false;
            stopwatch.Stop();
            hasRunFirstFrame = false;
            _onTick = null;
        }

        //CustomAdapter-specific API
        public void Start()
        {
            stopwatch.Restart();
            hasRunFirstFrame = false;
            isRunning = true;
        }

        public void RunFrame()
        {
            if(!isRunning || _onTick == null) return;

            double deltaSeconds;
            if(!hasRunFirstFrame)
            {
                deltaSeconds = 0.0;
                hasRunFirstFrame = true;
            }
            else { deltaSeconds = stopwatch.Elapsed.TotalSeconds; }

            stopwatch.Restart();
            _onTick.Invoke(deltaSeconds);
        }

        public void RunBlockingLoop(int targetHz = 60)
        {
            if(targetHz <= 0)
                throw new ArgumentOutOfRangeException(
                    nameof(targetHz),
                    "Target frequency must be a positive number of frames per second."
                );
            
            int sleepMilliseconds = 1000 / targetHz ;

            Start();
            while(isRunning)
            {
                RunFrame();
                System.Threading.Thread.Sleep(sleepMilliseconds);
            }
        }
    }
}