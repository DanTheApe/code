#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace felles
{
    inline constexpr uint8_t localTtl = 1;
    inline constexpr uint16_t udpPort = 50000;
    inline constexpr uint16_t tcpPort = 50001;

    inline constexpr const char *multicastIp = "239.0.0.1";
    inline constexpr const char *broadcastDefaultIp = "255.255.255.255";
    inline constexpr const char *fallbackLocalIp = "192.168.41.22";

    inline constexpr const char *msgPresence = "PRESENCE";
    inline constexpr const char *msgRoomAnnounce = "ROOM_ANNOUNCE";
    inline constexpr const char *msgInvite = "INVITE";
    inline constexpr const char *msgChat = "CHAT";

    inline constexpr const char *roomUsnChat = "USN Chat";
    inline constexpr const char *payloadOpen = "OPEN";

    extern std::string myUsername;

    std::string getUsername();
    void setUsername(const std::string &username);
    std::string getMyIp();
    std::vector<std::string> parseMessage(const std::string &msg);
}
