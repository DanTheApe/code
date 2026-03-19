#include "chat_node.hpp"

#include "network_utils.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

constexpr std::size_t kMaxDatagramSize = 1400;
constexpr std::size_t kMaxUserInput = 900;

void safeClose(int& fd) {
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
}

std::string safeText(const std::string& value) {
    std::string out = value;
    out.erase(std::remove(out.begin(), out.end(), '\n'), out.end());
    out.erase(std::remove(out.begin(), out.end(), '\r'), out.end());
    return out;
}

std::string trimNewline(std::string value) {
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
        value.pop_back();
    }
    return value;
}

} // namespace

ChatNode::ChatNode(std::string username)
    : username_(std::move(username)) {
    const auto netInfo = net::guessLocalNetInfo();
    localIp_ = netInfo.localIp;
    broadcastIp_ = netInfo.broadcastIp;
}

ChatNode::~ChatNode() {
    stop();
}

bool ChatNode::start() {
    if (running_.load()) {
        return true;
    }

    udpRecvSock_ = net::createUdpSocket(true);
    udpSendSock_ = net::createUdpSocket(true);
    if (udpRecvSock_ < 0 || udpSendSock_ < 0) {
        std::cerr << "[ERR] Failed to create sockets\n";
        stop();
        return false;
    }

    if (!net::setBroadcastEnabled(udpSendSock_)) {
        std::cerr << "[ERR] Failed to enable broadcast mode\n";
        stop();
        return false;
    }
    net::setMulticastTtl(udpSendSock_, 1);

    if (!net::bindUdpSocket(udpRecvSock_, kUdpPort, true)) {
        std::cerr << "[ERR] Failed to bind UDP receive socket on port " << kUdpPort << "\n";
        stop();
        return false;
    }

    if (!joinMulticastOnSocket(udpRecvSock_)) {
        std::cerr << "[ERR] Failed to join multicast group " << kMulticastIp << "\n";
        stop();
        return false;
    }

    net::setReadTimeoutMs(udpRecvSock_, 1000);

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        usersByName_[username_] = ActiveUser{username_, localIp_, nowEpochSeconds()};
    }

    running_.store(true);
    recvThread_ = std::thread(&ChatNode::recvLoop, this);
    heartbeatThread_ = std::thread(&ChatNode::heartbeatLoop, this);
    cleanupThread_ = std::thread(&ChatNode::cleanupLoop, this);

    sendPresence();
    advertiseRooms();
    return true;
}

void ChatNode::stop() {
    const bool wasRunning = running_.exchange(false);
    if (!wasRunning) {
        return;
    }

    if (recvThread_.joinable()) {
        recvThread_.join();
    }
    if (heartbeatThread_.joinable()) {
        heartbeatThread_.join();
    }
    if (cleanupThread_.joinable()) {
        cleanupThread_.join();
    }

    safeClose(udpRecvSock_);
    safeClose(udpSendSock_);
}

void ChatNode::sendCommonMessage(const std::string& text) {
    std::string clean = safeText(text);
    if (clean.empty()) {
        return;
    }
    if (clean.size() > kMaxUserInput) {
        std::cerr << "[WARN] Message too long\n";
        return;
    }

    sendBroadcast(encodeMessage("CHAT", kUsnChatRoom, clean));
}

void ChatNode::printUsers() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    std::cout << "---- Active users ----\n";
    for (const auto& entry : usersByName_) {
        const auto& user = entry.second;
        std::cout << "- " << user.username << " @ " << user.ip;
        if (user.username == username_) {
            std::cout << " (me)";
        }
        std::cout << "\n";
    }
}

void ChatNode::printRooms() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    std::cout << "---- Open group rooms ----\n";
    for (const auto& entry : roomsByName_) {
        const auto& room = entry.second;
        std::cout << "- " << room.roomName << " by " << room.owner;
        if (joinedRooms_.count(room.roomName) > 0) {
            std::cout << " (joined)";
        }
        std::cout << "\n";
    }
}

bool ChatNode::createGroupRoom(const std::string& roomName) {
    if (!validRoomName(roomName) || roomName == kUsnChatRoom) {
        std::cerr << "[WARN] Invalid room name\n";
        return false;
    }

    GroupRoomInfo info;
    info.roomName = roomName;
    info.owner = username_;
    info.lastAdvertisedEpoch = nowEpochSeconds();

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        ownedRooms_[roomName] = info;
        roomsByName_[roomName] = info;
    }

    advertiseRooms();
    return joinGroupRoom(roomName);
}

bool ChatNode::joinGroupRoom(const std::string& roomName) {
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        auto it = roomsByName_.find(roomName);
        if (it == roomsByName_.end()) {
            return false;
        }
        if (joinedRooms_.count(roomName) > 0) {
            return true;
        }
        joinedRooms_.insert(roomName);
    }

    std::cout << "[group] Joined room: " << roomName << "\n";
    return true;
}

void ChatNode::leaveGroupRoom(const std::string& roomName) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (joinedRooms_.erase(roomName) > 0) {
        std::cout << "[group] Left room: " << roomName << "\n";
    }
}

void ChatNode::sendGroupMessage(const std::string& roomName, const std::string& text) {
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (joinedRooms_.count(roomName) == 0) {
            std::cerr << "[WARN] Not joined in room: " << roomName << "\n";
            return;
        }
    }

    std::string clean = safeText(text);
    if (clean.empty() || clean.size() > kMaxUserInput) {
        std::cerr << "[WARN] Invalid message length\n";
        return;
    }

    if (!net::sendUdpTo(udpSendSock_, encodeMessage("CHAT", roomName, clean), kMulticastIp, kUdpPort)) {
        std::cerr << "[WARN] Failed sending group message\n";
    }
}

bool ChatNode::inviteOneToOne(const std::string& targetUser) {
    ActiveUser user;
    if (!lookupUser(targetUser, user)) {
        std::cerr << "[WARN] Unknown user: " << targetUser << "\n";
        return false;
    }

    const std::string roomName = "1to1-" + username_ + "-" + targetUser;
    const std::string payload = "CLOSED;to=" + targetUser;
    if (!net::sendUdpTo(udpSendSock_, encodeMessage("INVITE", roomName, payload), user.ip, kUdpPort)) {
        std::cerr << "[WARN] Failed sending invitation\n";
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        acceptedPeers_.insert(targetUser);
        directRoomByPeer_[targetUser] = roomName;
    }
    return true;
}

void ChatNode::sendDirectMessage(const std::string& targetUser, const std::string& text) {
    ActiveUser user;
    if (!lookupUser(targetUser, user)) {
        std::cerr << "[WARN] Unknown user: " << targetUser << "\n";
        return;
    }

    std::string roomName;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (acceptedPeers_.count(targetUser) == 0 || directRoomByPeer_.count(targetUser) == 0) {
            std::cerr << "[WARN] Invite user first using /invite " << targetUser << "\n";
            return;
        }
        roomName = directRoomByPeer_[targetUser];
    }

    std::string clean = safeText(text);
    if (clean.empty() || clean.size() > kMaxUserInput) {
        std::cerr << "[WARN] Invalid message length\n";
        return;
    }

    if (!net::sendUdpTo(udpSendSock_, encodeMessage("CHAT", roomName, clean), user.ip, kUdpPort)) {
        std::cerr << "[WARN] Failed sending direct message\n";
    }
}

std::string ChatNode::username() const {
    return username_;
}

std::string ChatNode::localIp() const {
    return localIp_;
}

std::string ChatNode::broadcastIp() const {
    return broadcastIp_;
}

void ChatNode::recvLoop() {
    std::vector<char> buffer(kMaxDatagramSize);

    while (running_.load()) {
        sockaddr_in fromAddr{};
        socklen_t fromSize = sizeof(fromAddr);
        const ssize_t received = recvfrom(
            udpRecvSock_,
            buffer.data(),
            buffer.size(),
            0,
            reinterpret_cast<sockaddr*>(&fromAddr),
            &fromSize);

        if (received <= 0) {
            continue;
        }

        char fromIpRaw[INET_ADDRSTRLEN]{};
        std::string fromIp = "0.0.0.0";
        if (inet_ntop(AF_INET, &fromAddr.sin_addr, fromIpRaw, sizeof(fromIpRaw)) != nullptr) {
            fromIp = fromIpRaw;
        }

        const std::string packet = trimNewline(std::string(buffer.data(), static_cast<std::size_t>(received)));
        handlePacket(packet, fromIp);
    }
}

void ChatNode::heartbeatLoop() {
    while (running_.load()) {
        sendPresence();
        advertiseRooms();
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}

void ChatNode::cleanupLoop() {
    while (running_.load()) {
        const auto now = nowEpochSeconds();

        std::lock_guard<std::mutex> lock(stateMutex_);
        for (auto it = usersByName_.begin(); it != usersByName_.end();) {
            if (it->first == username_) {
                ++it;
                continue;
            }
            if (now > it->second.lastSeenEpoch + 25) {
                acceptedPeers_.erase(it->first);
                directRoomByPeer_.erase(it->first);
                it = usersByName_.erase(it);
            } else {
                ++it;
            }
        }

        for (auto it = roomsByName_.begin(); it != roomsByName_.end();) {
            if (ownedRooms_.count(it->first) > 0) {
                ++it;
                continue;
            }
            if (now > it->second.lastAdvertisedEpoch + 30) {
                joinedRooms_.erase(it->first);
                it = roomsByName_.erase(it);
            } else {
                ++it;
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}

void ChatNode::handlePacket(const std::string& packet, const std::string& fromIp) {
    std::array<std::string, 4> parts;
    if (!parseMessage(packet, parts)) {
        return;
    }

    const std::string& type = parts[0];
    const std::string& room = parts[1];
    const std::string& sender = parts[2];
    const std::string& payload = parts[3];

    if (type == "PRESENCE") {
        if (sender.empty() || sender == username_ || room != "-") {
            return;
        }

        std::string advertisedIp = payload.empty() ? fromIp : payload;
        std::lock_guard<std::mutex> lock(stateMutex_);
        usersByName_[sender] = ActiveUser{sender, advertisedIp, nowEpochSeconds()};
        return;
    }

    if (type == "ROOM_ANNOUNCE") {
        if (payload != "OPEN" || !validRoomName(room)) {
            return;
        }

        GroupRoomInfo roomInfo;
        roomInfo.owner = sender;
        roomInfo.roomName = room;
        roomInfo.lastAdvertisedEpoch = nowEpochSeconds();

        std::lock_guard<std::mutex> lock(stateMutex_);
        roomsByName_[roomInfo.roomName] = roomInfo;
        return;
    }

    if (type == "INVITE") {
        const std::string token = ";to=";
        const auto pos = payload.find(token);
        if (pos == std::string::npos) {
            return;
        }

        const std::string roomType = payload.substr(0, pos);
        const std::string invitee = payload.substr(pos + token.size());
        if (invitee != username_ || roomType != "CLOSED" || !validRoomName(room)) {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            acceptedPeers_.insert(sender);
            directRoomByPeer_[sender] = room;
        }

        std::cout << "[1-1] Invitation received from " << sender << " for room '" << room << "'\n";
        return;
    }

    if (type == "CHAT") {
        if (sender == username_) {
            return;
        }

        if (room == kUsnChatRoom) {
            std::cout << "[common][" << sender << "] " << payload << "\n";
            return;
        }

        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            if (joinedRooms_.count(room) > 0) {
                std::cout << "[group:" << room << "][" << sender << "] " << payload << "\n";
                return;
            }
            auto directIt = directRoomByPeer_.find(sender);
            if (directIt != directRoomByPeer_.end() && directIt->second == room && acceptedPeers_.count(sender) > 0) {
                std::cout << "[1-1][" << sender << "] " << payload << "\n";
                return;
            }
        }
    }

    (void)fromIp;
}

void ChatNode::sendPresence() {
    sendBroadcast(encodeMessage("PRESENCE", "-", localIp_));
}

void ChatNode::advertiseRooms() {
    std::vector<GroupRoomInfo> mine;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        for (const auto& entry : ownedRooms_) {
            mine.push_back(entry.second);
        }
    }

    for (const auto& room : mine) {
        sendBroadcast(encodeMessage("ROOM_ANNOUNCE", room.roomName, "OPEN"));
    }
}

void ChatNode::sendBroadcast(const std::string& packet) const {
    bool sent = net::sendUdpTo(udpSendSock_, packet, broadcastIp_, kUdpPort);
    if (broadcastIp_ != "255.255.255.255") {
        sent = net::sendUdpTo(udpSendSock_, packet, "255.255.255.255", kUdpPort) || sent;
    }
    if (!sent) {
        std::cerr << "[WARN] Failed sending broadcast packet\n";
    }
}

bool ChatNode::sendUnicastToUser(const std::string& targetUser, const std::string& packet) {
    ActiveUser user;
    if (!lookupUser(targetUser, user)) {
        return false;
    }
    return net::sendUdpTo(udpSendSock_, packet, user.ip, kUdpPort);
}

std::uint64_t ChatNode::nowEpochSeconds() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}

bool ChatNode::parseMessage(const std::string& packet, std::array<std::string, 4>& fields) {
    std::size_t start = 0;
    for (std::size_t index = 0; index < 3; ++index) {
        const std::size_t sep = packet.find('|', start);
        if (sep == std::string::npos) {
            return false;
        }
        fields[index] = packet.substr(start, sep - start);
        start = sep + 1;
    }
    fields[3] = packet.substr(start);
    return !fields[0].empty() && !fields[2].empty();
}

bool ChatNode::validRoomName(const std::string& roomName) {
    if (roomName.empty() || roomName.size() > 32) {
        return false;
    }
    return roomName.find('|') == std::string::npos;
}

std::string ChatNode::encodeMessage(
    const std::string& type,
    const std::string& room,
    const std::string& payload) const {
    return type + "|" + room + "|" + username_ + "|" + payload + "\n";
}

bool ChatNode::lookupUser(const std::string& user, ActiveUser& outUser) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    auto it = usersByName_.find(user);
    if (it == usersByName_.end()) {
        return false;
    }
    outUser = it->second;
    return true;
}

bool ChatNode::joinMulticastOnSocket(int sockFd) const {
    ip_mreq mreq{};
    if (inet_pton(AF_INET, kMulticastIp, &mreq.imr_multiaddr) != 1) {
        return false;
    }
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    return setsockopt(sockFd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) == 0;
}
