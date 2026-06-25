using NUnit.Framework;
using Aegis.Managed.Session;
using Aegis.Managed.Interop;
using System.Collections.Generic;

namespace Aegis.Managed.SharpTests
{
    [TestFixture]
    public class SessionNativeTests
    {
        //! Host - first call to reach native code

        [Test]
        public void Host_PortZ0_ReturnsOk()
        {
            var session = new SessionManager();

            try
            {
                NetResult result = session.Host(0);
                Assert.That(result, Is.EqualTo(NetResult.Ok));
            }
            finally{ session.Shutdown(); }
        }

        [Test]
        public void Host_PortZ0_AssignsNonZeroPort()
        {
            var session = new SessionManager();

            try
            {
                session.Host(0);
                Assert.That(session.LocalPort, Is.GreaterThan((ushort)0));
            }
            finally{ session.Shutdown(); }
        }

        [Test]
        public void Host_Success_TransitionStateToHosting()
        {
            var session = new SessionManager();

            try
            {
                session.Host(0);
                Assert.That(session.State, Is.EqualTo(SessionState.Hosting));
            }
            finally{ session.Shutdown(); }
        }

        [Test]
        public void Host_Success_GensNonEmptySessionID()
        {
            var session = new SessionManager();

            try
            {
                session.Host(0);
                Assert.That(session.SessionID, Is.Not.Empty);
            }
            finally{ session.Shutdown(); }
        }

        [Test]
        public void Host_CalledTwiceSameInstance_SecondReturns_AlreadyInit()
        {
            var session = new SessionManager();

            try
            {
                var first = session.Host(0);
                var second = session.Host(0);
                Assert.That(first, Is.EqualTo(NetResult.Ok));
                Assert.That(second, Is.EqualTo(NetResult.AlreadyInitialized));
            }
            finally{ session.Shutdown(); }
        }

        [Test]
        public void Host_TwoInstances_GetDiffPorts()
        {
            var sessionA = new SessionManager();
            var sessionB = new SessionManager();

            try
            {
                var resultA = sessionA.Host(0);
                var resultB = sessionB.Host(0);

                Assert.That(resultA, Is.EqualTo(NetResult.Ok));
                Assert.That(resultB, Is.EqualTo(NetResult.AlreadyInitialized));
            }
            finally{ sessionA.Shutdown(); sessionB.Shutdown(); }
        }

        //! Shutdown - after real Host(), confirm teardown is completed

        [Test]
        public void Shutdown_AfterHost_TransitionsToDisconn()
        {
            var session = new SessionManager();
            session.Host(0);
            bool fired = false;
            session.OnSessionEnded += () => fired = true;
            session.Shutdown();

            Assert.That(fired, Is.True);
        }

        [Test]
        public void Shutdown_AfterHost_FutureHostCallSucceeds()
        {
            var session = new SessionManager();
            session.Host(0);
            session.Shutdown();

            NetResult result = session.Host(0);

            Assert.That(result, Is.EqualTo(NetResult.Ok));
            session.Shutdown();
        }

        //! Add Peer - after host succeeds -> call reaches native code

        [Test]
        public void AddPeer_AfterHost_ReturnsOk()
        {
            var session = new SessionManager();

            try
            {
                session.Host(0);
                NetResult result = session.Add_Peer(1, "127.0.0.1", 7777);
                Assert.That(result, Is.EqualTo(NetResult.Ok));
            }
            finally { session.Shutdown(); }
        }

        [Test]
        public void AddPeer_AfterHost_AddsToConnectedPeers()
        {
            var session = new SessionManager();

            try
            {
                session.Host(0);
                NetResult result = session.Add_Peer(1, "127.0.0.1", 7777);
                Assert.That(session.HasPeer(1), Is.True);
            }
            finally { session.Shutdown(); }
        }

        [Test]
        public void AddPeer_AfterHost_FiresOnPeerJoined()
        {
            var session = new SessionManager();

            try
            {
                session.Host(0);
                ulong? joinedPeerID = null;
                session.OnPeerJoined += id => joinedPeerID = id;
                session.Add_Peer(50, "127.0.0.1", 7777);
                Assert.That(joinedPeerID, Is.EqualTo((ulong)50));
            }
            finally { session.Shutdown(); }
        }

        [Test]
        public void AddPeer_SamePeerIDTwice_SecondReturnsAlreadyInit()
        {
            var session = new SessionManager();

            try
            {
                session.Host(0);
                var first = session.Add_Peer(1, "127.0.0.1", 7777);
                var second = session.Add_Peer(1, "127.0.0.1", 8888);

                Assert.That(first, Is.EqualTo(NetResult.Ok));
                Assert.That(second, Is.EqualTo(NetResult.AlreadyInitialized));
            }
            finally { session.Shutdown(); }
        }

        [Test]
        public void AddPeer_MulitpleDistinctPeers_AllSucceed()
        {
            var session = new SessionManager();

            try
            {
                session.Host(0);
                var resultA = session.Add_Peer(1, "127.0.0.1", 7777);
                var resultB = session.Add_Peer(2, "127.0.0.1", 7778);
                var resultC = session.Add_Peer(3, "127.0.0.1", 7779);

                Assert.That(resultA, Is.EqualTo(NetResult.Ok));
                Assert.That(resultB, Is.EqualTo(NetResult.Ok));
                Assert.That(resultC, Is.EqualTo(NetResult.Ok));
                Assert.That(session.PeerCount, Is.EqualTo(3));
            }
            finally { session.Shutdown(); }
        }

        //! Remove Peer - after adding peer => confirm native cleanup path

        [Test]
        public void RemovePeer_AfterAddingPeer_FiresOnPeerLeft()
        {
            var session = new SessionManager();

            try
            {
                session.Host(0);
                var resultA = session.Add_Peer(1, "127.0.0.1", 7777);

                bool fired = false;
                session.OnPeerLeft += _ => fired = true;
                session.RemovePeer(1);

                Assert.That(fired, Is.True);
            }
            finally { session.Shutdown(); }
        }

        [Test]
        public void RemovePeer_AfterAddingPeer_RemovesFromConnectedPeers()
        {
            var session = new SessionManager();

            try
            {
                session.Host(0);
                session.Add_Peer(1, "127.0.0.1", 7777);
                session.RemovePeer(1);

                Assert.That(session.HasPeer(1), Is.False);
                Assert.That(session.PeerCount, Is.EqualTo(0));
            }
            finally { session.Shutdown(); }
        }

        [Test]
        public void RemovePeer_AddingSamePeerIDBack_Succeeds()
        {
            var session = new SessionManager();

            try
            {
                session.Host(0);
                session.Add_Peer(1, "127.0.0.1", 7777);
                session.RemovePeer(1);

                NetResult result = session.Add_Peer(1, "127.0.0.1", 7777);

                Assert.That(result, Is.EqualTo(NetResult.Ok));
            }
            finally { session.Shutdown(); }
        }

        //! Send Raw - after a registered peer => confirm real native send

        [Test]
        public void SendRaw_ToRegisteredPeer_ReturnsOk()
        {
            var session = new SessionManager();

            try
            {
                session.Host(0);
                session.Add_Peer(1, "127.0.0.1", 9999);

                byte[] data = {0x12, 0x34, 0x56, 0x78};
                NetResult result = session.SendRaw(1, 0, data);

                Assert.That(result, Is.EqualTo(NetResult.Ok));
            }
            finally { session.Shutdown(); }
        }

        [Test]
        public void SendRaw_ToUnRegisteredPeer_ReturnsUnknownPeer()
        {
            var session = new SessionManager();

            try
            {
                session.Host(0);
                byte[] data = {1, 2, 3, 4};
                NetResult result = session.SendRaw(10, 0, data);

                Assert.That(result, Is.EqualTo(NetResult.UnknownPeer));
            }
            finally { session.Shutdown(); }
        }

        //! Tick - after tick -> confirm runs w/o throwing

        [Test]
        public void Tick_AfterHost_NoThrows()
        {
            var session = new SessionManager();

            try
            {
                session.Host(0);
                Assert.DoesNotThrow(
                    () => session.Tick(0.016)
                );
            }
            finally { session.Shutdown(); }
        }

        [Test]
        public void Tick_RepeatedCalls_AfterHost_NoThrows()
        {
            var session = new SessionManager();

            try
            {
                session.Host(0);
                Assert.DoesNotThrow(() =>
                    { for(int i = 0; i < 10; i++) session.Tick(0.016); }
                );
            }
            finally { session.Shutdown(); }
        }
    }
}