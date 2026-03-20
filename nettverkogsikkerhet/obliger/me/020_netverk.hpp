#pragma once

#include <iostream>

#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <thread>
#include <chrono>

namespace network {
    static bool del{o};
    std::string getMyIp();
    std::string anounceMyIp(bool s);
}
