/*
    * Manual integration test - Host side
    1. Host() on a fixed port (7777) so the client knows where to connect
    2. AddPeer() for the client 
        > Mainly to satisfy SendRaw()'s peer-lookup requirement if host needs to reply
    3. Loop calling Tick() approx. every 16ms (60 hz game loop)
        > prints what callback receives
*/

using Aegis.Managed.Session;
using Aegis.Managed.Interop;
using Aegis.Managed.EngineAdapters;
using System;

const ushort hostPort = 7777;
const ushort clientPort = 7778;
const ulong clientPeerID= 2;

var session = new SessionManager();
var adapter = new CustomAdapter();

// Clean native shutdown
Console.CancelKeyPress += (_, e) =>
{
    Console.WriteLine("\n[Host] Shutting down...");
    session.Shutdown(); 
};

NetResult initResult = session.Host(hostPort);
if(initResult != NetResult.Ok)
{
    Console.WriteLine($"[Host] Host() failed: {initResult}");
    return;
}

NetResult addClientResult = session.Add_Peer(clientPeerID, "127.0.0.1", clientPort);
Console.WriteLine($"[Host] Add_Peer(client) result: {addClientResult}");

Console.WriteLine($"[Host] Listening on Port: {session.LocalPort}," +
    $"Session ID: {session.SessionID}");
Console.WriteLine($"[Host] Driven by {adapter.engineName} adapter");
Console.WriteLine("[Host] Waiting for packets from LoopBackClient... (Ctrl+C to stop)");

session.OnDataReceived += (peerID, channelID, data) =>
{
    string hex = BitConverter.ToString(data).Replace("-", " ");
    Console.WriteLine($"[Host] Packet received from Peer {peerID} on channel {channelID}: {hex}");
};

while (true)
{
    session.Tick(0.016);
    System.Threading.Thread.Sleep(16);
}