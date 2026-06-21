/*
    * Handles cryptographic key management for Aegis engine sessions

    ! RESPONSIBILITIES
        1. Generate cryptographically random 120-bit session keys
        2. Derive unique per-peer keys from a master session key
        3. Pass keys to the native CryptoProvider via P/Invoke
*/

using System;
using System.Security.Cryptography;

namespace Aegis.Managed.Cryptography
{
    public static class SecurityManager
    {
        public const int keyBytes = 16; //must match AES_128 key length in bytes

        // * Key Generation
        public static byte[] GenerateSessionKey()
        {
            //allocate exactly 16 bytes
            byte[] key = new byte[keyBytes];

            using var rng = RandomNumberGenerator.Create();
            rng.GetBytes(key);
            return key;
        }

        // * Per peer key derivation
        public static byte[] DerivePeerKey(byte[] masterKey, ulong peerID)
        {
            if(masterKey == null || masterKey.Length != keyBytes)
                throw new ArgumentException($"Master Key must be exactly {keyBytes} bytes.", nameof(masterKey));
            
            byte[] peerBytes = new byte[8];
            peerBytes[0] = (byte)((peerID >> 56) & 0xFF);
            peerBytes[1] = (byte)((peerID >> 48) & 0xFF);
            peerBytes[2] = (byte)((peerID >> 40) & 0xFF);
            peerBytes[3] = (byte)((peerID >> 32) & 0xFF);
            peerBytes[4] = (byte)((peerID >> 24) & 0xFF);
            peerBytes[5] = (byte)((peerID >> 16) & 0xFF);
            peerBytes[6] = (byte)((peerID >> 8) & 0xFF);
            peerBytes[7] = (byte)(peerID & 0xFF);

            using var hmac = new HMACSHA256(masterKey);
            byte[] derived = hmac.ComputeHash(peerBytes);

            //Truncate to AES_128 key size
            byte[] peerKey = new byte[keyBytes];
            Array.Copy(derived, peerKey, keyBytes);
            return peerKey;
        }

        // * Native Key delivery

        public static unsafe void SetNativeSessionKey(byte[] key)
        {
            if(key == null || key.Length != keyBytes)
                throw new ArgumentException($"Key must be exactly {keyBytes} bytes.", nameof(key));
            
            fixed (byte* p = key)
            {
                Interop.NativeBindings.Aegis_SetSessionKey(p);
            }

            Array.Clear(key, 0, key.Length);
        }

        public static bool NativeHasKey()
        {
            return Interop.NativeBindings.Aegis_HasSessionKey() != 0;
        }
    }
}