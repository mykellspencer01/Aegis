/*
    * Client-side Integration Test
    * Flow:
        !1. Host() on port 0
            > client doesn't need a fixed port
            > OS can assign any free one
        !2. AddPeer() for the host
            > needs to match the HostPort constant exactly
                - using Peer ID 1
                - host's known fixed address (127.0.0.1:7777)
        !3. SendRaw() on recognizable byte payload on channel 0
        !4. Tick() a few times to flush the send thru native transport layer
            > then Shutdown() and exit
*/

using Aegis.Managed.Session;
using Aegis.Managed.Interop;
using Aegis.Managed.EngineAdapters;
using System;

const string hostIP = "127.0.0.1";
const ushort hostPort = 7777;
const ulong hostPeerID = 1;
const ushort clientPort = 7778;

var session = new SessionManager();
var adapter = new CustomAdapter();

NetResult initResult = session.Host(clientPort, adapter.engineTypeByte);

if(initResult != NetResult.Ok)
{
    Console.WriteLine($"[Client] Host() failed: {initResult}");
    return;
}

Console.WriteLine($"[Client] Bound to local port {session.LocalPort}");

NetResult addPeerResult =session.Add_Peer(hostPeerID, hostIP, hostPort);
if(addPeerResult != NetResult.Ok)
{
    Console.WriteLine($"[Client] Add_Peer() failed: {addPeerResult}");
    session.Shutdown();
    return;
}

Console.WriteLine($"[Client] Registered host peer at {hostIP}:{hostPort}");
Console.WriteLine($"[Client] Driven by {adapter.engineName} adapter.");

// simple recognizable payload
byte[] payload = {0x12, 0x34, 0x56, 0x78};

NetResult sendResult = session.SendRaw(hostPeerID, channelID: 0, payload);
Console.WriteLine($"[Client] SendRaw() result: {sendResult}");

// Wire the adapter, drive it manually for a few frames to flush out the send
adapter.RegisterTickCallback(session.Tick);
adapter.Start();


// Tick a few times to give native transport a chance to flush 
// datagram before shutdown
for(int i = 0; i < 5; i++)
{
    adapter.RunFrame();
    System.Threading.Thread.Sleep(16);
}

adapter.OnEngineShutdown();
session.Shutdown();
Console.WriteLine("[Client] Done.");