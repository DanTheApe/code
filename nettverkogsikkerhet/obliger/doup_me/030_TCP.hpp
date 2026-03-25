#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <cstring>
#include <atomic>

#include "040_felles.hpp"

// TYPE|ROOM|USERNAME|PAYLOAD
// PORT 50001
// Guaranteed Rooms (TCP, Invitation via UDP Broadcast)
// INVITE|room-name|owner|TCP_PRIVATE 
// MUST listen on TCP port 50001
// All chat messages over TCP MUST use the same CHAT format: CHAT|room-name|username|message
// CHAT|room-name|username|message
class tcp
{
private:
    std::string type;
    std::string room;
    std::string username;
    std::string payload;
    std::string tcpport = std::to_string(felles::tcpPort);
    std::string multicastip = felles::multicastIp;
    std::string udpport = std::to_string(felles::udpPort);
    std::string brodcastip = felles::broadcastDefaultIp;
    std::string myip;
    std::unordered_map<std::string, int> roomSockets; // roomName -> socket
    std::mutex roomSocketsMutex;

public:
    // default
    tcp(std::string username);
    ~tcp();

    void tcpListen();
    void startServerRoom(const std::string &roomName);
    void stopRoom(const std::string &roomName);
    void joinRoom(const std::string &roomName, const std::string &ownerIp);
    void leaveRoom(const std::string &roomName);
    void sendMessage(const std::string &roomName, const std::string &message);
    void onMessageReceived(const std::string &roomName, const std::string &sender, const std::string &message);
    void onClientConnected(const std::string &roomName, const std::string &clientIp);
    void onError(const std::string &errorMessage);

};