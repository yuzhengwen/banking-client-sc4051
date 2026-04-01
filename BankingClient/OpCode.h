#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>

// Must match model/OpCode.java exactly.
// These byte values appear in every datagram header at byte offset 4.
enum class OpCode : uint8_t {
	OPEN_ACCOUNT = 0x01,
	CLOSE_ACCOUNT = 0x02,
	DEPOSIT_WITHDRAW = 0x03,
	REGISTER_MONITOR = 0x04,
	QUERY_BALANCE = 0x05,   // idempotent
	TRANSFER_FUNDS = 0x06,   // non-idempotent
	DROP_NEXT = 0x07,
	NOTIFY = 0xFF    // server-pushed, no corresponding request. using "CALLBACK" runs into naming conflict with smth in Windows headers.
};

// Message type field (byte offset 5 in header)
enum class MsgType : uint8_t {
	REQUEST = 0x00,
	REPLY = 0x01,
	CALLBACK = 0x02
};

// Status field (byte offset 6 in header)
enum class Status : uint8_t {
	OK = 0x00,
	ERROR = 0x01,
	ERR_MALFORMED_REQ = 0x02,
	ERR_INSUFFICIENT_FUNDS = 0x0C, // 11
	ERR_CURRENCY_MISMATCH = 0x0D, // 12
	ERR_ACC_NOT_FOUND = 0x15, // 21
	ERR_WRONG_PASSWORD = 0x16, // 22
	ERR_NAME_MISMATCH = 0x17, // 23
	ERR_DST_ACC_NOT_FOUND = 0x1F, // 31
	ERR_DST_WRONG_PASSWORD = 0x20, // 32
	ERR_DST_NAME_MISMATCH = 0x21 // 33
};
inline std::string statusToString(Status s) {
	switch (s) {
	case Status::OK: return "OK";
	case Status::ERROR: return "ERROR";
	case Status::ERR_MALFORMED_REQ: return "MALFORMED_REQUEST";
	case Status::ERR_INSUFFICIENT_FUNDS: return "INSUFFICIENT_FUNDS";
	case Status::ERR_CURRENCY_MISMATCH: return "CURRENCY_MISMATCH";
	case Status::ERR_ACC_NOT_FOUND: return "ACCOUNT_NOT_FOUND";
	case Status::ERR_WRONG_PASSWORD: return "WRONG_PASSWORD";
	case Status::ERR_NAME_MISMATCH: return "NAME_MISMATCH";
	case Status::ERR_DST_ACC_NOT_FOUND: return "DESTINATION_ACCOUNT_NOT_FOUND";
	case Status::ERR_DST_WRONG_PASSWORD: return "DESTINATION_WRONG_PASSWORD";
	case Status::ERR_DST_NAME_MISMATCH: return "DESTINATION_NAME_MISMATCH";
	default: return "???";
	}
}

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
