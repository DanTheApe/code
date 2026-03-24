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

class network {
private:
    struct UserInfo
    {
        std::string ip;
        std::time_t lastSeen;
    };

    std::string myIp;
    std::atomic_bool del{false};

    std::mutex usersMutex;
    std::unordered_map<std::string, UserInfo> activeUsers;

    std::mutex roomsMutex;
    std::set<std::string> joinedOpenRooms;
    std::set<std::string> ownedOpenRooms;

    std::mutex listenSocketMutex;
    int listenSocketFd = -1;

    std::mutex announceThreadMutex;
    std::thread roomAnnounceThread;
    bool roomAnnounceRunning = false;

    bool isValidRoomName(const std::string &room) const;
    bool sendBroadcastMessage(const std::string &wire) const;
    void roomAnnounceLoop();
    void ensureRoomAnnounceThreadRunning();
    
public:
    enum class MsgType{
    PRESENCE,
    ROOM_ANNOUNCE,
    INVITE,
    CHAT,
    UNKNOWN
    };

    enum FieldIndex{
    TYPE = 0,
    ROOM = 1,
    USERNAME = 2,
    PAYLOAD = 3
    };

    network();
    ~network();

    std::string getUsername() const;

    std::string getMyIp() const;
    std::string getBroadcastAddress() const;
    in_addr getMulticastInterface() const;
    std::string anounceMyIp(bool s);
    std::vector<std::string> parseMessage(const std::string& msg) const;
    std::vector<std::vector<std::string>> listen(bool onlyPresence, bool b);
    MsgType toMsgType(const std::string& s) const;
    std::vector<std::string> getActiveUsers();
    void removeInactiveUsers(int maxS = 30);
    bool sendUSNChat(const std::string& username, const std::string& text);

    bool createOpenRoom(const std::string& room);
    bool joinOpenRoom(const std::string& room);
    bool leaveOpenRoom(const std::string& room);
    bool sendOpenRoomMessage(const std::string& room, const std::string& msg);

    void stop();

};
