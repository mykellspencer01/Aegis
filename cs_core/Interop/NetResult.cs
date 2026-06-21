/*
    * C# mirror of the C++ NetResult enum defined in EngineTypes.h
*/

namespace Aegis.Managed.Interop
{
    /*
        Return code for all Aegis engine native operations
        Maps 1-1 with C++'s NetResult
        Negative values are errors; zero is success; positive is informational
    */

    public enum NetResult : int
    {
        Ok = 0, // opeation completed successfully
        Pending = 1, // operation is queued; result not available
        Duplicate = 2, // Packet accepted, but is known dupe. Ack already sent for this seq #
        InvalidArg = -1, // a null or out-of-range argument
        SocketError = -2, // an OS-level socket operation failed
        Timeout = -3, // operation exceeded its deadling
        BufferOverflow = -4, // a write would exceed allocated buffer cap
        CryptoFail = -5, // Encryption, Decryption, or HMAC verification failed
        SerializeError = -6, // Bit writer or reader encountered malformed data
        UnknownPeer = -7, // Peer GUID not found in current session
        SessionFull = -8, // session has max number of peers already
        NotInitialized = -9, // engine subsystem not yet initialized
        GenreModuleNotFound = -10, // requested genre module has not been registered
        AlreadyInitialized = -11 // init called a second time without an intervening shutdown
    }
}