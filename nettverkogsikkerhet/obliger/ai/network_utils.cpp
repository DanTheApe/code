#include "network_utils.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace net {

std::vector<std::string> split(const std::string& value, char delimiter) {
    std::vector<std::string> parts;
    std::string current;
    for (char ch : value) {
        if (ch == delimiter) {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    parts.push_back(current);
    return parts;
}

std::string trim(const std::string& value) {
    const auto isSpace = [](unsigned char ch) {
        return std::isspace(ch) != 0;
    };

    auto start = std::find_if_not(value.begin(), value.end(), isSpace);
    auto end = std::find_if_not(value.rbegin(), value.rend(), isSpace).base();

    if (start >= end) {
        return "";
    }
    return std::string(start, end);
}

int createUdpSocket(bool reuseAddr) {
    int sockFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockFd < 0) {
        return -1;
    }

    if (reuseAddr) {
        int yes = 1;
        if (setsockopt(sockFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
            close(sockFd);
            return -1;
        }
#ifdef SO_REUSEPORT
        setsockopt(sockFd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));
#endif
    }
    return sockFd;
}

bool bindUdpSocket(int sockFd, std::uint16_t port, bool bindAny) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (bindAny) {
        address.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }

    return bind(sockFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;
}

bool setBroadcastEnabled(int sockFd) {
    int yes = 1;
    return setsockopt(sockFd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes)) == 0;
}

bool setMulticastTtl(int sockFd, int ttl) {
    unsigned char ttlValue = static_cast<unsigned char>(ttl);
    return setsockopt(sockFd, IPPROTO_IP, IP_MULTICAST_TTL, &ttlValue, sizeof(ttlValue)) == 0;
}

bool setReadTimeoutMs(int sockFd, int timeoutMs) {
    timeval tv{};
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    return setsockopt(sockFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0;
}

bool sendUdpTo(int sockFd, const std::string& payload, const std::string& ip, std::uint16_t port) {
    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &target.sin_addr) != 1) {
        return false;
    }

    return sendto(
               sockFd,
               payload.data(),
               payload.size(),
               0,
               reinterpret_cast<sockaddr*>(&target),
               sizeof(target)) == static_cast<ssize_t>(payload.size());
}

LocalNetInfo guessLocalNetInfo() {
    LocalNetInfo info{"127.0.0.1", "255.255.255.255"};

    int probe = socket(AF_INET, SOCK_DGRAM, 0);
    if (probe < 0) {
        return info;
    }

    sockaddr_in remote{};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr);

    if (connect(probe, reinterpret_cast<sockaddr*>(&remote), sizeof(remote)) == 0) {
        sockaddr_in local{};
        socklen_t len = sizeof(local);
        if (getsockname(probe, reinterpret_cast<sockaddr*>(&local), &len) == 0) {
            char ip[INET_ADDRSTRLEN]{};
            if (inet_ntop(AF_INET, &local.sin_addr, ip, sizeof(ip)) != nullptr) {
                info.localIp = ip;
            }
        }
    }
    close(probe);

    ifaddrs* interfaces = nullptr;
    if (getifaddrs(&interfaces) == 0 && interfaces != nullptr) {
        for (ifaddrs* it = interfaces; it != nullptr; it = it->ifa_next) {
            if (it->ifa_addr == nullptr || it->ifa_broadaddr == nullptr) {
                continue;
            }
            if (it->ifa_addr->sa_family != AF_INET) {
                continue;
            }
            if ((it->ifa_flags & IFF_LOOPBACK) != 0) {
                continue;
            }

            auto* addrIn = reinterpret_cast<sockaddr_in*>(it->ifa_addr);
            char ifaceIp[INET_ADDRSTRLEN]{};
            if (inet_ntop(AF_INET, &addrIn->sin_addr, ifaceIp, sizeof(ifaceIp)) == nullptr) {
                continue;
            }
            if (info.localIp != ifaceIp) {
                continue;
            }

            auto* bcastIn = reinterpret_cast<sockaddr_in*>(it->ifa_broadaddr);
            char bcast[INET_ADDRSTRLEN]{};
            if (inet_ntop(AF_INET, &bcastIn->sin_addr, bcast, sizeof(bcast)) != nullptr) {
                info.broadcastIp = bcast;
                break;
            }
        }
        freeifaddrs(interfaces);
    }

    return info;
}

std::string localIpGuess() {
    LocalNetInfo info = guessLocalNetInfo();
    if (info.localIp != "127.0.0.1") {
        return info.localIp;
    }

    char hostName[256]{};
    if (gethostname(hostName, sizeof(hostName)) != 0) {
        return "127.0.0.1";
    }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    addrinfo* results = nullptr;
    if (getaddrinfo(hostName, nullptr, &hints, &results) != 0 || results == nullptr) {
        return "127.0.0.1";
    }

    std::string found = "127.0.0.1";
    for (addrinfo* it = results; it != nullptr; it = it->ai_next) {
        auto* in = reinterpret_cast<sockaddr_in*>(it->ai_addr);
        char buffer[INET_ADDRSTRLEN]{};
        if (inet_ntop(AF_INET, &in->sin_addr, buffer, sizeof(buffer)) != nullptr) {
            std::string candidate(buffer);
            if (candidate != "127.0.0.1") {
                found = candidate;
                break;
            }
            found = candidate;
        }
    }

    freeaddrinfo(results);
    return found;
}

std::string nowIsoUtc() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const std::time_t raw = system_clock::to_time_t(now);

    std::tm utc{};
    gmtime_r(&raw, &utc);

    std::ostringstream out;
    out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

} // namespace net