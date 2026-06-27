using System;
using System.Runtime.InteropServices;

namespace Aegis.Managed.Interop
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void NativeReceivedCallback(
        ulong peerID, byte channelID, IntPtr payload, uint payloadLength
    );

    internal static class NativeBindings
    {
        #if UNITY_STANDALONE_WIN || UNITY_EDITOR_WIN
            private const string libName = "Aegis_Core";
        #elif UNITY_STANDALONE_OSX || UNITY_EDITOR_OSX || __MACOS__
            private const string libName = "Aegis_Core";
        #else 
            private const string libName = "Aegis_Core";
        #endif

        // * Core lifecycle

        [DllImport(libName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int Aegis_Init(ushort localPort, byte engineType);

        [DllImport(libName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void Aegis_Shutdown();

        [DllImport(libName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void Aegis_Tick(double deltaSeconds);

        [DllImport(libName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern ushort Aegis_GetLocalPort();

        // * Peer management
        [DllImport(libName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int Aegis_AddPeer(
            ulong peerID, [MarshalAs(UnmanagedType.LPStr)] string ip, ushort port
        );

        [DllImport(libName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void Aegis_RemovePeer(ulong peerID);

        // * Sending
        [DllImport(libName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern unsafe int Aegis_Send(ulong peerID, byte channelID, byte* data, uint length);

        // * Genre Selection
        [DllImport(libName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int Aegis_SetGenre(byte genreID);

        // * Crypto
        [DllImport(libName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern unsafe void Aegis_SetSessionKey(byte* key);

        [DllImport(libName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int Aegis_HasSessionKey();

        // * Snapshot Replication
        [DllImport(libName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void Aegis_CaptureSnapshot(uint tick);

        [DllImport(libName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint Aegis_GetLatestTick();

        [DllImport(libName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void Aegis_SetReceiveCallback(NativeReceivedCallback callback);
    }
}