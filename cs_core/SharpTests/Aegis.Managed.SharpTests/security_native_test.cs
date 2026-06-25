/*
    * Category B tests
        - SecurityManager.SetNativeSessionKey()
        - SecurityManger.NativeHasKey()
        ! only two manager methods that touch native code
*/

using NUnit.Framework;
using Aegis.Managed.Session;
using Aegis.Managed.Cryptography;
using System;

namespace Aegis.Managed.SharpTests
{
    [TestFixture]
    public class SecurityNativeTests
    {
        //! NativeHasKey - before any key is set in the process

        [Test]
        public void SetNativeSessionKey_ValidKey_NativeHasKeyTrue()
        {
            byte[] key = SecurityManager.GenerateSessionKey();
            SecurityManager.SetNativeSessionKey(key);
            Assert.That(SecurityManager.NativeHasKey(), Is.True);
        }

        [Test]
        public void SetNativeSessionKey_ZeroesManagedArray()
        {
            byte[] key = SecurityManager.GenerateSessionKey();
            byte[] allZero = new byte[SecurityManager.keyBytes];
            SecurityManager.SetNativeSessionKey(key);
            Assert.That(key, Is.EqualTo(allZero));
        }

        [Test]
        public void SetNativeSessionKey_WrongLength_ThrowsArgException()
        {
            byte[] badKey = new byte[8];
            Assert.Throws<ArgumentException>(
                () => SecurityManager.SetNativeSessionKey(badKey)
            );
        }

        [Test]
        public void SetNativeSessionKey_CalledTwiceDiffKeys_NativeHasKeyTrue()
        {
            byte[] keyA = SecurityManager.GenerateSessionKey();
            byte[] keyB = SecurityManager.GenerateSessionKey();

            SecurityManager.SetNativeSessionKey(keyA);
            Assert.That(SecurityManager.NativeHasKey(), Is.True);

            SecurityManager.SetNativeSessionKey(keyA);
            Assert.That(SecurityManager.NativeHasKey(), Is.True);
        }

        [Test]
        public void SetNativeSessionKey_ActiveSession_NotRequired()
        {
            byte[] key = SecurityManager.GenerateSessionKey();
            Assert.DoesNotThrow(
                () => SecurityManager.SetNativeSessionKey(key)
            );
            Assert.That(SecurityManager.NativeHasKey(), Is.True);
        }

        [Test]
        public void SetNativeSessionKey_DerivedPeerKey_CanBeSetNatively()
        {
            byte[] masterKey = SecurityManager.GenerateSessionKey();
            byte[] peerKey = SecurityManager.DerivePeerKey(masterKey, 1);
            Assert.DoesNotThrow(
                () => SecurityManager.SetNativeSessionKey(peerKey)
            );
            Assert.That(SecurityManager.NativeHasKey(), Is.True);
        }
    }
}