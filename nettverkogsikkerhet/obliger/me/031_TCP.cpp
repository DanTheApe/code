#include "030_TCP.hpp"

// TYPE|ROOM|USERNAME|PAYLOAD

std::string tcp::getMyIp()
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