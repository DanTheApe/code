#pragma once

#include <iostream>

#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

namespace network {
    std::string getMyIp();
    void anounceMyIp();
}
