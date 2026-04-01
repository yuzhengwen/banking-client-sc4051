#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <chrono>
#include <limits>
#include <sstream>

#include "OpCode.h"
#include "Message.h"
#include "Marshaller.h"
#include "RequestBuilder.h"
#include "InvocationLayer.h"
#include "UDPSocket.h"
#include <cstdint>

// declaration for getInput helper method
template <typename T>
static T getInput(const std::string& prompt,
	T min = std::numeric_limits<T>::lowest(),
	T max = (std::numeric_limits<T>::max)());

// ------------------------------------------------------------------ reply helpers

// Read a single string from a reply body (used for confirmation / error messages)
static std::string readReplyString(const Message& msg) {
	size_t offset = 0;
	return Marshaller::readString(msg.body, offset);
}

// Read a single float from a reply body (balance / new balance)
static float readReplyFloat(const Message& msg) {
	size_t offset = 0;
	return Marshaller::readFloat(msg.body, offset);
}

// Read a single int32 from a reply body (account number)
static int32_t readReplyInt(const Message& msg) {
	size_t offset = 0;
	return Marshaller::readInt(msg.body, offset);
}

// ------------------------------------------------------------------ operations

static void doOpenAccount(InvocationLayer& inv) {
	std::string name, password, currStr;
	float balance;

	std::cout << "  Holder name   : "; std::getline(std::cin, name);
	std::cout << "  Password      : "; std::getline(std::cin, password);
	std::cout << "  Currency (SGD/USD/EUR/GBP): "; std::getline(std::cin, currStr);
	balance = getInput<float>("  Initial balance: ", 0);

	Currency cur = currencyFromString(currStr);
	auto packet = RequestBuilder::buildOpenAccount(cur, balance, name, password, 0);

	try {
		Message reply = inv.sendRequest(packet);
		if (reply.status == Status::OK)
			std::cout << "  [OK] Account opened. Account number: " << readReplyInt(reply) << "\n";
		else
			std::cout << "  [ERROR] " << statusToString(reply.status) << "\n";
	}
	catch (const std::exception& e) {
		std::cout << "  [FAIL] " << e.what() << "\n";
	}
}

static void doCloseAccount(InvocationLayer& inv) {
	int32_t accountNo;
	std::string name, password;

	accountNo = getInput<int32_t>("  Account number: ", 0);
	std::cout << "  Holder name   : "; std::getline(std::cin, name);
	std::cout << "  Password      : "; std::getline(std::cin, password);

	auto packet = RequestBuilder::buildCloseAccount(accountNo, name, password, 0);

	try {
		Message reply = inv.sendRequest(packet);
		if (reply.status == Status::OK)
			std::cout << "  [OK] " << "\n";
		else
			std::cout << "  [ERROR] " << statusToString(reply.status) << "\n";
	}
	catch (const std::exception& e) {
		std::cout << "  [FAIL] " << e.what() << "\n";
	}
}

static void doDepositWithdraw(InvocationLayer& inv) {
	int32_t accountNo;
	std::string name, password, currStr;
	float amount;

	accountNo = getInput<int32_t>("  Account number: ", 0);
	std::cout << "  Holder name    : "; std::getline(std::cin, name);
	std::cout << "  Password       : "; std::getline(std::cin, password);
	std::cout << "  Currency       : "; std::getline(std::cin, currStr);
	amount = getInput<float>("  Amount (+dep / -withdraw): ", -10000.0f, 10000.0f);

	Currency cur = currencyFromString(currStr);
	auto packet = RequestBuilder::buildDepositWithdraw(accountNo, cur, amount, name, password, 0);

	try {
		Message reply = inv.sendRequest(packet);
		if (reply.status == Status::OK)
			std::cout << "  [OK] New balance: " << readReplyFloat(reply) << " " << currStr << "\n";
		else
			std::cout << "  [ERROR] " << statusToString(reply.status) << "\n";
	}
	catch (const std::exception& e) {
		std::cout << "  [FAIL] " << e.what() << "\n";
	}
}

static void doQueryBalance(InvocationLayer& inv) {
	int32_t accountNo;
	std::string name, password;

	accountNo = getInput<int32_t>("  Account number: ", 0);
	std::cout << "  Holder name   : "; std::getline(std::cin, name);
	std::cout << "  Password      : "; std::getline(std::cin, password);

	auto packet = RequestBuilder::buildQueryBalance(accountNo, name, password, 0);

	try {
		Message reply = inv.sendRequest(packet);
		if (reply.status == Status::OK)
			std::cout << "  [OK] Balance: " << readReplyFloat(reply) << "\n";
		else
			std::cout << "  [ERROR] " << statusToString(reply.status) << "\n";
	}
	catch (const std::exception& e) {
		std::cout << "  [FAIL] " << e.what() << "\n";
	}
}

static void doTransfer(InvocationLayer& inv) {
	int32_t srcNo, dstNo;
	std::string name, password;
	float amount;
	srcNo = getInput<int32_t>("  Source account : ", 0);
	dstNo = getInput<int32_t>("  Dest account   : ", 0);
	std::cout << "  Your name      : "; std::getline(std::cin, name);
	std::cout << "  Password       : "; std::getline(std::cin, password);
	amount = getInput<float>("  Amount         : ", -10000.0f, 10000.0f);

	auto packet = RequestBuilder::buildTransferFunds(srcNo, dstNo, amount, name, password, 0);

	try {
		Message reply = inv.sendRequest(packet);
		if (reply.status == Status::OK)
			std::cout << "  [OK] Transfer done. New source balance: " << readReplyFloat(reply) << "\n";
		else
			std::cout << "  [ERROR] " << statusToString(reply.status) << "\n";
	}
	catch (const std::exception& e) {
		std::cout << "  [FAIL] " << e.what() << "\n";
	}
}

// ------------------------------------------------------------------ monitor mode

static void parseAndPrintCallback(const std::vector<uint8_t>& data) {
	// Callback body starts at byte 12 (after the 12-byte header)
	// Body: accountNumber(int32) | currency(byte) | newBalance(float32)
	if (data.size() < 12 + 9) {
		std::cout << "  [Callback] Packet too short to parse\n";
		return;
	}
	size_t offset = 0;

	// Parse body directly (skip header — we already know it's a callback)
	std::vector<uint8_t> body(data.begin() + 12, data.end());
	int32_t  accountNo = Marshaller::readInt(body, offset);
	uint8_t  currByte = Marshaller::readByte(body, offset);
	float    newBalance = Marshaller::readFloat(body, offset);
	Currency currency = static_cast<Currency>(currByte);

	if (newBalance < 0)
		std::cout << "  [Callback] Account " << accountNo << " was CLOSED\n";
	else
		std::cout << "  [Callback] Account " << accountNo
		<< " updated — balance: " << newBalance
		<< " " << currencyToString(currency) << "\n";
}

static void doRegisterMonitor(InvocationLayer& inv) {
	int32_t intervalSeconds;
	intervalSeconds = getInput<int32_t>("  Monitor interval (seconds): ", 30);

	auto packet = RequestBuilder::buildRegisterMonitor(intervalSeconds, 0);

	try {
		// Send the registration request and wait for the server's confirmation
		Message reply = inv.sendRequest(packet);
		if (reply.status != Status::OK) {
			std::cout << "  [ERROR] " << statusToString(reply.status) << "\n";
			return;
		}
		std::cout << "  [OK] " << "\n";
		std::cout << "  Listening for callbacks for " << intervalSeconds << " seconds...\n";

		// Switch to a short 500ms timeout so we can poll the deadline
		inv.socket().setTimeout(500);

		auto deadline = std::chrono::steady_clock::now()
			+ std::chrono::seconds(intervalSeconds);

		while (std::chrono::steady_clock::now() < deadline) {
			std::vector<uint8_t> data = inv.socket().receive();
			if (data.empty()) continue;   // timeout — just check the deadline again

			// Sanity check: is this a callback packet?
			if (data.size() >= 5 && data[4] == static_cast<uint8_t>(OpCode::NOTIFY))
				parseAndPrintCallback(data);
		}

		// Restore normal timeout
		inv.socket().setTimeout(InvocationLayer::TIMEOUT_MS);
		std::cout << "  Monitor interval expired.\n";

	}
	catch (const std::exception& e) {
		std::cout << "  [FAIL] " << e.what() << "\n";
		inv.socket().setTimeout(InvocationLayer::TIMEOUT_MS);
	}
}

static void dropNext(InvocationLayer& inv) {
	auto packet = RequestBuilder::buildDropNext(0);
	try {
		Message reply = inv.sendRequest(packet);
		if (reply.status == Status::OK)
			std::cout << "  [OK] Next request will be dropped by the server.\n";
		else
			std::cout << "  [ERROR] " << statusToString(reply.status) << "\n";
	}
	catch (const std::exception& e) {
		std::cout << "  [FAIL] " << e.what() << "\n";
	}
}

// ------------------------------------------------------------------ menu

static void printMenu() {
	std::cout << "\n===== Banking Client =====\n";
	std::cout << "  1. Open account\n";
	std::cout << "  2. Close account\n";
	std::cout << "  3. Deposit / Withdraw\n";
	std::cout << "  4. Query balance\n";
	std::cout << "  5. Transfer funds\n";
	std::cout << "  6. Register monitor\n";
	std::cout << "  7. Drop Next\n";
	std::cout << "  0. Exit\n";
	std::cout << "Choice: ";
}

template <typename T>
static T getInput(const std::string& prompt, T min, T max) {
	T val;
	std::string line;

	while (true) {
		std::cout << prompt;

		// Read the entire line from the user
		if (!std::getline(std::cin, line)) {
			// Handle EOF or stream errors
			continue;
		}

		// Use stringstream to parse the line
		std::stringstream ss(line);
		char remaining;

		// ss >> val: reads the number
		// !(ss >> remaining): ensures there is NO non-whitespace character after the number
		if ((ss >> val) && !(ss >> remaining) && (val >= min && val <= max)) {
			return val;
		}

		std::cout << "  [!] Invalid input. Please enter a value between "
			<< min << " and " << max << ".\n";
	}
}

// ------------------------------------------------------------------ main

int main(int argc, char* argv[]) {
	// Usage: client.exe <host> <port> [amo|alo]
	std::string host = (argc > 1) ? argv[1] : "localhost";
	int         port = (argc > 2) ? std::stoi(argv[2]) : 2222;
	bool        atMostOnce = (argc < 4) || (std::string(argv[3]) == "amo");

	std::cout << "Distributed Banking Client\n";
	std::cout << "Server : " << host << ":" << port << "\n";
	std::cout << "Mode   : " << (atMostOnce ? "at-most-once" : "at-least-once") << "\n";

	try {
		InvocationLayer inv(host, port, atMostOnce);

		int choice = -1;
		while (choice != 0) {
			printMenu();

			// getIntInput will loop internally until the user gives a valid 0-6.
			// Once it returns, 'choice' is GUARANTEED to be valid.
			choice = getInput<int>("Choice: ", 0, 7);

			switch (choice) {
			case 1: doOpenAccount(inv); break;
			case 2: doCloseAccount(inv); break;
			case 3: doDepositWithdraw(inv); break;
			case 4: doQueryBalance(inv); break;
			case 5: doTransfer(inv); break;
			case 6: doRegisterMonitor(inv); break;
			case 7: dropNext(inv); break;
			case 0: std::cout << "Bye.\n"; break;
			default: std::cout << "  [!] Invalid choice, try again.\n"; break;
			}
		}
	}
	catch (const std::exception& e) {
		std::cerr << "Fatal error: " << e.what() << "\n";
		return 1;
	}

	return 0;
}
