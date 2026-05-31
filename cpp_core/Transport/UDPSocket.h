/*

* Summary: Non-blocking, cross-platform UDP socket
* UDPSocket wraps the OS Socket API behind a single class to promote a platform-agnostic engine

* Threading Model:
    - UDPSocket is not thread safe by itself
    - Transport layer owns one socket and serializes access through its internal mutex
    - Do not share across threads

* Packet Size:
    - Datagrams are limited to NETENGINE_MAX_PACKET_SIZE (1400 bytes)
        + Stays safely below the 1500-byte MTU after IP + UDP headers
        + Avoids fragmentation
*/

#pragma once

#include "EngineTypes.h"
#include <string>
#include <cstdint>

#if defined(_WIN32) || defined(_WIN64)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    using SocketHandle = SOCKET;
    constexpr SocketHandle INVALID_SOCKET_HANDLE = INVALID_SOCKET;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    using SocketHandle = int;
    constexpr SocketHandle INVALID_SOCKET_HANDLE = -1;
#endif


namespace NetEngine {
namespace Transport {

//* From/to address in send/receive calls; mapped by C# PeerEndpoint struct
struct RemoteEndpoint
{
    char ip[46]; //*IPv4 or IPv6 address string
    uint16_t port; //* Port in host byte order
};

/*
* UDPSocket class:
    - Non-blocking UDP socket with send-receive primitives
    
* Lifecycle:
    1. Construct with local port
    2. Call Open() once before I/O
    3. Call Send()/Receive() when needed
    4. Destructor calls Close() automatically
*/

class UDPSocket
{
    public:
        explicit UDPSocket(uint16_t localPort = 0);
        ~UDPSocket();

        // Non-copyable
        UDPSocket(const UDPSocket&) = delete;
        UDPSocket& operator = (const UDPSocket&) = delete;

        // Allocate OS Socket, bind to local port, set non-blocking
        NetResult Open();

        // Close socket and free resources
        void Close();

        // Return true when socket is open and ready for I/O
        bool IsOpen() const;

        // * Dest: destination address and port, data: pointer to byte buffer to send, lenght: # of bytes to send
        int32_t Send(const RemoteEndpoint& dest, const uint8_t* data, uint32_t length);
        // * Sender: sender address and port, Buffer: called-supplied buffer to write datagram into
        int32_t Receive(RemoteEndpoint& sender, uint8_t* buffer, uint32_t bufferSize);
        // * Returns local port the socket is bound to
        uint16_t GetLocalPort() const;

    private:
        SocketHandle t_socket; //Underlying OS socket descriptor
        uint16_t t_localPort; // Requested bind port
        bool t_isOpen; //Guard to prevent double-open
        // * Populate a sockaddr_in froma remote endpoint to send calls
        static sockaddr_in MakeSockAddr(const RemoteEndpoint& ep); 
};

} //namespace Transport
} //naemspace NetEngine