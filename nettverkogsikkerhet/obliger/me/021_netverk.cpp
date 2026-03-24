#include "020_netverk.hpp"
#include "040_felles.hpp"
#include <ifaddrs.h>

namespace
{
    bool applyUdpLocalOnlyTtl(int s)
    {
        int ttl = felles::localTtl;
        if (setsockopt(s, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) < 0)
        {
            perror("setsockopt IP_TTL");
            return false;
        }
        return true;
    }
}

network::network()
    : myIp(felles::getMyIp())
{
}

network::~network()
{
    stop();

    std::lock_guard<std::mutex> lock(announceThreadMutex);
    if (roomAnnounceThread.joinable())
    {
        roomAnnounceThread.join();
    }
}

std::string network::getUsername() const
{
    return felles::getUsername();
}

std::string network::getMyIp() const
{
    return myIp;
}

void network::stop()
{
    del.store(true);
}

bool network::isValidRoomName(const std::string &room) const
{
    return !room.empty() && room.size() <= 32 && room.find('|') == std::string::npos;
}

bool network::sendBroadcastMessage(const std::string &wire) const
{
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
    {
        perror("socket");
        return false;
    }

    int enable = 1;
    if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable)) < 0)
    {
        perror("setsockopt SO_BROADCAST");
        close(s);
        return false;
    }

    if (!applyUdpLocalOnlyTtl(s))
    {
        close(s);
        return false;
    }

    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(felles::udpPort);
    std::string broadcastAddr = getBroadcastAddress();
    if (inet_pton(AF_INET, broadcastAddr.c_str(), &dst.sin_addr) != 1)
    {
        perror("inet_pton");
        close(s);
        return false;
    }

    const ssize_t n = sendto(
        s,
        wire.c_str(),
        wire.size(),
        0,
        reinterpret_cast<sockaddr *>(&dst),
        sizeof(dst));

    if (n < 0 || static_cast<size_t>(n) != wire.size())
    {
        perror("sendto");
        close(s);
        return false;
    }

    close(s);
    return true;
}

void network::roomAnnounceLoop()
{
    while (!del.load())
    {
        std::vector<std::string> roomsToSend;
        {
            std::lock_guard<std::mutex> lock(roomsMutex);
            roomsToSend.assign(ownedOpenRooms.begin(), ownedOpenRooms.end());
        }

        for (const auto &room : roomsToSend)
        {
            const std::string wire = std::string(felles::msgRoomAnnounce) + "|" + room + "|" + getUsername() + "|" + felles::payloadOpen + "\n";
            sendBroadcastMessage(wire);
        }

        sleep(10);
    }
}

void network::ensureRoomAnnounceThreadRunning()
{
    std::lock_guard<std::mutex> lock(announceThreadMutex);
    if (roomAnnounceRunning)
    {
        return;
    }

    roomAnnounceRunning = true;
    roomAnnounceThread = std::thread(&network::roomAnnounceLoop, this);
}

std::string network::getBroadcastAddress() const
{
    struct ifaddrs *ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == -1)
    {
        perror("getifaddrs");
        return felles::broadcastDefaultIp;
    }

    std::string broadcastAddr = felles::broadcastDefaultIp;

    for (struct ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == nullptr)
            continue;

        if (ifa->ifa_addr->sa_family != AF_INET)
            continue;

        struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
        struct sockaddr_in *netmask = (struct sockaddr_in *)ifa->ifa_netmask;

        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr->sin_addr, ipStr, INET_ADDRSTRLEN);

        if (myIp == ipStr)
        {
            uint32_t ip = addr->sin_addr.s_addr;
            uint32_t mask = netmask->sin_addr.s_addr;
            uint32_t broadcast = ip | ~mask;

            struct in_addr broadcastAddr_in;
            broadcastAddr_in.s_addr = broadcast;
            inet_ntop(AF_INET, &broadcastAddr_in, ipStr, INET_ADDRSTRLEN);
            broadcastAddr = ipStr;
            break;
        }
    }

    freeifaddrs(ifaddr);
    return broadcastAddr;
}

in_addr network::getMulticastInterface() const
{
    struct ifaddrs *ifaddr = nullptr;
    in_addr result{};
    result.s_addr = htonl(INADDR_ANY);

    if (getifaddrs(&ifaddr) == -1)
    {
        perror("getifaddrs");
        return result;
    }

    for (struct ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == nullptr)
            continue;

        if (ifa->ifa_addr->sa_family != AF_INET)
            continue;

        struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr->sin_addr, ipStr, INET_ADDRSTRLEN);

        if (myIp == ipStr)
        {
            result = addr->sin_addr;
            break;
        }
    }

    freeifaddrs(ifaddr);
    return result;
}

std::string network::anounceMyIp(bool b)
{
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
    {
        perror("socket");
        return "";
    }

    int enable = 1;
    if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable)) < 0)
    {
        perror("setsockopt SO_BROADCAST");
        close(s);
        return "";
    }

    if (!applyUdpLocalOnlyTtl(s))
    {
        close(s);
        return "";
    }

    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(felles::udpPort);
    std::string broadcastAddr = getBroadcastAddress();
    inet_pton(AF_INET, broadcastAddr.c_str(), &dst.sin_addr);

    std::string msg = std::string(felles::msgPresence) + "|-|" + getUsername() + "|" + getMyIp();
    if (b)
    {
        while (!del.load())
        {
            ssize_t n = sendto(s, msg.c_str(), msg.size(), 0, reinterpret_cast<sockaddr *>(&dst), sizeof(dst));
            if (n < 0)
                perror("sendto");
            sleep(10);
        }
    }

    ssize_t n = sendto(s, msg.c_str(), msg.size(), 0, reinterpret_cast<sockaddr *>(&dst), sizeof(dst));
    if (n < 0)
        perror("sendto");

    close(s);
    return msg;
}

network::MsgType network::toMsgType(const std::string &s) const
{
    if (s == felles::msgPresence)
        return MsgType::PRESENCE;
    if (s == felles::msgRoomAnnounce)
        return MsgType::ROOM_ANNOUNCE;
    if (s == felles::msgInvite)
        return MsgType::INVITE;
    if (s == felles::msgChat)
        return MsgType::CHAT;
    return MsgType::UNKNOWN;
}

std::vector<std::string> network::parseMessage(const std::string &msg) const
{
    return felles::parseMessage(msg);
}

std::vector<std::vector<std::string>> network::listen(bool onlyPresence, bool b)
{
    std::vector<std::vector<std::string>> messages;

    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
    {
        perror("socket");
        return messages;
    }

    int reuse = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        perror("setsockopt SO_REUSEADDR");
        close(s);
        return messages;
    }

    timeval tv{};
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        perror("setsockopt SO_RCVTIMEO");
        close(s);
        return messages;
    }

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(felles::udpPort);
    local.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s, reinterpret_cast<sockaddr *>(&local), sizeof(local)) < 0)
    {
        perror("bind");
        close(s);
        return messages;
    }

    {
        std::lock_guard<std::mutex> lock(listenSocketMutex);
        listenSocketFd = s;
    }

    while (!del.load())
    {
        char buf[2048];
        sockaddr_in src{};
        socklen_t srcLen = sizeof(src);

        ssize_t n = recvfrom(
            s, buf, sizeof(buf) - 1, 0,
            reinterpret_cast<sockaddr *>(&src), &srcLen);

        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                continue;
            }
            perror("recvfrom");
            continue;
        }

        buf[n] = '\0';
        std::vector<std::string> parts = parseMessage(std::string(buf));

        if (parts.size() != 4)
        {
            continue;
        }

        MsgType t = toMsgType(parts[TYPE]);
        if (onlyPresence && t != MsgType::PRESENCE)
        {
            continue;
        }

        if (t == MsgType::PRESENCE)
        {
            const std::string &username = parts[USERNAME];
            const std::string &ip = parts[PAYLOAD];

            {
                std::lock_guard<std::mutex> lock(usersMutex);
                activeUsers[username] = {ip, std::time(nullptr)};
            }
        }

        if (t == MsgType::CHAT && parts[ROOM] == felles::roomUsnChat)
        {
            std::cout << "[" << felles::roomUsnChat << "] " << parts[USERNAME] << ": " << parts[PAYLOAD] << std::endl;
        }

        if (t == MsgType::ROOM_ANNOUNCE && parts[PAYLOAD] == felles::payloadOpen)
        {
            std::cout << "[OPEN ROOM] " << parts[ROOM] << " owner=" << parts[USERNAME] << std::endl;
        }

        if (t == MsgType::CHAT && parts[ROOM] != felles::roomUsnChat)
        {
            bool shouldPrint = false;
            {
                std::lock_guard<std::mutex> lock(roomsMutex);
                shouldPrint = joinedOpenRooms.find(parts[ROOM]) != joinedOpenRooms.end();
            }
            if (shouldPrint)
            {
                std::cout << "[" << parts[ROOM] << "] " << parts[USERNAME] << ": " << parts[PAYLOAD] << std::endl;
            }
        }

        removeInactiveUsers(30);

        messages.push_back(parts);

        if (!b)
        {
            break;
        }
    }

    close(s);

    {
        std::lock_guard<std::mutex> lock(listenSocketMutex);
        if (listenSocketFd == s)
        {
            listenSocketFd = -1;
        }
    }

    return messages;
}

std::vector<std::string> network::getActiveUsers()
{
    std::lock_guard<std::mutex> lock(usersMutex);
    std::vector<std::string> out;
    out.reserve(activeUsers.size());

    for (const auto &entry : activeUsers)
    {
        const std::string &user = entry.first;
        const std::string &ip = entry.second.ip;
        out.push_back(user + "|" + ip);
    }
    return out;
}

void network::removeInactiveUsers(int maxS)
{
    const std::time_t now = std::time(nullptr);
    std::lock_guard<std::mutex> lock(usersMutex);

    for (auto it = activeUsers.begin(); it != activeUsers.end();)
    {
        if (now - it->second.lastSeen > maxS)
        {
            std::cout << "Removing inactive user: " << it->first << std::endl;
            it = activeUsers.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

bool network::sendUSNChat(const std::string &username, const std::string &text)
{
    if (username.empty() || text.empty())
    {
        return false;
    }

    if (username.find('|') != std::string::npos || text.find('|') != std::string::npos)
    {
        return false;
    }

    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
    {
        perror("socket");
        return false;
    }

    int enable = 1;
    if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable)) < 0)
    {
        perror("setsockopt SO_BROADCAST");
        close(s);
        return false;
    }

    if (!applyUdpLocalOnlyTtl(s))
    {
        close(s);
        return false;
    }

    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(felles::udpPort);
    std::string broadcastAddr = getBroadcastAddress();
    if (inet_pton(AF_INET, broadcastAddr.c_str(), &dst.sin_addr) != 1)
    {
        perror("inet_pton");
        close(s);
        return false;
    }

    std::string wire = std::string(felles::msgChat) + "|" + felles::roomUsnChat + "|" + username + "|" + text + "\n";

    ssize_t n = sendto(
        s,
        wire.c_str(),
        wire.size(),
        0,
        reinterpret_cast<sockaddr *>(&dst),
        sizeof(dst));

    if (n < 0 || static_cast<size_t>(n) != wire.size())
    {
        perror("sendto");
        close(s);
        return false;
    }

    close(s);
    return true;
}

bool network::createOpenRoom(const std::string &room)
{
    if (!isValidRoomName(room))
    {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(roomsMutex);
        ownedOpenRooms.insert(room);
    }

    ensureRoomAnnounceThreadRunning();

    const std::string wire = std::string(felles::msgRoomAnnounce) + "|" + room + "|" + getUsername() + "|" + felles::payloadOpen + "\n";
    return sendBroadcastMessage(wire);
}

bool network::joinOpenRoom(const std::string &room)
{
    if (!isValidRoomName(room))
    {
        return false;
    }

    bool firstJoinedRoom = false;
    {
        std::lock_guard<std::mutex> roomLock(roomsMutex);
        if (joinedOpenRooms.find(room) != joinedOpenRooms.end())
        {
            return true;
        }
        firstJoinedRoom = joinedOpenRooms.empty();
        joinedOpenRooms.insert(room);
    }

    if (!firstJoinedRoom)
    {
        return true;
    }

    std::lock_guard<std::mutex> lock(listenSocketMutex);
    if (listenSocketFd < 0)
    {
        std::cerr << "listen socket is not ready\n";
        std::lock_guard<std::mutex> roomLock(roomsMutex);
        joinedOpenRooms.erase(room);
        return false;
    }

    ip_mreq mreq{};
    if (inet_pton(AF_INET, felles::multicastIp, &mreq.imr_multiaddr) != 1)
    {
        perror("inet_pton");
        return false;
    }
    mreq.imr_interface = getMulticastInterface();

    if (setsockopt(listenSocketFd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    {
        perror("setsockopt IP_ADD_MEMBERSHIP");
        std::lock_guard<std::mutex> roomLock(roomsMutex);
        joinedOpenRooms.erase(room);
        return false;
    }

    return true;
}

bool network::leaveOpenRoom(const std::string &room)
{
    if (!isValidRoomName(room))
    {
        return false;
    }

    bool shouldDropMembership = false;
    {
        std::lock_guard<std::mutex> roomLock(roomsMutex);
        const auto it = joinedOpenRooms.find(room);
        if (it == joinedOpenRooms.end())
        {
            return true;
        }
        joinedOpenRooms.erase(it);
        shouldDropMembership = joinedOpenRooms.empty();
    }

    if (!shouldDropMembership)
    {
        return true;
    }

    std::lock_guard<std::mutex> lock(listenSocketMutex);
    if (listenSocketFd < 0)
    {
        return true;
    }

    ip_mreq mreq{};
    if (inet_pton(AF_INET, felles::multicastIp, &mreq.imr_multiaddr) != 1)
    {
        perror("inet_pton");
        return false;
    }
    mreq.imr_interface = getMulticastInterface();

    if (setsockopt(listenSocketFd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    {
        perror("setsockopt IP_DROP_MEMBERSHIP");
        std::lock_guard<std::mutex> roomLock(roomsMutex);
        joinedOpenRooms.insert(room);
        return false;
    }

    return true;
}

bool network::sendOpenRoomMessage(const std::string &room, const std::string &msg)
{
    if (room.empty() || msg.empty())
    {
        return false;
    }

    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
    {
        perror("socket");
        return false;
    }

    unsigned char ttl = felles::localTtl;
    if (setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0)
    {
        perror("setsockopt IP_MULTICAST_TTL");
        close(s);
        return false;
    }

    in_addr mcastIf = getMulticastInterface();
    if (setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, &mcastIf, sizeof(mcastIf)) < 0)
    {
        perror("setsockopt IP_MULTICAST_IF");
        close(s);
        return false;
    }

    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(felles::udpPort);
    if (inet_pton(AF_INET, felles::multicastIp, &dst.sin_addr) != 1)
    {
        perror("inet_pton");
        close(s);
        return false;
    }

    std::string wire = std::string(felles::msgChat) + "|" + room + "|" + getUsername() + "|" + msg + "\n";
    ssize_t n = sendto(
        s,
        wire.c_str(),
        wire.size(),
        0,
        reinterpret_cast<sockaddr *>(&dst),
        sizeof(dst));

    if (n < 0 || static_cast<size_t>(n) != wire.size())
    {
        perror("sendto");
        close(s);
        return false;
    }

    close(s);
    return true;
}
