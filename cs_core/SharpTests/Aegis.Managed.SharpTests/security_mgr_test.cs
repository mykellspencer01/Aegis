using NUnit.Framework;
using Aegis.Managed.Cryptography;
using System;

namespace Aegis.Manager.SharpTests
{
    [TestFixture]
    public class SecurityManagerTests
    {
        //! Generate session key
        [Test]
        public void GenSeshKey_ReturnsCorrectLength()
        {
            byte[] key = SecurityManager.GenerateSessionKey();
            Assert.That(key.Length, Is.EqualTo(SecurityManager.keyBytes));
        }

        [Test]
        public void GenSeshKey_ReturnsNonNull()
        {
            byte[] key = SecurityManager.GenerateSessionKey();
            Assert.That(key, Is.Not.Null);
        }

        [Test]
        public void GenSeshKey_MakesDifferentKey_EachCall()
        {
            byte[] keyA = SecurityManager.GenerateSessionKey();
            byte[] keyB = SecurityManager.GenerateSessionKey();
            Assert.That(keyA, Is.Not.EqualTo(keyB));
        }

        [Test]
        public void GenSeshKey_NotAll_ZeroBytes()
        {
            byte[] key = SecurityManager.GenerateSessionKey();
            byte[] allZero = new byte[SecurityManager.keyBytes];
            Assert.That(key, Is.Not.EqualTo(allZero));
        }

        //! Derive Peer Key for validation
        [Test]
        public void DerivePeerKey_NullMasterKey_ThrowsArgException()
        {
            Assert.Throws<ArgumentException>(
                () => SecurityManager.DerivePeerKey(null!, 1)
            );
        }

        [Test]
        public void DerivePeerKey_WrongLengthMasterKey_ThrowsArgException()
        {
            byte[] badKey = new byte[8];
            Assert.Throws<ArgumentException>(
                () => SecurityManager.DerivePeerKey(badKey, 1)
            );
        }

        [Test]
        public void DerivePeerKey_MasterKeyTooLong_ThrowsArgException()
        {
            byte[] tooLong = new byte[32];
            Assert.Throws<ArgumentException>(
                () => SecurityManager.DerivePeerKey(tooLong, 1)
            );
        }

        //! Determinism and uniqueness
        [Test]
        public void DerivePeerKey_SamePeerID_SameKeyProduced()
        {
            byte[] master = SecurityManager.GenerateSessionKey();
            byte[] keyA = SecurityManager.DerivePeerKey(master, 30);
            byte[] keyB = SecurityManager.DerivePeerKey(master, 30);
            Assert.That(keyA, Is.EqualTo(keyB));
        }

        [Test]
        public void DerivePeerKey_DiffPeerID_DiffKeyProduced()
        {
            byte[] master = SecurityManager.GenerateSessionKey();
            byte[] keyA = SecurityManager.DerivePeerKey(master, 15);
            byte[] keyB = SecurityManager.DerivePeerKey(master, 30);
            Assert.That(keyA, Is.Not.EqualTo(keyB));
        }

        [Test]
        public void DerivePeerKey_DiffMasterKeys_DiffResult_SamePeerID()
        {
            byte[] masterA = SecurityManager.GenerateSessionKey();
            byte[] masterB = SecurityManager.GenerateSessionKey();
            byte[] keyA = SecurityManager.DerivePeerKey(masterA, 25);
            byte[] keyB = SecurityManager.DerivePeerKey(masterB, 25);
            Assert.That(keyA, Is.Not.EqualTo(keyB));
        }

        [Test]
        public void DerivePeerKey_ReturnsCorrectLength()
        {
            byte[] master = SecurityManager.GenerateSessionKey();
            byte[] peerKey = SecurityManager.DerivePeerKey(master, 50);
            Assert.That(peerKey.Length, Is.EqualTo(SecurityManager.keyBytes));
        }

        [Test]
        public void DerivePeerKey_PeerIDZero_NoThrows()
        {
            byte[] master = SecurityManager.GenerateSessionKey();
            Assert.DoesNotThrow(
                () => SecurityManager.DerivePeerKey(master, 0)
            );
        }

        [Test]
        public void DerivePeerKey_MaxPeerID_NoThrows()
        {
            byte[] master = SecurityManager.GenerateSessionKey();
            Assert.DoesNotThrow(
                () => SecurityManager.DerivePeerKey(master, ulong.MaxValue)
            );
        }
    }
}