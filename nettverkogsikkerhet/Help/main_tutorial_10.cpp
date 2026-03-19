#include "Application.hpp"
#include <iostream>

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")

#include "ParseMessage.h"

int main()
{
    // UDP connect trick
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in remote{};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr); // any external IP

    connect(s, (sockaddr*)&remote, sizeof(remote));

    sockaddr_in local{};
    int len = sizeof(local);
    getsockname(s, (sockaddr*)&local, &len);

    std::cout << "Local IP used: " << inet_ntoa(local.sin_addr) << "\n";

    closesocket(s);
    WSACleanup();

    // Get IP from selected interface
    ULONG size = 15000;
    IP_ADAPTER_ADDRESSES* adapters = (IP_ADAPTER_ADDRESSES*)malloc(size);

    GetAdaptersAddresses(AF_INET, 0, nullptr, adapters, &size);

    int index = 0;
    IP_ADAPTER_ADDRESSES* list[50];

    // List interfaces
    for (auto a = adapters; a; a = a->Next) {
        std::wcout << index << L": " << a->FriendlyName << std::endl;
        list[index++] = a;
    }

    // User selection
    int choice;
    std::cout << "Select interface: ";
    std::cin >> choice;

    auto adapter = list[choice];

    // Print IP addresses
    for (auto u = adapter->FirstUnicastAddress; u; u = u->Next) {
        sockaddr_in* addr = (sockaddr_in*)u->Address.lpSockaddr;

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));

        std::cout << "Interface IP: " << ip << std::endl;
    }
    free(adapters);


    // Protocoll parsing
    ParseMessage msg("CHAT|USN Chat|alice|Hello everyone!\n");

    if (!msg.isValid()) {
        std::cout << "Invalid message\n";
        return 1;
    }

    std::cout << "Type: " << msg.getTypeString() << "\n";
    std::cout << "Room: " << msg.getRoom() << "\n";
    std::cout << "User: " << msg.getUsername() << "\n";
    std::cout << "Payload: " << msg.getPayload() << "\n";

    if (msg.isChat()) {
        std::cout << "This is a chat message.\n";
    }

    return 0;
}