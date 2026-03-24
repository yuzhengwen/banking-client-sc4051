#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "OpCode.h"
#include "Message.h"
#include "Marshaller.h"

/**
 * RequestBuilder — one static method per operation.
 *
 * Each method takes the user's typed inputs, marshals them into a body
 * byte vector, wraps them in a Message header, and returns the complete
 * datagram ready to pass to UDPSocket::send().
 *
 * Field order for every operation must exactly match what the Java handler
 * reads (see handlers/*.java on the server side).
 */
class RequestBuilder {
public:

    // ------------------------------------------------------------------ 0x01
    // Open account
    // Body: currency(1B) | initialBalance(float32) | name(str) | password(str)
    static std::vector<uint8_t> buildOpenAccount(
        Currency currency, float initialBalance,
        const std::string& name, const std::string& password,
        int32_t requestId)
    {
        std::vector<uint8_t> body;
        Marshaller::writeByte  (body, static_cast<uint8_t>(currency));
        Marshaller::writeFloat (body, initialBalance);
        Marshaller::writeString(body, name);
        Marshaller::writeString(body, password);
        return makePacket(requestId, OpCode::OPEN_ACCOUNT, body);
    }

    // ------------------------------------------------------------------ 0x02
    // Close account
    // Body: accountNumber(int32) | name(str) | password(str)
    static std::vector<uint8_t> buildCloseAccount(
        int32_t accountNumber,
        const std::string& name, const std::string& password,
        int32_t requestId)
    {
        std::vector<uint8_t> body;
        Marshaller::writeInt   (body, accountNumber);
        Marshaller::writeString(body, name);
        Marshaller::writeString(body, password);
        return makePacket(requestId, OpCode::CLOSE_ACCOUNT, body);
    }

    // ------------------------------------------------------------------ 0x03
    // Deposit / Withdraw  (positive amount = deposit, negative = withdraw)
    // Body: accountNumber(int32) | currency(1B) | amount(float32) | name(str) | password(str)
    static std::vector<uint8_t> buildDepositWithdraw(
        int32_t accountNumber, Currency currency, float amount,
        const std::string& name, const std::string& password,
        int32_t requestId)
    {
        std::vector<uint8_t> body;
        Marshaller::writeInt   (body, accountNumber);
        Marshaller::writeByte  (body, static_cast<uint8_t>(currency));
        Marshaller::writeFloat (body, amount);
        Marshaller::writeString(body, name);
        Marshaller::writeString(body, password);
        return makePacket(requestId, OpCode::DEPOSIT_WITHDRAW, body);
    }

    // ------------------------------------------------------------------ 0x04
    // Register monitor
    // Body: intervalSeconds(int32)
    static std::vector<uint8_t> buildRegisterMonitor(
        int32_t intervalSeconds,
        int32_t requestId)
    {
        std::vector<uint8_t> body;
        Marshaller::writeInt(body, intervalSeconds);
        return makePacket(requestId, OpCode::REGISTER_MONITOR, body);
    }

    // ------------------------------------------------------------------ 0x05
    // Query balance  (idempotent)
    // Body: accountNumber(int32) | name(str) | password(str)
    static std::vector<uint8_t> buildQueryBalance(
        int32_t accountNumber,
        const std::string& name, const std::string& password,
        int32_t requestId)
    {
        std::vector<uint8_t> body;
        Marshaller::writeInt   (body, accountNumber);
        Marshaller::writeString(body, name);
        Marshaller::writeString(body, password);
        return makePacket(requestId, OpCode::QUERY_BALANCE, body);
    }

    // ------------------------------------------------------------------ 0x06
    // Transfer funds  (non-idempotent)
    // Body: srcAccountNumber(int32) | dstAccountNumber(int32) | amount(float32) | name(str) | password(str)
    static std::vector<uint8_t> buildTransferFunds(
        int32_t srcAccountNumber, int32_t dstAccountNumber, float amount,
        const std::string& name, const std::string& password,
        int32_t requestId)
    {
        std::vector<uint8_t> body;
        Marshaller::writeInt   (body, srcAccountNumber);
        Marshaller::writeInt   (body, dstAccountNumber);
        Marshaller::writeFloat (body, amount);
        Marshaller::writeString(body, name);
        Marshaller::writeString(body, password);
        return makePacket(requestId, OpCode::TRANSFER_FUNDS, body);
    }

private:
    // Wrap a body in a full Message (header + body) and serialise to bytes.
    static std::vector<uint8_t> makePacket(int32_t requestId, OpCode opcode,
                                            const std::vector<uint8_t>& body)
    {
        Message msg;
        msg.requestId = requestId;
        msg.opcode    = opcode;
        msg.msgType   = MsgType::REQUEST;
        msg.status    = Status::OK;
        msg.body      = body;
        return msg.toBytes();
    }
};
