// udp_broadcast_win_rx_tx.cpp
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <cstring>

// Link with Ws2_32.lib (MSVC). For MinGW, use -lws2_32.

int main() {
    //const char* message = "Hello students! This is a UDP broadcast message (Windows).";
    const char* message = "Hi, my name is Ingar";
    const unsigned short port = 50000;

    // Initialize Winsock
    WSADATA wsaData;
    int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaResult != 0) {
        std::cerr << "WSAStartup failed: " << wsaResult << "\n";
        return 1;
    }

    // Create receiver (RX) socket and bind to :port
    SOCKET rxSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (rxSock == INVALID_SOCKET) {
        std::cerr << "socket(rx) failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    // Allow reuse of socket, eg to allow multiple applications running at oce
    BOOL reuseAddr = TRUE;
    if (setsockopt(rxSock, SOL_SOCKET, SO_REUSEADDR,
                   (const char*)&reuseAddr, sizeof(reuseAddr)) == SOCKET_ERROR) {
        std::cerr << "setsockopt(rx, SO_REUSEADDR) failed: " << WSAGetLastError() << "\n";
        closesocket(rxSock);
        WSACleanup();
        return 1;
    }

    sockaddr_in localAddr{};
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(port);
    localAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(rxSock, (sockaddr*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR) {
        std::cerr << "bind(rx) failed: " << WSAGetLastError() << "\n";
        closesocket(rxSock);
        WSACleanup();
        return 1;
    }

    // Create sender (TX) socket
    SOCKET txSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (txSock == INVALID_SOCKET) {
        std::cerr << "socket(tx) failed: " << WSAGetLastError() << "\n";
        closesocket(rxSock);
        WSACleanup();
        return 1;
    }

    // Enable broadcast on TX socket
    BOOL broadcastEnable = TRUE;
    if (setsockopt(txSock, SOL_SOCKET, SO_BROADCAST,
                   (const char*)&broadcastEnable, sizeof(broadcastEnable)) == SOCKET_ERROR) {
        std::cerr << "setsockopt(tx, SO_BROADCAST) failed: " << WSAGetLastError() << "\n";
        closesocket(txSock);
        closesocket(rxSock);
        WSACleanup();
        return 1;
    }

    // Destination broadcast address
    sockaddr_in broadcastAddr{};
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(port);

    // Use your subnet broadcast 192.168.41.255, must be correct, otherwise no broadcast will work. Depends on the subnet mask
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

    // Receive loop on RX socket
    std::cout << "Listening for UDP packets on port " << port << "...\n";

    char recvBuffer[2048];
    sockaddr_in senderAddr{};
    int senderAddrSize = sizeof(senderAddr);

    while (true) {
        int received = recvfrom(rxSock,
                                recvBuffer,
                                (int)sizeof(recvBuffer) - 1,
                                0,
                                (sockaddr*)&senderAddr,
                                &senderAddrSize);

        if (received == SOCKET_ERROR) {
            std::cerr << "recvfrom failed: " << WSAGetLastError() << "\n";
            break;
        }

        recvBuffer[received] = '\0';

        char senderIp[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &senderAddr.sin_addr, senderIp, sizeof(senderIp));

        std::cout << "Received " << received << " bytes from "
                  << senderIp << ":" << ntohs(senderAddr.sin_port)
                  << " -> " << recvBuffer << "\n";
    }

    // 7) Cleanup
    closesocket(txSock);
    closesocket(rxSock);
    WSACleanup();
    return 0;
}
