#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <iostream>
#include "UDPSocket.h"
#include "Message.h"

/**
 * InvocationLayer — handles the send/retry/receive loop.
 *
 * Maintains a monotonically increasing requestId counter.
 * On each new request, increments the counter.
 * On retransmission (timeout), reuses the same requestId so the server's
 * DedupFilter (at-most-once mode) can detect it as a duplicate.
 *
 * At-least-once: server re-executes every received request.
 * At-most-once:  server checks DedupFilter and returns cached reply on duplicate.
 *                The CLIENT SIDE IS IDENTICAL — the difference is entirely on the server.
 *
 * sendRequest() returns the parsed reply Message on success,
 * or throws std::runtime_error if all retries are exhausted.
 */
class InvocationLayer {
public:
    static constexpr int MAX_RETRIES  = 5;
    static constexpr int TIMEOUT_MS   = 3000;   // 3 seconds per attempt

    InvocationLayer(const std::string& serverHost, int serverPort, bool atMostOnce)
        : host_(serverHost)
        , port_(serverPort)
        , atMostOnce_(atMostOnce)
        , nextRequestId_(1)
        , socket_(TIMEOUT_MS)
    {
        std::cout << "[InvocationLayer] Targeting " << host_ << ":" << port_
                  << " (" << (atMostOnce_ ? "at-most-once" : "at-least-once") << ")\n";
    }

    /**
     * Send a pre-built packet and wait for a reply.
     * `packet` is the full datagram bytes from RequestBuilder.
     * `newRequest` — pass true for a fresh request (increments requestId),
     *                pass false to reuse the current requestId (used internally for retries).
     *
     * Returns the parsed reply Message.
     * Throws std::runtime_error after MAX_RETRIES timeouts.
     */
    Message sendRequest(std::vector<uint8_t> packet, bool newRequest = true) {
        if (newRequest)
            currentRequestId_ = nextRequestId_++;

        // Patch the requestId into the packet header (bytes 0–3).
        // RequestBuilder already wrote a requestId, but InvocationLayer
        // owns the counter, so we overwrite it here.
        patchRequestId(packet, currentRequestId_);

        for (int attempt = 1; attempt <= MAX_RETRIES; ++attempt) {
            std::cout << "[Invocation] Sending reqId=" << currentRequestId_
                      << " attempt " << attempt << "/" << MAX_RETRIES << "\n";

            socket_.send(packet, host_, port_);

            std::vector<uint8_t> replyData = socket_.receive();

            if (replyData.empty()) {
                std::cout << "[Invocation] Timeout — ";
                if (attempt < MAX_RETRIES)
                    std::cout << "retrying...\n";
                else
                    std::cout << "giving up.\n";
                continue;
            }

            // Got a reply — parse and return it
            Message reply = Message::fromBytes(replyData);
            std::cout << "[Invocation] Got reply for reqId=" << reply.requestId
                      << " status=" << (reply.status == Status::OK ? "OK" : "ERROR") << "\n";
            return reply;
        }

        throw std::runtime_error("No reply after " + std::to_string(MAX_RETRIES) + " attempts");
    }

    // Access the socket directly (used by monitor mode to set short timeout
    // and loop reading callbacks)
    UDPSocket& socket() { return socket_; }

    int currentRequestId() const { return currentRequestId_; }

private:
    std::string host_;
    int         port_;
    bool        atMostOnce_;
    int32_t     nextRequestId_;
    int32_t     currentRequestId_ = 0;
    UDPSocket   socket_;

    // Overwrite bytes 0–3 of the packet with the correct requestId.
    // This lets InvocationLayer own the ID counter regardless of what
    // RequestBuilder put in the packet.
    static void patchRequestId(std::vector<uint8_t>& packet, int32_t requestId) {
        uint32_t net = htonl(static_cast<uint32_t>(requestId));
        memcpy(packet.data(), &net, 4);
    }
};
