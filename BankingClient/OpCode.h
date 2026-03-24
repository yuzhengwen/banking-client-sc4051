#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>

// Must match model/OpCode.java exactly.
// These byte values appear in every datagram header at byte offset 4.
enum class OpCode : uint8_t {
    OPEN_ACCOUNT     = 0x01,
    CLOSE_ACCOUNT    = 0x02,
    DEPOSIT_WITHDRAW = 0x03,
    REGISTER_MONITOR = 0x04,
    QUERY_BALANCE    = 0x05,   // idempotent
    TRANSFER_FUNDS   = 0x06,   // non-idempotent
	NOTIFY = 0xFF    // server-pushed, no corresponding request. using "CALLBACK" runs into naming conflict with smth in Windows headers.
};

// Message type field (byte offset 5 in header)
enum class MsgType : uint8_t {
    REQUEST  = 0x00,
    REPLY    = 0x01,
    CALLBACK = 0x02
};

// Status field (byte offset 6 in header)
enum class Status : uint8_t {
    OK    = 0x00,
    ERROR = 0x01
};

// Currency codes — must match model/Currency.java
enum class Currency : uint8_t {
    SGD = 0x00,
    USD = 0x01,
    EUR = 0x02,
    GBP = 0x03
};

inline std::string currencyToString(Currency c) {
    switch (c) {
        case Currency::SGD: return "SGD";
        case Currency::USD: return "USD";
        case Currency::EUR: return "EUR";
        case Currency::GBP: return "GBP";
        default:            return "???";
    }
}

inline Currency currencyFromString(const std::string& s) {
    if (s == "SGD") return Currency::SGD;
    if (s == "USD") return Currency::USD;
    if (s == "EUR") return Currency::EUR;
    if (s == "GBP") return Currency::GBP;
    throw std::invalid_argument("Unknown currency: " + s);
}
