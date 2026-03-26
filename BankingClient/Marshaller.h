#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <stdexcept>

// Winsock2 provides htonl / htons / ntohl / ntohs
#include <winsock2.h>

/**
 * Marshaller — mirror of util/Marshaller.java.
 *
 * All multi-byte values are big-endian (network byte order).
 * C++ is typically little-endian on x86/x64, so every multi-byte read/write
 * must go through htonl/htons (host→network) or ntohl/ntohs (network→host).
 *
 * Floats: must go through the IEEE 754 bit pattern as a uint32_t.
 * Never cast float* to uint32_t* directly — use memcpy.
 *
 * Strings: uint16 length prefix (big-endian) + UTF-8 bytes. No null terminator.
 *
 * Usage pattern (writing):
 *   std::vector<uint8_t> buf;
 *   Marshaller::writeInt(buf, accountNo);
 *   Marshaller::writeFloat(buf, amount);
 *   Marshaller::writeString(buf, name);
 *
 * Usage pattern (reading):
 *   size_t offset = 0;
 *   int accountNo = Marshaller::readInt(data, offset);
 *   float amount  = Marshaller::readFloat(data, offset);
 *   std::string name = Marshaller::readString(data, offset);
 */
class Marshaller {
public:

	// ------------------------------------------------------------------ write
	// Each write appends bytes to the end of `buf`.

	static void writeInt(std::vector<uint8_t>& buf, int32_t value) {
		uint32_t net = htonl(static_cast<uint32_t>(value));
		uint8_t bytes[4];
		memcpy(bytes, &net, 4);
		buf.insert(buf.end(), bytes, bytes + 4);
	}

	static void writeFloat(std::vector<uint8_t>& buf, float value) {
		// Step 1: get the raw IEEE 754 bit pattern
		uint32_t bits;
		memcpy(&bits, &value, 4);
		// Step 2: convert to network byte order
		bits = htonl(bits);
		// Step 3: write the 4 bytes
		uint8_t bytes[4];
		memcpy(bytes, &bits, 4);
		buf.insert(buf.end(), bytes, bytes + 4);
	}

	static void writeByte(std::vector<uint8_t>& buf, uint8_t value) {
		buf.push_back(value);
	}

	static void writeBool(std::vector<uint8_t>& buf, bool value) {
		buf.push_back(value ? 0x01 : 0x00);
	}

	// Writes a uint16 length prefix then the string bytes (no null terminator).
	static void writeString(std::vector<uint8_t>& buf, const std::string& value) {
		if (value.size() > 0xFFFF)
			throw std::invalid_argument("String too long to marshal");
		uint16_t len = htons(static_cast<uint16_t>(value.size()));
		uint8_t lenBytes[2];
		memcpy(lenBytes, &len, 2);
		buf.insert(buf.end(), lenBytes, lenBytes + 2);
		buf.insert(buf.end(), value.begin(), value.end());
	}

	// ------------------------------------------------------------------ read
	// Each read extracts bytes from `data` starting at `offset`,
	// then advances offset by the number of bytes consumed.

	static int32_t readInt(const std::vector<uint8_t>& data, size_t& offset) {
		checkBounds(data, offset, 4);
		uint32_t net;
		memcpy(&net, data.data() + offset, 4);
		offset += 4;
		return static_cast<int32_t>(ntohl(net));
	}

	static float readFloat(const std::vector<uint8_t>& data, size_t& offset) {
		checkBounds(data, offset, 4);
		uint32_t net;
		memcpy(&net, data.data() + offset, 4);
		offset += 4;
		uint32_t bits = ntohl(net);
		float value;
		memcpy(&value, &bits, 4);
		return value;
	}

	static uint8_t readByte(const std::vector<uint8_t>& data, size_t& offset) {
		checkBounds(data, offset, 1);
		return data[offset++];
	}

	static bool readBool(const std::vector<uint8_t>& data, size_t& offset) {
		return readByte(data, offset) != 0;
	}

	static std::string readString(const std::vector<uint8_t>& data, size_t& offset) {
		checkBounds(data, offset, 2);
		uint16_t net;
		memcpy(&net, data.data() + offset, 2);
		offset += 2;
		uint16_t len = ntohs(net);
		checkBounds(data, offset, len);
		std::string result(reinterpret_cast<const char*>(data.data() + offset), len);
		offset += len;
		return result;
	}

private:
	static void checkBounds(const std::vector<uint8_t>& data, size_t offset, size_t needed) {
		if (data.size() == 0) {
			std::cout << "Empty body" << std::endl;
			return;
		}
		if (offset + needed > data.size())
			throw std::out_of_range("Marshaller: read past end of buffer");
	}
};
