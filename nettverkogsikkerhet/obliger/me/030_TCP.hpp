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
#include <unordered_set>
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
    std::string myip;
    std::unordered_map<std::string, int> roomSockets; // roomName -> socket
    std::unordered_set<std::string> ownedServerRooms;
    std::unordered_map<std::string, std::vector<int>> roomClients; // roomName -> connected client sockets
    std::unordered_map<int, std::string> socketToRoom;             // client socket -> roomName
    std::vector<std::thread> clientThreads;
    std::unordered_map<std::string, std::thread> roomReceiverThreads; // roomName -> client receiver thread
    std::mutex roomSocketsMutex;

    void handleClientSocket(int clientSocket, sockaddr_in clientAddr);
    void unregisterClientSocket(int clientSocket);
    void relayToRoom(const std::string &roomName, const std::string &wire, int excludeSocket);
    void runRoomReceiver(const std::string &roomName, int socketFd);
    static bool sendAll(int socketFd, const std::string &wire);

public:
    tcp(std::string username);
    ~tcp();

    void tcpListen();
    bool startServerRoom(const std::string &roomName);
    void stopRoom(const std::string &roomName);
    bool joinRoom(const std::string &roomName, const std::string &ownerIp);
    void leaveRoom(const std::string &roomName);
    bool sendMessage(const std::string &roomName, const std::string &message);
    void onMessageReceived(const std::string &roomName, const std::string &sender, const std::string &message);
    void onClientConnected(const std::string &roomName, const std::string &clientIp);
    void onError(const std::string &errorMessage);

};