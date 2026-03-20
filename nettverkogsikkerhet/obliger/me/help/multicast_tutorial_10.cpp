#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

static void printLastError(const char* where) {
    std::cerr << where << " failed, WSAGetLastError() = "
        << WSAGetLastError() << "\n";
}

int main() {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printLastError("WSAStartup");
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        printLastError("socket");
        WSACleanup();
        return 1;
    }

    BOOL reuse = TRUE;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
        reinterpret_cast<const char*>(&reuse), sizeof(reuse)) == SOCKET_ERROR) {
        printLastError("setsockopt(SO_REUSEADDR)");
    }

    // Bind to the multicast port on all local IPv4 addresses.
    sockaddr_in localAddr{};
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(5000);
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, reinterpret_cast<sockaddr*>(&localAddr), sizeof(localAddr)) == SOCKET_ERROR) {
        printLastError("bind");
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // Join the group on the default interface.
    ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = inet_addr("239.255.0.1");
    mreq.imr_interface.s_addr = inet_addr("172.29.70.140"); // Your interface IP adress

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
        reinterpret_cast<const char*>(&mreq), sizeof(mreq)) == SOCKET_ERROR) {
        printLastError("setsockopt(IP_ADD_MEMBERSHIP)");
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // On Windows, loopback control is on the RECEIVING socket.
    DWORD loop = 1;
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP,
        reinterpret_cast<const char*>(&loop), sizeof(loop)) == SOCKET_ERROR) {
        printLastError("setsockopt(IP_MULTICAST_LOOP)");
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // Force sending via a specific local interface.
     in_addr localIf{};
     localIf.s_addr = inet_addr("172.29.70.140");   // Replace with interface IP
     if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF,
                    reinterpret_cast<const char*>(&localIf), sizeof(localIf)) == SOCKET_ERROR) {
         printLastError("setsockopt(IP_MULTICAST_IF)");
     }

    sockaddr_in group{};
    group.sin_family = AF_INET;
    group.sin_port = htons(5000);
    group.sin_addr.s_addr = inet_addr("239.255.0.1");

    const char* msg = "hello multicast";

    int sent = sendto(sock, msg, static_cast<int>(std::strlen(msg)), 0,
        reinterpret_cast<sockaddr*>(&group), sizeof(group));
    if (sent == SOCKET_ERROR) {
        printLastError("sendto");
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    std::cout << "Sent " << sent << " bytes\n";

    char buffer[1024];
    sockaddr_in sender{};
    int senderLen = sizeof(sender);

    int received = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
        reinterpret_cast<sockaddr*>(&sender), &senderLen);
    if (received == SOCKET_ERROR) {
        printLastError("recvfrom");
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    buffer[received] = '\0';
    std::cout << "Received: " << buffer << "\n";

    closesocket(sock);
    WSACleanup();
    return 0;
}