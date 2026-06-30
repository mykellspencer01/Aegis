/*
    * Unity-specific monobehaviour that bridges Unity's Update() loop
    * into the Aegis SessionManager via the UnityAdapter

    * Editor setup
        1. Create empty game object in the scene
        2. Attach this script to empty object
        3. Set isHost = true in the inspector for the acting host machine
            > false for the one acting as client
        4. Play the scene
            > Watch console logs, confirm they match those from the examples
*/

using UnityEngine;
using Aegis.Managed.Session;
using Aegis.Managed.Interop;
using Aegis.Managed.EngineAdapters;

public class AegisNetworkBridge : MonoBehaviour
{
    [Header("Role")]
    [Tooltip("True = host. False = client.")]

    [SerializeField] private bool isHost = true;
    [SerializeField] private string hostIP = "127.0.0.1";
    [SerializeField] private ushort hostPort = 7777;
    [SerializeField] private ushort clientPort = 7778;
    [SerializeField] private ulong remotePeerID = 1;

    private SessionManager session = null;
    private UnityAdapter adapter = null;

    //! Unity lifecycle
    private void Start()
    {
        session = new SessionManager();
        adapter = new UnityAdapter();

        session.OnDataReceived += OnDataReceived;
        session.OnPeerJoined += peerID => Debug.Log($"[Aegis] Peer {peerID} joined.");
        session.OnPeerLeft += peerID => Debug.Log($"[Aegis] Peer {peerID} left.");

        if(isHost) StartAsHost();
        else StartAsClient();

        adapter.RegisterTickCallback(session.Tick);
    }

    private void Update() { adapter.Tick(Time.deltaTime); }

    private void OnAppQuit() { Shutdown(); }

    private void OnDestroy() { Shutdown(); }

    //! Role-specific startup
    private void StartAsHost()
    {
        NetResult initResult = session.Host(hostPort, adapter.engineTypeByte);
        if(initResult != NetResult.Ok)
        {
            Debug.LogError($"[Aegis] Host() failed: {initResult}");
            return;
        }
        Debug.Log($"[Aegis] Hosting on port {session.LocalPort}, Session ID: {session.SessionID}");
        Debug.Log($"[Aegis] Driven by: {adapter.engineName} adapter");

        NetResult addPeerResult = session.Add_Peer(remotePeerID, hostIP, clientPort);
        Debug.Log($"[Aegis] Add_Peer (client) result:  {addPeerResult}");
    }

    private void StartAsClient()
    {
        NetResult initResult = session.Host(hostPort, adapter.engineTypeByte);
        if(initResult != NetResult.Ok)
        {
            Debug.LogError($"[Aegis] Host() failed: {initResult}");
            return;
        }
        Debug.Log($"[Aegis] Bound to local port {session.LocalPort}");
        Debug.Log($"[Aegis] Driven by: {adapter.engineName} adapter");

        NetResult addPeerResult = session.Add_Peer(remotePeerID, hostIP, clientPort);
        Debug.Log($"[Aegis] Add_Peer (host) result:  {addPeerResult}");

        byte[] payload = { 0x12, 0x34, 0x56, 0x78 };
        NetResult sendResult = session.SendRaw(remotePeerID, channelID: 0, payload);
        Debug.Log($"[Aegis] SendRaw() result: {sendResult}");
    }

    //! Receive Handler
    private void OnDataReceived(ulong peerID, byte channelID, byte[] data)
    {
        string hex = System.BitConverter.ToString(data).Replace("-", " ");
        Debug.Log($"[Aegis] Packet received from peer {peerID} on channel {channelID}: {hex}");
    }

    //! Shutdown
    private void Shutdown()
    {
        adapter?.OnEngineShutdown();
        session?.Shutdown();
    }

}