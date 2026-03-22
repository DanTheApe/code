#include "020_netverk.hpp"

#include <unordered_map>
#include <mutex>
#include <ctime>

std::atomic_bool network::del{false};

namespace
{ // maKES IT PRIVATE FOR THIS .CCP.

    struct UserInfo
    {
        std::string ip;
        std::time_t lastSeen;
    };

    std::mutex usersMutex;
    std::unordered_map<std::string, UserInfo> activeUsers;
}

std::string network::getMyIp()
{
    int s = socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in remote{};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr);

    connect(s, (sockaddr *)&remote, sizeof(remote));

    sockaddr_in local{};
    socklen_t len = sizeof(local);
    getsockname(s, (sockaddr *)&local, &len);

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &local.sin_addr, ip, sizeof(ip));
    std::string mrip(ip);

    if (mrip == "0.0.0.0")
    {
        mrip = "192.168.41.22";
    } // IF it don't fined my ip I just hardcode it. Problem on IRI-LAb

    close(s);
    return mrip;
}

std::string network::anounceMyIp(bool b)
{
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
    {
        perror("socket");
        return "";
    } // AI

    int enable = 1;
    if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable)) < 0)
    { // AI
        perror("setsockopt SO_BROADCAST");
        close(s);
        return "";
    }

    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(50000);
    inet_pton(AF_INET, "192.168.41.255", &dst.sin_addr);
    // PRESENCE|-|username|ip-address
    std::string msg = "PRESENCE|-|BIG|" + getMyIp();
    if (b)
    {
        while (!network::del.load()) // AI sead .load() is safer. Had it as !network::del before.
        {
            ssize_t n = sendto(s, msg.c_str(), msg.size(), 0, reinterpret_cast<sockaddr *>(&dst), sizeof(dst));
            if (n < 0)
                perror("sendto"); // AI
            sleep(10);
        }
    }
    ssize_t n = sendto(s, msg.c_str(), msg.size(), 0, reinterpret_cast<sockaddr *>(&dst), sizeof(dst));
    if (n < 0)
        perror("sendto"); // AI
    close(s);
    return msg;
    // AI brukt til å finne feil. Hadde satt inet_pton til 255.255.255.255 som ikke fungerete, men 192.168.41.255 fungrete.
}

network::MsgType network::toMsgType(const std::string &s)
{
    if (s == "PRESENCE")
        return MsgType::PRESENCE;
    if (s == "ROOM_ANNOUNCE")
        return MsgType::ROOM_ANNOUNCE;
    if (s == "INVITE")
        return MsgType::INVITE;
    if (s == "CHAT")
        return MsgType::CHAT;
    return MsgType::UNKNOWN;
}

std::vector<std::string> network::parseMessage(const std::string &msg)
{
    std::string clean = msg;
    while (!clean.empty() && (clean.back() == '\n' || clean.back() == '\r'))
    {
        clean.pop_back();
    }

    std::vector<std::string> parts;
    int start = 0;
    int pos = clean.find('|'); // Using what teatcher showed.

    while (pos != std::string::npos)
    {
        parts.push_back(clean.substr(start, pos - start));
        start = pos + 1;
        pos = clean.find('|', start);
    }
    parts.push_back(clean.substr(start));

    if (parts.size() != 4) // if not 4 just throw it.
    {
        return {};
    }
    return parts;
}

std::vector<std::vector<std::string>> network::listen(bool onlyPresence, bool b)
{
    std::vector<std::vector<std::string>> messages;

    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) // Reused what AI showed me. Incase of error.
    {
        perror("socket");
        return messages;
    }

    int reuse = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) // same here.
    {
        perror("setsockopt SO_REUSEADDR");
        close(s);
        return messages;
    }

    timeval tv{};
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) // same here.
    {
        perror("setsockopt SO_RCVTIMEO");
        close(s);
        return messages;
    }

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(50000);
    local.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s, reinterpret_cast<sockaddr *>(&local), sizeof(local)) < 0) // same here.
    {
        perror("bind");
        close(s);
        return messages;
    }

    while (!network::del.load())
    {
        char buf[2048]; // Just put a limit. AI sead it was safer.
        sockaddr_in src{};
        socklen_t srcLen = sizeof(src);

        ssize_t n = recvfrom(
            s, buf, sizeof(buf) - 1, 0,
            reinterpret_cast<sockaddr *>(&src), &srcLen);

        if (n < 0) // same here.
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

        /* // For testing.
        for (const auto &part : parts)
        {
            std::cout << "Part: " << part << std::endl;
        }
        */

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

            std::cout << "PRESENCE user=" << username << " ip=" << ip << std::endl;
        }

        if (t == MsgType::CHAT && parts[ROOM] == "USN Chat")
        {
            std::cout << "[USN Chat] " << parts[USERNAME] << ": " << parts[PAYLOAD] << std::endl;
        }

        removeInactiveUsers(30);

        messages.push_back(parts);

        if (t == MsgType::PRESENCE)
        {
            std::cout
                << "PRESENCE user=" << parts[USERNAME]
                << " ip=" << parts[PAYLOAD] << std::endl;
        }
        if (!b)
        {
            break;
        }
    }

    close(s);
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

    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(50000);
    if (inet_pton(AF_INET, "255.255.255.255", &dst.sin_addr) != 1)
    {
        perror("inet_pton");
        close(s);
        return false;
    }

    std::string wire = "CHAT|USN Chat|" + username + "|" + text + "\n";

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
