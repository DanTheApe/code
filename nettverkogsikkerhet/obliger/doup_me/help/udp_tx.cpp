// udp_broadcast_win_rx_tx.cpp
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

    // Create sender (TX) socket (do NOT bind it)
    SOCKET txSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (txSock == INVALID_SOCKET) {
        std::cerr << "socket(tx) failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    // Enable broadcast on TX socket
    BOOL broadcastEnable = TRUE;
    if (setsockopt(txSock, SOL_SOCKET, SO_BROADCAST,
                   (const char*)&broadcastEnable, sizeof(broadcastEnable)) == SOCKET_ERROR) {
        std::cerr << "setsockopt(tx, SO_BROADCAST) failed: " << WSAGetLastError() << "\n";
        closesocket(txSock);
        WSACleanup();
        return 1;
    }

    // Destination broadcast address
    sockaddr_in broadcastAddr{};
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(port);

    // Use your subnet broadcast 192.168.41.255
    broadcastAddr.sin_addr.s_addr = inet_addr("192.168.41.255");

    // Send the broadcast packet (once)
    int sent = sendto(txSock,
                      message,
                      (int)std::strlen(message),
                      0,
                      (sockaddr*)&broadcastAddr,
                      sizeof(broadcastAddr));

    if (sent == SOCKET_ERROR) {
        std::cerr << "sendto failed: " << WSAGetLastError() << "\n";
    } else {
        std::cout << "Broadcast message sent (" << sent << " bytes) to "
                  << inet_ntoa(broadcastAddr.sin_addr) << ":" << port << "\n";
    }

    // 7) Cleanup
    closesocket(txSock);
    WSACleanup();
    return 0;
}
