/*
    * Primary game-facing object for managing an Aegis network session
*/

using System;
using System.Collections.Generic;
using Aegis.Managed.Interop;

namespace Aegis.Managed.Session
{
    public enum SessionState
    {
        Idle,
        Hosting,
        Connected,
        Disconnected
    }

    public class SessionManager
    {
        public SessionState State { get; private set; } = SessionState.Idle;
        public ushort LocalPort { get; private set; }
        public string SessionID {get; private set; } = string.Empty;

        // * Events
        public event Action<ulong>? OnPeerJoined;
        public event Action<ulong>? OnPeerLeft;
        public event Action? OnSessionEnded;

        // * Private State
        private readonly Dictionary<ulong, string> _peers = new();

        // * Lifecycle
        public NetResult Host(ushort port = 0, byte engineType = 0)
        {
            //Guard to prevent double init
            if(State != SessionState.Idle&& State != SessionState.Disconnected)
                return NetResult.AlreadyInitialized;
            
            NetResult result = (NetResult)NativeBindings.Aegis_Init(port, engineType);
            if(result == NetResult.Ok)
            {
                LocalPort = NativeBindings.Aegis_GetLocalPort();
                SessionID = Guid.NewGuid().ToString("N")[..8].ToUpperInvariant();
                State = SessionState.Hosting;
            }

            return result;
        }

        public NetResult Add_Peer(ulong peerID, string ip, ushort port)
        {
            if(State == SessionState.Idle) return NetResult.NotInitialized;
            if(string.IsNullOrEmpty(ip)) return NetResult.InvalidArg;
            if(_peers.ContainsKey(peerID)) return NetResult.AlreadyInitialized;

            NetResult result = (NetResult)NativeBindings.Aegis_AddPeer(peerID, ip, port);

            if(result == NetResult.Ok)
            {
                _peers[peerID] = $"{ip}:{port}";
                OnPeerJoined?.Invoke(peerID);
            }

            return result;
        }

        public void RemovePeer(ulong peerID)
        {
            if(State == SessionState.Idle) return;
            NativeBindings.Aegis_RemovePeer(peerID);
            if(_peers.Remove(peerID)) OnPeerLeft?.Invoke(peerID);
        }

        public unsafe NetResult SendRaw(ulong peerID, byte channelID, byte[] data)
        {
            if(State == SessionState.Idle) return NetResult.NotInitialized;
            if(data == null || data.Length == 0) return NetResult.InvalidArg;

            fixed (byte* p = data)
            {
                return (NetResult)NativeBindings.Aegis_Send(peerID, channelID, p, (uint)data.Length);
            }
        }

        public void Tick(double deltaSeconds)
        {
            if(State == SessionState.Idle || State == SessionState.Disconnected) return;
            NativeBindings.Aegis_Tick(deltaSeconds); 
        }

        public void Shutdown()
        {
            if(State == SessionState.Idle || State == SessionState.Disconnected) return;

            NativeBindings.Aegis_Shutdown();
            _peers.Clear();
            LocalPort = 0;
            SessionID = string.Empty;
            State = SessionState.Disconnected;
            OnSessionEnded?.Invoke();
        }

        // * Accessors
        public IReadOnlyDictionary<ulong, string> ConnectedPeers => _peers;
        public int PeerCount => _peers.Count;
        public bool HasPeer(ulong peerID) => _peers.ContainsKey(peerID);
    }
}