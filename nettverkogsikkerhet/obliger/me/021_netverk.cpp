#include "020_netverk.hpp"
std::atomic_bool network::del{false};

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

    if (mrip == "0.0.0.0") { mrip = "192.168.41.22";} // IF it don't fined my ip I just hardcode it. Problem on IRI-LAb

    close(s);
    return mrip;
}

std::string network::anounceMyIp(bool b){
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) { perror("socket"); return ""; } // AI

    int enable = 1;
    if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable)) < 0) { // AI
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
    if (b){
        while (!network::del) {
            ssize_t n = sendto(s, msg.c_str(), msg.size(), 0, reinterpret_cast<sockaddr*>(&dst), sizeof(dst));
            if (n < 0) perror("sendto"); // AI
            sleep(10);
        }
    }
    ssize_t n = sendto(s, msg.c_str(), msg.size(), 0, reinterpret_cast<sockaddr*>(&dst), sizeof(dst));
    if (n < 0) perror("sendto"); // AI
    close(s);
    return msg;
    // AI brukt til å finne feil. Hadde satt inet_pton til 255.255.255.255 som ikke fungerete, men 192.168.41.255 fungrete.
}