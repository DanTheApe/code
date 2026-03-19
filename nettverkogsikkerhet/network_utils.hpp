#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace net {

struct LocalNetInfo {
	std::string localIp;
	std::string broadcastIp;
};

std::vector<std::string> split(const std::string& value, char delimiter);
std::string trim(const std::string& value);

int createUdpSocket(bool reuseAddr);
bool bindUdpSocket(int sockFd, std::uint16_t port, bool bindAny);

bool setBroadcastEnabled(int sockFd);
bool setMulticastTtl(int sockFd, int ttl);
bool setReadTimeoutMs(int sockFd, int timeoutMs);

bool sendUdpTo(int sockFd, const std::string& payload, const std::string& ip, std::uint16_t port);

LocalNetInfo guessLocalNetInfo();
std::string localIpGuess();
std::string nowIsoUtc();

} // namespace net