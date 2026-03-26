#pragma once

#include <string>
#include <vector>
#include <atomic>

class felles
{
    friend class tcp;
    friend class network;

private:
    static constexpr int localTtl = 1;
    static constexpr int udpPort = 50000;
    static constexpr int tcpPort = 50001;

    static constexpr const char *multicastIp = "239.0.0.1";
    static constexpr const char *broadcastDefaultIp = "255.255.255.255";
    static constexpr const char *fallbackLocalIp = "192.168.41.22";

    static constexpr const char *msgPresence = "PRESENCE";
    static constexpr const char *msgRoomAnnounce = "ROOM_ANNOUNCE";
    static constexpr const char *msgInvite = "INVITE";
    static constexpr const char *msgChat = "CHAT";

    static constexpr const char *roomUsnChat = "USN Chat";
    static constexpr const char *payloadOpen = "OPEN";
    static constexpr const char *payloadClosed = "CLOSED";
    static constexpr const char *payloadTcp = "TCP";
    static constexpr const char *payloadTcpPrivate = "TCP_PRIVATE";

    static std::string myUsername;
    static std::atomic_bool del;

    enum class MsgType
    {
        PRESENCE,
        ROOM_ANNOUNCE,
        INVITE,
        CHAT,
        UNKNOWN
    };

    enum FieldIndex
    {
        TYPE = 0,
        ROOM = 1,
        USERNAME = 2,
        PAYLOAD = 3
    };

public:
    felles() = default;
    ~felles() = default;
    static bool isValidRoomName(const std::string &room);
    static bool isNonEmptyNoPipe(const std::string &value);
    static bool isSendComplete(int bytesSent, size_t expectedSize);
    static std::string getUsername();
    static void setUsername(const std::string &username);
    static std::string getMyIp();
    static std::vector<std::string> parseMessage(const std::string &msg);
    static std::string encryptXor(const std::string &plaintext, const std::string &key);
    static std::string decryptXor(const std::string &ciphertext, const std::string &key);
};