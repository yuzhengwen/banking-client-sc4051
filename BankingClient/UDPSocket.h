#pragma once

// winsock2.h must come before windows.h
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>
#include <vector>
#include <string>
#include <stdexcept>

#pragma comment(lib, "ws2_32.lib")   // tell the linker to include Winsock

/**
 * UDPSocket — thin RAII wrapper around a Winsock2 UDP socket.
 *
 * Constructor: calls WSAStartup, creates the socket, sets a default timeout.
 * Destructor:  closes the socket, calls WSACleanup.
 *
 * send()    — sends bytes to a specific host:port.
 * receive() — blocks up to timeoutMs milliseconds for an incoming datagram.
 *             Returns the bytes received, or an empty vector on timeout.
 */
class UDPSocket {
public:

    explicit UDPSocket(int timeoutMs = 3000) {
        // Initialise Winsock
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0)
            throw std::runtime_error("WSAStartup failed: " + std::to_string(result));

        // Create a UDP socket
        sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock_ == INVALID_SOCKET) {
            WSACleanup();
            throw std::runtime_error("socket() failed: " + std::to_string(WSAGetLastError()));
        }

        // Bind to any local port (OS chooses)
        sockaddr_in local{};
        local.sin_family      = AF_INET;
        local.sin_addr.s_addr = INADDR_ANY;
        local.sin_port        = 0;
        if (bind(sock_, reinterpret_cast<sockaddr*>(&local), sizeof(local)) == SOCKET_ERROR) {
            closesocket(sock_);
            WSACleanup();
            throw std::runtime_error("bind() failed: " + std::to_string(WSAGetLastError()));
        }

        setTimeout(timeoutMs);
    }

    ~UDPSocket() {
        closesocket(sock_);
        WSACleanup();
    }

    // No copy
    UDPSocket(const UDPSocket&)            = delete;
    UDPSocket& operator=(const UDPSocket&) = delete;

    // ------------------------------------------------------------------ send

    void send(const std::vector<uint8_t>& data, const std::string& host, int port) {
        sockaddr_in dest{};
        dest.sin_family = AF_INET;
        dest.sin_port   = htons(static_cast<uint16_t>(port));

        // Resolve hostname to IP
        addrinfo hints{}, *res = nullptr;
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        if (getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0 || res == nullptr)
            throw std::runtime_error("Cannot resolve host: " + host);
        dest.sin_addr = reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr;
        freeaddrinfo(res);

        int sent = sendto(sock_,
                          reinterpret_cast<const char*>(data.data()),
                          static_cast<int>(data.size()),
                          0,
                          reinterpret_cast<sockaddr*>(&dest),
                          sizeof(dest));
        if (sent == SOCKET_ERROR)
            throw std::runtime_error("sendto() failed: " + std::to_string(WSAGetLastError()));
    }

    // ------------------------------------------------------------------ receive

    // Returns received bytes, or empty vector on timeout.
    std::vector<uint8_t> receive() {
        static const int BUF_SIZE = 65535;
        std::vector<uint8_t> buf(BUF_SIZE);

        sockaddr_in sender{};
        int senderLen = sizeof(sender);

        int received = recvfrom(sock_,
                                reinterpret_cast<char*>(buf.data()),
                                BUF_SIZE,
                                0,
                                reinterpret_cast<sockaddr*>(&sender),
                                &senderLen);

        if (received == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAETIMEDOUT)
                return {};   // empty = timeout
            throw std::runtime_error("recvfrom() failed: " + std::to_string(err));
        }

        buf.resize(received);
        return buf;
    }

    // Change the receive timeout after construction (used by monitor mode)
    void setTimeout(int ms) {
        DWORD timeout = static_cast<DWORD>(ms);
        setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    }

private:
    SOCKET sock_ = INVALID_SOCKET;
};
