#pragma once

#include <iostream>
#include <string>
#include <atomic> // AI sugestion for thread safety

#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <thread>
#include <chrono>

namespace network {
    extern std::atomic_bool del;
    std::string getMyIp();
    std::string anounceMyIp(bool s);
}
