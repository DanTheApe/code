#include "030_TCP.hpp"
#include "040_felles.hpp"

#include <sys/select.h>

// TYPE|ROOM|USERNAME|PAYLOAD

tcp::tcp(std::string username)
    : username(std::move(username)), myip(felles::getMyIp())
{
}

tcp::~tcp() = default;

void tcp::tcpListen()
{
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0)
    {
        onError("Failed to create TCP socket");
        return;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(felles::tcpPort);

    if (bind(serverSocket, (sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        onError("Failed to bind TCP socket");
        close(serverSocket);
        return;
    }

    if (listen(serverSocket, 5) < 0)
    {
        onError("Failed to listen on TCP socket");
        close(serverSocket);
        return;
    }

    std::cout << "TCP server listening on port " << felles::tcpPort << std::endl;

    while (!felles::del.load())
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(serverSocket, &readfds);

        timeval tv{};
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ready = select(serverSocket + 1, &readfds, nullptr, nullptr, &tv);
        if (ready < 0)
        {
            onError("select() failed for TCP listener");
            continue;
        }
        if (ready == 0)
        {
            continue;
        }

        sockaddr_in clientAddr{};
        socklen_t clientAddrLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (sockaddr *)&clientAddr, &clientAddrLen);
        if (clientSocket < 0)
        {
            onError("Failed to accept TCP connection");
            continue;
        }

        char buffer[1024];
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead > 0)
        {
            buffer[bytesRead] = '\0';
            std::string message(buffer);
            std::vector<std::string> parts = felles::parseMessage(message);
            if (parts.size() >= 4)
            {
                std::string type = parts[0];
                std::string roomName = parts[1];
                std::string sender = parts[2];
                std::string payload = parts[3];
                onMessageReceived(roomName, sender, payload);
            }
        }
        close(clientSocket);
    }

    close(serverSocket);
}

void tcp::startServerRoom(const std::string &roomName)
{
    std::lock_guard<std::mutex> lock(roomSocketsMutex);
    roomSockets.emplace(roomName, -1);
}

void tcp::stopRoom(const std::string &roomName)
{
    std::lock_guard<std::mutex> lock(roomSocketsMutex);
    auto it = roomSockets.find(roomName);
    if (it != roomSockets.end())
    {
        if (it->second >= 0)
        {
            close(it->second);
        }
        roomSockets.erase(it);
    }
}

void tcp::joinRoom(const std::string &roomName, const std::string &ownerIp)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
    {
        onError("Failed to create TCP client socket");
        return;
    }

    int ttl = felles::localTtl;
    if (setsockopt(s, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) < 0)
    {
        onError("Failed to set TCP TTL");
        close(s);
        return;
    }

    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(felles::tcpPort);
    if (inet_pton(AF_INET, ownerIp.c_str(), &dst.sin_addr) != 1)
    {
        onError("Invalid owner IP for TCP room");
        close(s);
        return;
    }

    if (connect(s, reinterpret_cast<sockaddr *>(&dst), sizeof(dst)) < 0)
    {
        onError("Failed to connect to TCP room owner");
        close(s);
        return;
    }

    std::lock_guard<std::mutex> lock(roomSocketsMutex);
    roomSockets[roomName] = s;
}

void tcp::leaveRoom(const std::string &roomName)
{
    stopRoom(roomName);
}

void tcp::sendMessage(const std::string &roomName, const std::string &message)
{
    int s = -1;
    {
        std::lock_guard<std::mutex> lock(roomSocketsMutex);
        auto it = roomSockets.find(roomName);
        if (it != roomSockets.end())
        {
            s = it->second;
        }
    }

    if (s < 0)
    {
        onError("No active TCP socket for room");
        return;
    }

    std::string wire = std::string(felles::msgChat) + "|" + roomName + "|" + username + "|" + message + "\n";
    int n = send(s, wire.c_str(), wire.size(), 0);
    if (!felles::isSendComplete(n, wire.size()))
    {
        onError("Failed to send TCP message");
    }
}

void tcp::onMessageReceived(const std::string &roomName, const std::string &sender, const std::string &message)
{
    std::cout << "[TCP " << roomName << "] " << sender << ": " << message << std::endl;
}

void tcp::onClientConnected(const std::string &roomName, const std::string &clientIp)
{
    std::cout << "[TCP " << roomName << "] client connected: " << clientIp << std::endl;
}

void tcp::onError(const std::string &errorMessage)
{
    std::cerr << "[TCP ERROR] " << errorMessage << std::endl;
}
