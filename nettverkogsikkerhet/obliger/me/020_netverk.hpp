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

namespace network {
    enum class MsgType{
    PRESENCE,
    ROOM_ANNOUNCE,
    INVITE,
    CHAT,
    UNKNOWN
    };

    enum Fildindex{
    TYPE = 0,
    ROOM = 1,
    USERNAME = 2,
    PAYLOAD = 3
    };

    extern std::atomic_bool del;

    std::string getMyIp();
    std::string anounceMyIp(bool s);
    std::vector<std::string> parseMessage(const std::string& msg);
    std::vector<std::vector<std::string>> listen(bool onlyPresence, bool b);
    MsgType toMsgType(const std::string& s);
    std::vector<std::string> getActiveUsers();
    void removeInactiveUsers(int maxS = 30);
    bool sendUSNChat(const std::string& room, const std::string& msg);

}
