#pragma once

#include <string>
#include <vector>
#include <atomic>

namespace felles
{
    inline constexpr int localTtl = 1;
    inline constexpr int udpPort = 50000;
    inline constexpr int tcpPort = 50001;

    inline constexpr const char *multicastIp = "239.0.0.1";
    inline constexpr const char *broadcastDefaultIp = "255.255.255.255";
    inline constexpr const char *fallbackLocalIp = "192.168.41.22";

    inline constexpr const char *msgPresence = "PRESENCE";
    inline constexpr const char *msgRoomAnnounce = "ROOM_ANNOUNCE";
    inline constexpr const char *msgInvite = "INVITE";
    inline constexpr const char *msgChat = "CHAT";

    inline constexpr const char *roomUsnChat = "USN Chat";
    inline constexpr const char *payloadOpen = "OPEN";
    inline constexpr const char *payloadClosed = "CLOSED";
    inline constexpr const char *payloadTcp = "TCP";
    inline constexpr const char *payloadTcpPrivate = "TCP_PRIVATE";

    extern std::string myUsername;
    extern std::atomic_bool del;

    std::string getUsername();
    void setUsername(const std::string &username);
    std::string getMyIp();
    std::vector<std::string> parseMessage(const std::string &msg);
    std::string encryptXor(const std::string &plaintext, const std::string &key);
    std::string decryptXor(const std::string &ciphertext, const std::string &key);
}
