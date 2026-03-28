#pragma once

#include <iostream>
#include <string>
#include <atomic> // AI sugestion for thread safety
#include <vector>

#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <thread>
#include <chrono>

#include <unordered_map>
#include <mutex>
#include <ctime>
#include <set>

#include <cerrno>

#include "040_felles.hpp"

class tcp;  // Forward declaration

class network {
private:
    struct UserInfo
    {
        std::string ip;
        std::time_t lastSeen;
    };

    std::string myIp;

    std::mutex usersMutex;
    std::unordered_map<std::string, UserInfo> activeUsers;

    std::mutex roomsMutex;
    std::set<std::string> joinedOpenRooms;
    std::set<std::string> ownedOpenRooms;

    std::mutex guaranteedRoomsMutex;
    std::set<std::string> ownedGuaranteedRooms;
    std::set<std::string> joinedGuaranteedRooms;
    std::unordered_map<std::string, std::string> guaranteedRoomOwners; // roomName -> ownerIp

    std::mutex privateRoomsMutex;
    std::set<std::string> ownedPrivateRooms;
    std::set<std::string> joinedPrivateRooms;
    std::unordered_map<std::string, std::string> privateRoomKeys; // roomName -> encryption key

    std::mutex inviteMutex;
    std::vector<std::pair<std::string, std::string>> pendingInvites; // room,inviter

    std::mutex listenSocketMutex;
    int listenSocketFd = -1;

    std::mutex announceThreadMutex;
    std::thread roomAnnounceThread;
    bool roomAnnounceRunning = false;

    tcp* tcpObj = nullptr;

    bool isValidRoomName(const std::string &room) const;
    bool sendBroadcastMessage(const std::string &wire) const;
    bool sendUnicastMessage(const std::string &wire, const std::string &destIp) const;
    void roomAnnounceLoop();
    void ensureRoomAnnounceThreadRunning();
    felles::MsgType toMsgType(const std::string& s) const;
    
public:
    network();
    ~network();

    std::string getUsername() const;

    std::string getMyIp() const;
    std::string getBroadcastAddress() const;
    in_addr getMulticastInterface() const;
    std::string anounceMyIp(bool s);
    std::vector<std::string> parseMessage(const std::string& msg) const;
    std::vector<std::vector<std::string>> listen(bool onlyPresence, bool b);
    std::vector<std::string> getActiveUsers();
    void removeInactiveUsers(int maxS = 30);
    bool sendUSNChat(const std::string& username, const std::string& text);

    bool createOpenRoom(const std::string& room);
    bool joinOpenRoom(const std::string& room);
    bool leaveOpenRoom(const std::string& room);
    bool sendOpenRoomMessage(const std::string& room, const std::string& msg);

    bool createGuaranteedRoom(const std::string& room);
    bool joinGuaranteedRoom(const std::string& room, const std::string& ownerIp);
    bool sendGuaranteedRoomMessage(const std::string& room, const std::string& msg);
    bool leaveGuaranteedRoom(const std::string& room);

    bool createPrivateRoom(const std::string& room, const std::string& invitedUser);
    bool acceptPrivateInvite(const std::string& room, const std::string& ownerIp);
    bool sendPrivateRoomMessage(const std::string& room, const std::string& msg);
    bool leavePrivateRoom(const std::string& room);
    std::vector<std::pair<std::string, std::string>> getPendingInvites();
    void clearInvite(const std::string& room);

    void setTcpObject(tcp* obj) { tcpObj = obj; }

    void stop();

};
