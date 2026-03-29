#include "030_TCP.hpp"
#include "040_felles.hpp"

#include <sys/select.h>
#include <algorithm>
#include <cerrno>

// TYPE|ROOM|USERNAME|PAYLOAD

tcp::tcp(std::string username)
    : username(std::move(username)), myip(felles::getMyIp())
{
}

tcp::~tcp()
{
    std::unordered_map<std::string, int> socketsToClose;
    std::unordered_map<std::string, std::thread> receiverThreadsToJoin;
    std::vector<std::thread> threadsToJoin;
    {
        std::lock_guard<std::mutex> lock(roomSocketsMutex);
        socketsToClose = roomSockets;
        receiverThreadsToJoin.swap(roomReceiverThreads);
        threadsToJoin.swap(clientThreads);
    }

    for (const auto &entry : socketsToClose)
    {
        if (entry.second >= 0)
        {
            shutdown(entry.second, SHUT_RDWR);
            close(entry.second);
        }
    }

    for (auto &entry : receiverThreadsToJoin)
    {
        if (entry.second.joinable())
        {
            entry.second.join();
        }
    }

    for (auto &t : threadsToJoin)
    {
        if (t.joinable())
        {
            t.join();
        }
    }
}

bool tcp::sendAll(int socketFd, const std::string &wire)
{
    size_t total = 0;
    while (total < wire.size())
    {
    int flags = 0;
#ifdef MSG_NOSIGNAL
    flags |= MSG_NOSIGNAL;
#endif
    int n = send(socketFd, wire.c_str() + total, wire.size() - total, flags);
        if (n <= 0)
        {
            return false;
        }
        total += static_cast<size_t>(n);
    }
    return true;
}

void tcp::runRoomReceiver(const std::string &roomName, int socketFd)
{
    timeval tv{};
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(socketFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    std::string pending;
    char buffer[2048];
    while (!felles::del.load())
    {
        int bytesRead = recv(socketFd, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                continue;
            }
            break;
        }
        if (bytesRead == 0)
        {
            break;
        }

        buffer[bytesRead] = '\0';
        pending.append(buffer, bytesRead);

        size_t pos = std::string::npos;
        while ((pos = pending.find('\n')) != std::string::npos)
        {
            std::string line = pending.substr(0, pos);
            pending.erase(0, pos + 1);

            std::vector<std::string> parts = felles::parseMessage(line);
            if (parts.size() != 4)
            {
                continue;
            }

            if (parts[felles::TYPE] != felles::msgChat)
            {
                continue;
            }

            if (parts[felles::ROOM] != roomName)
            {
                continue;
            }

            onMessageReceived(parts[felles::ROOM], parts[felles::USERNAME], parts[felles::PAYLOAD]);
        }
    }

    {
        std::lock_guard<std::mutex> lock(roomSocketsMutex);
        auto it = roomSockets.find(roomName);
        if (it != roomSockets.end() && it->second == socketFd)
        {
            roomSockets.erase(it);
        }
    }
    shutdown(socketFd, SHUT_RDWR);
    close(socketFd);
}

void tcp::unregisterClientSocket(int clientSocket)
{
    std::lock_guard<std::mutex> lock(roomSocketsMutex);
    auto roomIt = socketToRoom.find(clientSocket);
    if (roomIt == socketToRoom.end())
    {
        return;
    }

    auto clientsIt = roomClients.find(roomIt->second);
    if (clientsIt != roomClients.end())
    {
        auto &clients = clientsIt->second;
        clients.erase(std::remove(clients.begin(), clients.end(), clientSocket), clients.end());
    }
    socketToRoom.erase(roomIt);
}

void tcp::relayToRoom(const std::string &roomName, const std::string &wire, int excludeSocket)
{
    std::vector<int> targets;
    {
        std::lock_guard<std::mutex> lock(roomSocketsMutex);
        auto it = roomClients.find(roomName);
        if (it != roomClients.end())
        {
            targets = it->second;
        }
    }

    for (int s : targets)
    {
        if (s == excludeSocket)
        {
            continue;
        }

        if (!sendAll(s, wire))
        {
            unregisterClientSocket(s);
            close(s);
        }
    }
}

void tcp::handleClientSocket(int clientSocket, sockaddr_in clientAddr)
{
    timeval tv{};
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char ip[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &clientAddr.sin_addr, ip, sizeof(ip));

    std::string pending;
    char buffer[2048];

    auto registerSocketInRoom = [this, clientSocket](const std::string &roomName)
    {
        std::lock_guard<std::mutex> lock(roomSocketsMutex);
        socketToRoom[clientSocket] = roomName;
        auto &clients = roomClients[roomName];
        if (std::find(clients.begin(), clients.end(), clientSocket) == clients.end())
        {
            clients.push_back(clientSocket);
        }
    };

    while (!felles::del.load())
    {
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                continue;
            }
            break;
        }
        if (bytesRead == 0)
        {
            break;
        }

        buffer[bytesRead] = '\0';
        pending.append(buffer, bytesRead);

        size_t pos = std::string::npos;
        while ((pos = pending.find('\n')) != std::string::npos)
        {
            std::string line = pending.substr(0, pos);
            pending.erase(0, pos + 1);

            std::vector<std::string> parts = felles::parseMessage(line);
            if (parts.size() != 4)
            {
                continue;
            }

            const std::string &msgType = parts[felles::TYPE];
            const std::string &roomName = parts[felles::ROOM];
            const std::string &sender = parts[felles::USERNAME];
            const std::string &msgPayload = parts[felles::PAYLOAD];

            if (msgType == "JOIN")
            {
                registerSocketInRoom(roomName);
                onClientConnected(roomName, ip);
                continue;
            }

            if (msgType == felles::msgChat)
            {
                std::string targetRoom;
                {
                    std::lock_guard<std::mutex> lock(roomSocketsMutex);
                    auto it = socketToRoom.find(clientSocket);
                    if (it == socketToRoom.end())
                    {
                        targetRoom = roomName;
                    }
                    else
                    {
                        targetRoom = it->second;
                    }
                }

                if (targetRoom == roomName)
                {
                    registerSocketInRoom(roomName);
                }

                if (targetRoom.empty())
                {
                    continue;
                }

                const std::string wire = std::string(felles::msgChat) + "|" + targetRoom + "|" + sender + "|" + msgPayload + "\n";
                onMessageReceived(targetRoom, sender, msgPayload);
                relayToRoom(targetRoom, wire, -1);
            }
        }
    }

    unregisterClientSocket(clientSocket);
    close(clientSocket);
}

void tcp::tcpListen()
{
    while (!felles::del.load())
    {
        int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket < 0)
        {
            onError("Failed to create TCP socket");
            sleep(1);
            continue;
        }

        int reuse = 1;
        if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
        {
            onError("Failed to set TCP SO_REUSEADDR");
            close(serverSocket);
            sleep(1);
            continue;
        }

#ifdef SO_REUSEPORT
        if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0)
        {
            onError("Failed to set TCP SO_REUSEPORT");
        }
#endif

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(felles::tcpPort);

        if (bind(serverSocket, (sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
        {
            onError("Failed to bind TCP socket");
            close(serverSocket);
            sleep(1);
            continue;
        }

        if (listen(serverSocket, 5) < 0)
        {
            onError("Failed to listen on TCP socket");
            close(serverSocket);
            sleep(1);
            continue;
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
                if (errno == EINTR)
                {
                    continue;
                }
                onError("select() failed for TCP listener");
                break;
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

            std::lock_guard<std::mutex> lock(roomSocketsMutex);
            clientThreads.emplace_back(&tcp::handleClientSocket, this, clientSocket, clientAddr);
        }

        close(serverSocket);
    }

    std::vector<std::thread> threadsToJoin;
    {
        std::lock_guard<std::mutex> lock(roomSocketsMutex);
        threadsToJoin.swap(clientThreads);
    }
    for (auto &t : threadsToJoin)
    {
        if (t.joinable())
        {
            t.join();
        }
    }

}

bool tcp::startServerRoom(const std::string &roomName)
{
    std::lock_guard<std::mutex> lock(roomSocketsMutex);
    ownedServerRooms.insert(roomName);
    return true;
}

void tcp::stopRoom(const std::string &roomName)
{
    std::vector<int> socketsToClose;
    std::thread receiverThread;
    {
        std::lock_guard<std::mutex> lock(roomSocketsMutex);

        ownedServerRooms.erase(roomName);

        auto clientsIt = roomClients.find(roomName);
        if (clientsIt != roomClients.end())
        {
            socketsToClose = clientsIt->second;
            for (int s : socketsToClose)
            {
                socketToRoom.erase(s);
            }
            roomClients.erase(clientsIt);
        }

        auto it = roomSockets.find(roomName);
        if (it != roomSockets.end())
        {
            if (it->second >= 0)
            {
                socketsToClose.push_back(it->second);
            }
            roomSockets.erase(it);
        }

        auto recvIt = roomReceiverThreads.find(roomName);
        if (recvIt != roomReceiverThreads.end())
        {
            receiverThread = std::move(recvIt->second);
            roomReceiverThreads.erase(recvIt);
        }
    }

    for (int s : socketsToClose)
    {
        shutdown(s, SHUT_RDWR);
        close(s);
    }

    if (receiverThread.joinable())
    {
        receiverThread.join();
    }
}

bool tcp::joinRoom(const std::string &roomName, const std::string &ownerIp)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
    {
        onError("Failed to create TCP client socket");
        return false;
    }

    int ttl = felles::localTtl;
    if (setsockopt(s, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) < 0)
    {
        onError("Failed to set TCP TTL");
        close(s);
        return false;
    }

    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(felles::tcpPort);
    if (inet_pton(AF_INET, ownerIp.c_str(), &dst.sin_addr) != 1)
    {
        onError("Invalid owner IP for TCP room");
        close(s);
        return false;
    }

    if (connect(s, reinterpret_cast<sockaddr *>(&dst), sizeof(dst)) < 0)
    {
        onError("Failed to connect to TCP room owner");
        close(s);
        return false;
    }

    std::string joinWire = "JOIN|" + roomName + "|" + username + "|-\n";
    if (!sendAll(s, joinWire))
    {
        onError("Failed to send TCP room join handshake");
        close(s);
        return false;
    }

    std::thread oldReceiver;
    {
        std::lock_guard<std::mutex> lock(roomSocketsMutex);
        roomSockets[roomName] = s;
        auto recvIt = roomReceiverThreads.find(roomName);
        if (recvIt != roomReceiverThreads.end())
        {
            oldReceiver = std::move(recvIt->second);
            roomReceiverThreads.erase(recvIt);
        }
        roomReceiverThreads.emplace(roomName, std::thread(&tcp::runRoomReceiver, this, roomName, s));
    }

    if (oldReceiver.joinable())
    {
        oldReceiver.join();
    }
    return true;
}

void tcp::leaveRoom(const std::string &roomName)
{
    stopRoom(roomName);
}

bool tcp::sendMessage(const std::string &roomName, const std::string &message)
{
    int s = -1;
    bool isOwnedServerRoom = false;
    {
        std::lock_guard<std::mutex> lock(roomSocketsMutex);
        auto it = roomSockets.find(roomName);
        if (it != roomSockets.end())
        {
            s = it->second;
        }
        isOwnedServerRoom = ownedServerRooms.find(roomName) != ownedServerRooms.end();
    }

    std::string wire = std::string(felles::msgChat) + "|" + roomName + "|" + username + "|" + message + "\n";

    if (s >= 0)
    {
        if (!sendAll(s, wire))
        {
            onError("Failed to send TCP message");
            close(s);
            std::lock_guard<std::mutex> lock(roomSocketsMutex);
            roomSockets.erase(roomName);
            return false;
        }
        return true;
    }

    if (isOwnedServerRoom)
    {
        onMessageReceived(roomName, username, message);
        relayToRoom(roomName, wire, -1);
        return true;
    }

    onError("No active TCP socket for room");
    return false;
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
