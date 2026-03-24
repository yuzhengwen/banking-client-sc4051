#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include <stdexcept>
#include "OpCode.h"

// Winsock2 for htonl/ntohl
#include <winsock2.h>

/**
 * Message — mirrors model/Message.java exactly.
 *
 * Wire layout (same on both Java server and C++ client):
 *
 *   Offset  Size  Field
 *   0       4     requestId   (int32, big-endian)
 *   4       1     opcode      (byte)
 *   5       1     msgType     (byte)  0=request 1=reply 2=callback
 *   6       1     status      (byte)  0=ok 1=error
 *   7       1     reserved    (always 0x00)
 *   8       4     bodyLen     (int32, big-endian) — number of body bytes
 *   12      N     body        (variable)
 *
 * Total minimum size: 12 bytes (header only, empty body).
 */
struct Message {
    static constexpr int HEADER_SIZE = 12;

    int32_t              requestId = 0;
    OpCode               opcode    = OpCode::QUERY_BALANCE;
    MsgType              msgType   = MsgType::REQUEST;
    Status               status    = Status::OK;
    std::vector<uint8_t> body;

    // --------------------------------------------------------------------
    // Serialise this Message into a flat byte vector ready to sendto().
    // --------------------------------------------------------------------
    std::vector<uint8_t> toBytes() const {
        std::vector<uint8_t> out;
        out.reserve(HEADER_SIZE + body.size());

        // requestId — 4 bytes big-endian
        uint32_t reqNet = htonl(static_cast<uint32_t>(requestId));
        uint8_t reqBytes[4];
        memcpy(reqBytes, &reqNet, 4);
        out.insert(out.end(), reqBytes, reqBytes + 4);

        // opcode, msgType, status, reserved — 1 byte each
        out.push_back(static_cast<uint8_t>(opcode));
        out.push_back(static_cast<uint8_t>(msgType));
        out.push_back(static_cast<uint8_t>(status));
        out.push_back(0x00);   // reserved

        // bodyLen — 4 bytes big-endian
        uint32_t lenNet = htonl(static_cast<uint32_t>(body.size()));
        uint8_t lenBytes[4];
        memcpy(lenBytes, &lenNet, 4);
        out.insert(out.end(), lenBytes, lenBytes + 4);

        // body
        out.insert(out.end(), body.begin(), body.end());

        return out;
    }

    // --------------------------------------------------------------------
    // Parse raw bytes (from recvfrom) into a Message struct.
    // Throws std::runtime_error if the data is too short or malformed.
    // --------------------------------------------------------------------
    static Message fromBytes(const std::vector<uint8_t>& data) {
        if (data.size() < HEADER_SIZE)
            throw std::runtime_error("Packet too short to contain a valid header");

        Message msg;

        // requestId (bytes 0–3)
        uint32_t reqNet;
        memcpy(&reqNet, data.data() + 0, 4);
        msg.requestId = static_cast<int32_t>(ntohl(reqNet));

        // opcode (byte 4)
        msg.opcode = static_cast<OpCode>(data[4]);

        // msgType (byte 5)
        msg.msgType = static_cast<MsgType>(data[5]);

        // status (byte 6)
        msg.status = static_cast<Status>(data[6]);

        // byte 7 is reserved — skip

        // bodyLen (bytes 8–11)
        uint32_t lenNet;
        memcpy(&lenNet, data.data() + 8, 4);
        uint32_t bodyLen = ntohl(lenNet);

        if (data.size() < static_cast<size_t>(HEADER_SIZE + bodyLen))
            throw std::runtime_error("Packet body shorter than bodyLen field claims");

        // body (bytes 12 … 12+bodyLen-1)
        msg.body.assign(data.begin() + HEADER_SIZE,
                        data.begin() + HEADER_SIZE + bodyLen);

        return msg;
    }
};
