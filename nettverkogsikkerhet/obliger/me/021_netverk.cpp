#include "020_netverk.hpp"

std::string network::getMyIp(){
        int s = socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in remote{};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr);

    connect(s, (sockaddr*)&remote, sizeof(remote));

    sockaddr_in local{};
    socklen_t len = sizeof(local);
    getsockname(s, (sockaddr*)&local, &len);

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &local.sin_addr, ip, sizeof(ip));
    std::string mrip(ip);

    close(s);
    return mrip;
}

void network::anounceMyIp(){
    
}