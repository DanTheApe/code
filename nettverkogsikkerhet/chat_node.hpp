#pragma once

#include <atomic>
#include <array>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

struct ActiveUser {
    std::string username;
    std::string ip;
    std::uint64_t lastSeenEpoch{};
};

struct GroupRoomInfo {
    std::string roomName;
    std::string owner;
    std::uint64_t lastAdvertisedEpoch{};
};

class ChatNode {
public:
    static constexpr std::uint16_t kUdpPort = 50000;
    static constexpr const char* kUsnChatRoom = "USN Chat";
    static constexpr const char* kMulticastIp = "239.0.0.1";

    explicit ChatNode(std::string username);
    ~ChatNode();

    ChatNode(const ChatNode&) = delete;
    ChatNode& operator=(const ChatNode&) = delete;

    bool start();
    void stop();

    void sendCommonMessage(const std::string& text);
    void printUsers();
    void printRooms();

    bool createGroupRoom(const std::string& roomName);
    bool joinGroupRoom(const std::string& roomName);
    void leaveGroupRoom(const std::string& roomName);
    void sendGroupMessage(const std::string& roomName, const std::string& text);

    bool inviteOneToOne(const std::string& targetUser);
    void sendDirectMessage(const std::string& targetUser, const std::string& text);

    std::string username() const;
    std::string localIp() const;
    std::string broadcastIp() const;

private:
    std::string username_;
    std::string localIp_;
    std::string broadcastIp_;

    int udpRecvSock_{-1};
    int udpSendSock_{-1};

    std::atomic<bool> running_{false};
    std::thread recvThread_;
    std::thread heartbeatThread_;
    std::thread cleanupThread_;

    mutable std::mutex stateMutex_;
    std::unordered_map<std::string, ActiveUser> usersByName_;
    std::unordered_map<std::string, GroupRoomInfo> roomsByName_;
    std::unordered_map<std::string, GroupRoomInfo> ownedRooms_;
    std::unordered_set<std::string> joinedRooms_;
    std::unordered_map<std::string, std::string> directRoomByPeer_;

    std::unordered_set<std::string> acceptedPeers_;

    void recvLoop();
    void heartbeatLoop();
    void cleanupLoop();

    void handlePacket(const std::string& packet, const std::string& fromIp);

    void sendPresence();
    void advertiseRooms();
    void sendBroadcast(const std::string& packet) const;
    bool sendUnicastToUser(const std::string& targetUser, const std::string& packet);

    static std::uint64_t nowEpochSeconds();
    static bool parseMessage(const std::string& packet, std::array<std::string, 4>& fields);
    static bool validRoomName(const std::string& roomName);

    std::string encodeMessage(
        const std::string& type,
        const std::string& room,
        const std::string& payload) const;

    bool lookupUser(const std::string& user, ActiveUser& outUser);
    bool joinMulticastOnSocket(int sockFd) const;
};