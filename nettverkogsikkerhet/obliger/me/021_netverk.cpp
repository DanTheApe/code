#include "020_netverk.hpp"
#include "040_felles.hpp"
#include <ifaddrs.h>
#include <algorithm>

namespace
{
    bool applyUdpLocalOnlyTtl(int s, int ttl)
    {
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
    felles::del.store(false);
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
    felles::del.store(true);
}

bool network::isValidRoomName(const std::string &room) const
{
    return felles::isValidRoomName(room);
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

    if (!applyUdpLocalOnlyTtl(s, felles::localTtl))
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

    const int n = sendto(
        s,
        wire.c_str(),
        wire.size(),
        0,
        reinterpret_cast<sockaddr *>(&dst),
        sizeof(dst));

    if (!felles::isSendComplete(n, wire.size()))
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
    while (!felles::del.load())
    {
        std::vector<std::string> openRoomsToSend;
        std::vector<std::string> guaranteedRoomsToSend;
        {
            std::lock_guard<std::mutex> lock(roomsMutex);
            openRoomsToSend.assign(ownedOpenRooms.begin(), ownedOpenRooms.end());
        }
        {
            std::lock_guard<std::mutex> lock(guaranteedRoomsMutex);
            guaranteedRoomsToSend.assign(ownedGuaranteedRooms.begin(), ownedGuaranteedRooms.end());
        }

        for (const auto &room : openRoomsToSend)
        {
            const std::string wire = std::string(felles::msgRoomAnnounce) + "|" + room + "|" + getUsername() + "|" + felles::payloadOpen + "\n";
            sendBroadcastMessage(wire);
        }

        for (const auto &room : guaranteedRoomsToSend)
        {
            const std::string wire = std::string(felles::msgInvite) + "|" + room + "|" + getUsername() + "|" + felles::payloadTcp + "\n";
            sendBroadcastMessage(wire);
        }

        for (int i = 0; i < 10 && !felles::del.load(); ++i)
        {
            sleep(1);
        }
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

    if (!applyUdpLocalOnlyTtl(s, felles::localTtl))
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
        while (!felles::del.load())
        {
            int n = sendto(s, msg.c_str(), msg.size(), 0, reinterpret_cast<sockaddr *>(&dst), sizeof(dst));
            if (n < 0)
                perror("sendto");
            for (int i = 0; i < 10 && !felles::del.load(); ++i)
            {
                sleep(1);
            }
        }
    }

    int n = sendto(s, msg.c_str(), msg.size(), 0, reinterpret_cast<sockaddr *>(&dst), sizeof(dst));
    if (n < 0)
        perror("sendto");

    close(s);
    return msg;
}

felles::MsgType network::toMsgType(const std::string &s) const
{
    if (s == felles::msgPresence)
        return felles::MsgType::PRESENCE;
    if (s == felles::msgRoomAnnounce)
        return felles::MsgType::ROOM_ANNOUNCE;
    if (s == felles::msgInvite)
        return felles::MsgType::INVITE;
    if (s == felles::msgChat)
        return felles::MsgType::CHAT;
    return felles::MsgType::UNKNOWN;
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

    while (!felles::del.load())
    {
        char buf[2048];
        sockaddr_in src{};
        socklen_t srcLen = sizeof(src);

        int n = recvfrom(
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

        felles::MsgType t = toMsgType(parts[felles::TYPE]);
        if (onlyPresence && t != felles::MsgType::PRESENCE)
        {
            continue;
        }

        if (t == felles::MsgType::PRESENCE)
        {
            const std::string &username = parts[felles::USERNAME];
            const std::string &ip = parts[felles::PAYLOAD];

            {
                std::lock_guard<std::mutex> lock(usersMutex);
                activeUsers[username] = {ip, std::time(nullptr)};
            }
        }

        if (t == felles::MsgType::INVITE && (parts[felles::PAYLOAD] == felles::payloadTcpPrivate || parts[felles::PAYLOAD] == felles::payloadTcp))
        {
            std::cout << "[INVITE] " << parts[felles::USERNAME] << " invites you to room: " << parts[felles::ROOM] << " (type: " << parts[felles::PAYLOAD] << ")" << std::endl;
            {
                std::lock_guard<std::mutex> lock(inviteMutex);
                pendingInvites.push_back({parts[felles::ROOM], parts[felles::USERNAME]});
            }
            continue;
        }

        if (t == felles::MsgType::CHAT && parts[felles::ROOM] == felles::roomUsnChat)
        {
            std::cout << "[" << felles::roomUsnChat << "] " << parts[felles::USERNAME] << ": " << parts[felles::PAYLOAD] << std::endl;
        }

        if (t == felles::MsgType::ROOM_ANNOUNCE && parts[felles::PAYLOAD] == felles::payloadOpen)
        {
            std::cout << "[OPEN ROOM] " << parts[felles::ROOM] << " owner=" << parts[felles::USERNAME] << std::endl;
        }

        if (t == felles::MsgType::CHAT && parts[felles::ROOM] != felles::roomUsnChat)
        {
            bool shouldPrint = false;
            {
                std::lock_guard<std::mutex> lock(roomsMutex);
                shouldPrint = joinedOpenRooms.find(parts[felles::ROOM]) != joinedOpenRooms.end();
            }
            if (shouldPrint)
            {
                std::cout << "[" << parts[felles::ROOM] << "] " << parts[felles::USERNAME] << ": " << parts[felles::PAYLOAD] << std::endl;
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
    if (!felles::isNonEmptyNoPipe(username) || !felles::isNonEmptyNoPipe(text))
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

    if (!applyUdpLocalOnlyTtl(s, felles::localTtl))
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

    int n = sendto(
        s,
        wire.c_str(),
        wire.size(),
        0,
        reinterpret_cast<sockaddr *>(&dst),
        sizeof(dst));

    if (!felles::isSendComplete(n, wire.size()))
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
    if (!felles::isNonEmptyNoPipe(room) || msg.empty())
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
    int n = sendto(
        s,
        wire.c_str(),
        wire.size(),
        0,
        reinterpret_cast<sockaddr *>(&dst),
        sizeof(dst));

    if (!felles::isSendComplete(n, wire.size()))
    {
        perror("sendto");
        close(s);
        return false;
    }

    close(s);
    return true;
}

bool network::createGuaranteedRoom(const std::string &room)
{
    if (!isValidRoomName(room))
    {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(guaranteedRoomsMutex);
        ownedGuaranteedRooms.insert(room);
        guaranteedRoomOwners[room] = getMyIp();
    }

    // Broadcast invitation (will be repeated by ensureRoomAnnounceThreadRunning)
    const std::string wire = std::string(felles::msgInvite) + "|" + room + "|" + getUsername() + "|" + felles::payloadTcp + "\n";
    return sendBroadcastMessage(wire);
}

bool network::joinGuaranteedRoom(const std::string &room, const std::string &ownerIp)
{
    if (!isValidRoomName(room))
    {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(guaranteedRoomsMutex);
        joinedGuaranteedRooms.insert(room);
        guaranteedRoomOwners[room] = ownerIp;
    }

    return true;
}

bool network::sendGuaranteedRoomMessage(const std::string &room, const std::string &msg)
{
    if (msg.empty())
    {
        return false;
    }

    // Check if we own this room or joined it
    {
        std::lock_guard<std::mutex> lock(guaranteedRoomsMutex);
        auto it = guaranteedRoomOwners.find(room);
        if (it == guaranteedRoomOwners.end())
        {
            return false;
        }
    }

    // Message will be sent via TCP by the tcp class
    // This is a placeholder that validates the room exists
    return true;
}

bool network::leaveGuaranteedRoom(const std::string &room)
{
    if (!isValidRoomName(room))
    {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(guaranteedRoomsMutex);
        joinedGuaranteedRooms.erase(room);
        ownedGuaranteedRooms.erase(room);
        guaranteedRoomOwners.erase(room);
    }

    return true;
}

bool network::sendUnicastMessage(const std::string &wire, const std::string &destIp) const
{
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
    {
        perror("socket");
        return false;
    }

    if (!applyUdpLocalOnlyTtl(s, felles::localTtl))
    {
        close(s);
        return false;
    }

    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(felles::udpPort);
    if (inet_pton(AF_INET, destIp.c_str(), &dst.sin_addr) != 1)
    {
        perror("inet_pton");
        close(s);
        return false;
    }

    int n = sendto(
        s,
        wire.c_str(),
        wire.size(),
        0,
        reinterpret_cast<sockaddr *>(&dst),
        sizeof(dst));

    if (!felles::isSendComplete(n, wire.size()))
    {
        perror("sendto");
        close(s);
        return false;
    }

    close(s);
    return true;
}

bool network::createPrivateRoom(const std::string &room, const std::string &invitedUser)
{
    if (!isValidRoomName(room))
    {
        return false;
    }

    std::string inviteeIp;
    {
        std::lock_guard<std::mutex> lock(usersMutex);
        for (const auto &entry : activeUsers)
        {
            if (entry.first == invitedUser)
            {
                inviteeIp = entry.second.ip;
                break;
            }
        }
    }

    if (inviteeIp.empty())
    {
        std::cerr << "User " << invitedUser << " not found or offline.\n";
        return false;
    }

    std::string encKey = room;
    if (encKey.length() < 8)
    {
        encKey += "|" + invitedUser;
    }

    {
        std::lock_guard<std::mutex> lock(privateRoomsMutex);
        ownedPrivateRooms.insert(room);
        privateRoomKeys[room] = encKey;
    }

    std::string wire = std::string(felles::msgInvite) + "|" + room + "|" + getUsername() + "|" + felles::payloadTcpPrivate + "\n";
    return sendUnicastMessage(wire, inviteeIp);
}

bool network::acceptPrivateInvite(const std::string &room, const std::string &ownerIp)
{
    if (!isValidRoomName(room))
    {
        return false;
    }

    std::string encKey = room;
    if (encKey.length() < 8)
    {

        encKey += "|";
    }

    {
        std::lock_guard<std::mutex> lock(privateRoomsMutex);
        joinedPrivateRooms.insert(room);
        privateRoomKeys[room] = encKey;
    }

    {
        std::lock_guard<std::mutex> lock(inviteMutex);
        pendingInvites.erase(
            std::remove_if(pendingInvites.begin(), pendingInvites.end(),
                           [&room](const auto &p) { return p.first == room; }),
            pendingInvites.end());
    }

    return true;
}

bool network::sendPrivateRoomMessage(const std::string &room, const std::string &msg)
{
    if (msg.empty())
    {
        return false;
    }

    std::string encKey;
    {
        std::lock_guard<std::mutex> lock(privateRoomsMutex);
        auto it = privateRoomKeys.find(room);
        if (it == privateRoomKeys.end())
        {
            return false;
        }
        encKey = it->second;
    }

    std::string wire = std::string(felles::msgChat) + "|" + room + "|" + getUsername() + "|" + msg;
    std::string encrypted = felles::encryptXor(wire, encKey);
    encrypted += "\n";

    return true;
}

bool network::leavePrivateRoom(const std::string &room)
{
    if (!isValidRoomName(room))
    {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(privateRoomsMutex);
        joinedPrivateRooms.erase(room);
        ownedPrivateRooms.erase(room);
        privateRoomKeys.erase(room);
    }

    return true;
}

std::vector<std::pair<std::string, std::string>> network::getPendingInvites()
{
    std::lock_guard<std::mutex> lock(inviteMutex);
    return pendingInvites;
}

void network::clearInvite(const std::string &room)
{
    std::lock_guard<std::mutex> lock(inviteMutex);
    pendingInvites.erase(
        std::remove_if(pendingInvites.begin(), pendingInvites.end(),
                       [&room](const auto &p) { return p.first == room; }),
        pendingInvites.end());
}
