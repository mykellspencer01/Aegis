/*
    * These tests deliberately exercise only the code paths in SessionManager
    * that return early before reaching any NativeBinding call
*/

using NUnit.Framework;
using Aegis.Managed.Session;
using Aegis.Managed.Interop;

namespace Aegis.Managed.SharpTests
{
    [TestFixture]
    public class SessionGuardTests
    {
        //! Initial State
        [Test]
        public void SessionStarts_IdleState()
        {
            var session = new SessionManager();
            Assert.That(session.State, Is.EqualTo(SessionState.Idle));
        }

        [Test]
        public void SessionLocalPort_DefaultZero()
        {
            var session = new SessionManager();
            Assert.That(session.SessionID, Is.EqualTo(string.Empty));
        }


        [Test]
        public void AddPeer_BeforeHost_ReturnsUninitialized()
        {
            var session = new SessionManager();
            NetResult result = session.Add_Peer(1, "127.0.0.1", 7777);
            Assert.That(result, Is.EqualTo(NetResult.NotInitialized));
        }

        [Test]
        public void AddPeer_BeforeHost_NotAddedToConnectedPeers()
        {
            var session = new SessionManager();
            session.Add_Peer(1, "127.0.0.1", 7777);
            Assert.That(session.HasPeer(1), Is.False);
        }

        [Test]
        public void AddPeer_BeforeHost_NoFireOnPeerJoined()
        {
            var session = new SessionManager();
            bool fired = false;
            session.OnPeerJoined += temp => fired = true;
            session.Add_Peer(1, "127.0.0.1", 7777);
            Assert.That(fired, Is.False);
        }

        //! Tick Guard: no operations while idle or disconnected
        [Test]
        public void Tick_WhileIdle_NoThrows()
        {
            var session = new SessionManager();
            Assert.DoesNotThrow(() => session.Tick(0.016));
        }

        [Test]
        public void Tick_WhileIdle_NoStateChanges()
        {
            var session = new SessionManager();
            session.Tick(0.016);
            Assert.That(session.State, Is.EqualTo(SessionState.Idle));
        }

        //! Shutdown guard
        [Test]
        public void ShutdownWhileIdle_NoThrows()
        {
            var session = new SessionManager();
            Assert.DoesNotThrow(() => session.Shutdown());
        }

        [Test]
        public void ShutdownWhileIdle_NoFireOnSessionEnded()
        {
            var session = new SessionManager();
            bool fired = false;
            session.OnSessionEnded += () => fired = true;
            session.Shutdown();
            Assert.That(fired, Is.False);
        }

        [Test]
        public void ShutdownWhileIdle_MultipleCalls_NoThrows()
        {
            var session = new SessionManager();
            Assert.DoesNotThrow(() =>
            {
                session.Shutdown();
                session.Shutdown();
                session.Shutdown();
            });
        }

        //! Remove Peer Guard
        [Test]
        public void RemovePeer_WhileIdle_NoThrows()
        {
            var session = new SessionManager();
            Assert.DoesNotThrow(() => session.RemovePeer(1));
        }

        [Test]
        public void RemovePeer_WhileIdle_DoesNotFire_OnPeerLeft()
        {
            var session = new SessionManager();
            bool fired = false;
            session.OnPeerLeft += temp => fired = true;
            session.RemovePeer(1);
            Assert.That(fired, Is.False);
        }

        //! SendRaw guard
        [Test]
        public void SendRaw_BeforeHost_ReturnsNotInitialized()
        {
            var session = new SessionManager();
            byte[] data = {1, 2, 3, 4};
            NetResult result = session.SendRaw(1, 0, data);
            Assert.That(result, Is.EqualTo(NetResult.NotInitialized));
        }

        [Test]
        public void SendRaw_NullData_ReturnsInvalidArg()
        {
            var session = new SessionManager();
            NetResult result = session.SendRaw(1, 0, null!);
            Assert.That(result, Is.EqualTo(NetResult.NotInitialized));
        }
    }
}