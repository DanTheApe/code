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
    std::string tcpport = "50001";
    std::string multicastip = "239.0.0.1";
    std::string udpport = "50000";
    std::string brodcastip = "192.168.1.255";
    std::string myip;
    std::unordered_map<std::string, int> roomSockets; // roomName -> socket
    std::mutex roomSocketsMutex;

    std::string getMyIp();

public:
    // default
    tcp(std::string username) : username(std::move(username)) {myip = getMyIp();};
    ~tcp();

    void startServerRoom(const std::string &roomName);
    void stopRoom(const std::string &roomName);
    void joinRoom(const std::string &roomName, const std::string &ownerIp);
    void leaveRoom(const std::string &roomName);
    void sendMessage(const std::string &roomName, const std::string &message);
    void onMessageReceived(const std::string &roomName, const std::string &sender, const std::string &message);
    void onClientConnected(const std::string &roomName, const std::string &clientIp);
    void onError(const std::string &errorMessage);

};