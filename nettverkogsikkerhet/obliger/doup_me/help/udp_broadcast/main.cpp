// udp_broadcast_win.cpp
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <cstring>

// Link with Ws2_32.lib (MSVC). For MinGW, use -lws2_32.

int main() {
    const char* message = "Hello students! This is a UDP broadcast message (Windows).";
    const unsigned short port = 50000;

    // 1) Initialize Winsock
    WSADATA wsaData;
    int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaResult != 0) {
        std::cerr << "WSAStartup failed: " << wsaResult << "\n";
        return 1;
    }

    // 2) Create UDP socket (BSD-style)
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "socket failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    // 3) Enable broadcast
    BOOL broadcastEnable = TRUE;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST,
                   (const char*)&broadcastEnable, sizeof(broadcastEnable)) == SOCKET_ERROR) {
        std::cerr << "setsockopt(SO_BROADCAST) failed: " << WSAGetLastError() << "\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // Optional: allow rebinding quickly in repeated demos
    BOOL reuseAddr = TRUE;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
               (const char*)&reuseAddr, sizeof(reuseAddr));

    // 4) Destination = limited broadcast 255.255.255.255:port
    sockaddr_in broadcastAddr{};
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(port);
    broadcastAddr.sin_addr.s_addr = inet_addr("192.168.41.255");

    // 5) Send the packet
    int sent = sendto(sock,
                      message,
                      (int)std::strlen(message),
                      0,
                      (sockaddr*)&broadcastAddr,
                      sizeof(broadcastAddr));

    if (sent == SOCKET_ERROR) {
        std::cerr << "sendto failed: " << WSAGetLastError() << "\n";
    } else {
        std::cout << "Broadcast message sent (" << sent << " bytes) to 255.255.255.255:" << port << "\n";
    }

    // 6) Cleanup
    closesocket(sock);
    WSACleanup();
    return 0;
}
