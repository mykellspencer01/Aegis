/*
    * Summary: Implementation of the cross-platform UDPSocket class
    * Platform Differences Handled:
        - WinSock2 Initialization/cleanup on Windows
        - Non-blocking mode via ioctlsocket (windows) vs fcntl (POSIX)
        - Error-code retrieval via WSAGetLastError() vs errno
    * Internal Logging uses the NETENGINE_LOGGING macro guard 
        - Debug output is compiled out in release builds
*/

#include "UDPSocket.h"
#include <cstring>
#include <cstdio>

#ifdef NETENGINE_LOGGING
    #include <cstdio>
    #define NE_LOG(fmt, ...) std::printf("[Transport] " fmt "\n", ##__VA_ARGS__)
#else
    #define NE_LOG(fmt, ...) (void)0
#endif


namespace Aegis {
namespace Transport{

// * Constructor/Destructor
UDPSocket::UDPSocket(uint16_t localPort)
    : t_socket(INVALID_SOCKET_HANDLE)
    , t_localPort(localPort)
    , t_isOpen(false)
{

#if defined(_WIN32) || defined(_WIN64)
    static bool wsaInit = []() -> bool {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            NE_LOG("WSAStartup failed with error: %d\n", result);
            return false;
        }
        return true;
    }();
    (void)wsaInit;
#endif
}

UDPSocket::~UDPSocket()
{
    Close();
}


// * Open: allocate, bind, set non-blocking
NetResult UDPSocket::Open()
{
    if (t_isOpen)
    {
        NE_LOG("Open() called on already-open socket (port %u)", t_localPort);
        return NetResult::AlreadyInitialized;
    }

    // * Create UDP Socket
    t_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (t_socket == INVALID_SOCKET_HANDLE)
    {
        NE_LOG("socket() failed.");
        return NetResult::SocketError;
    }

    // * Allow reuse of local port on all interfaces (INADDR_ANY)
    sockaddr_in bindAddr = {};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = INADDR_ANY;
    bindAddr.sin_port = htons(t_localPort);

    if(bind(t_socket, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) != 0)
    {
        NE_LOG("bind() failed on port %u", t_localPort);
        Close(); 
        return NetResult::SocketError;
    }

    if(t_localPort == 0)
    {
        sockaddr_in actual{};
        socklen_t len = sizeof(actual);
        if(getsockname(t_socket, reinterpret_cast<sockaddr*>(&actual), &len) == 0)
        {
            t_localPort = ntohs(actual.sin_port);
        }
    }

    // * Set non-blocking mode
    #if defined(_WIN32) || defined(_WIN64)
        u_long nonBlocking = 1;
        if (ioctlsocket(t_socket, FIONBIO, &nonBlocking) != 0)
        {
            NE_LOG("ioctlsocket() failed to set non-blocking mode.");
            Close();
            return NetResult::SocketError;
        }
    #else
        int flags = fcntl(t_socket, F_GETFL, 0);
        if (flags == -1 || fcntl(t_socket, F_SETFL, flags | O_NONBLOCK) == -1)
        {
            NE_LOG("fcntl() failed to set non-blocking mode.");
            Close();
            return NetResult::SocketError;
        }
    #endif

    t_isOpen = true;
    NE_LOG("Socket opened on local port %u", t_localPort);
    return NetResult::Ok;
}

// * Close
void UDPSocket::Close()
{
    if(t_socket != INVALID_SOCKET_HANDLE)
    {
        #if defined(_WIN32) || defined(_WIN64)
            closesocket(t_socket);
        #else
            close(t_socket);
        #endif
        t_socket = INVALID_SOCKET_HANDLE;
    }
    t_isOpen = false;
    NE_LOG("Socket on port %u closed", t_localPort);
}

bool UDPSocket::IsOpen() const{ return t_isOpen; }
uint16_t UDPSocket::GetLocalPort() const { return t_localPort; }

// * Send
int32_t UDPSocket::Send(const RemoteEndpoint& dest, const uint8_t* data, uint32_t length)
{
    if(!t_isOpen || length == 0 || length > NETENGINE_MAX_PACKET_SIZE)
        return -1;
    sockaddr_in destAddr = MakeSockAddr(dest);
    int sent = sendto(
        t_socket,
        reinterpret_cast<const char*>(data), 
        static_cast<int> (length), 
        0,
        reinterpret_cast<sockaddr*>(&destAddr),
        sizeof(destAddr)
    );

    if(sent < 0) NE_LOG("sendto() failed to %s:%u", dest.ip, dest.port);
    return static_cast<int32_t>(sent);
}

// * Receive
int32_t UDPSocket::Receive(RemoteEndpoint& sender, uint8_t* buffer, uint32_t bufferSize)
{
    if(!t_isOpen) return -1;
    sockaddr_in senderAddr;
    socklen_t addrLen = sizeof(senderAddr);
    int received = recvfrom(
        t_socket,
        reinterpret_cast<char*>(buffer),
        static_cast<int>(bufferSize),
        0,
        reinterpret_cast<sockaddr*>(&senderAddr),
        &addrLen
    );

    if(received < 0)
    {
        #if defined(_WIN32) || defined(_WIN64)
            if(WSAGetLastError() == WSAEWOULDBLOCK) return 0;
        #else
            if(errno == EWOULDBLOCK || errno == EAGAIN) return 0;
        #endif
        return -1;
    }

    // Fill caller's endpoint struct
    inet_ntop(AF_INET, &senderAddr.sin_addr, sender.ip, sizeof(sender.ip));
    sender.port = ntohs(senderAddr.sin_port);
    return static_cast<int32_t>(received);
}

// * Helper
sockaddr_in UDPSocket::MakeSockAddr(const RemoteEndpoint& ep)
{
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ep.port);
    inet_pton(AF_INET, ep.ip, &addr.sin_addr);
    return addr;
}

UDPSocket& UDPSocket::operator=(UDPSocket&& other) noexcept
{
    if(this != &other)
    {
        Close();
        t_socket = other.t_socket;
        t_localPort = other.t_localPort;
        t_isOpen = other.t_isOpen;

        other.t_socket = INVALID_SOCKET_HANDLE;
        other.t_isOpen = false;
        other.t_localPort = 0;
    }
    return *this;
}

} //namespace transport
} //namespace netengine