#include "020_netverk.hpp"

#include <iostream>

#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <thread>
#include <chrono>

int main(){
    std::cout << "START" << std::endl;
    std::cout << "Geting my IP address" << std::endl;
    std::string ip = network::getMyIp();
    std::cout << "My IP address is: " << ip << std::endl;
    std::cout << "Anouncing my IP address" << std::endl;
    int i = 0;
    while (i<10) {
    std::thread(network::anounceMyIp(1));
    std::cout << network::anounceMyIp(0) << "\n";
    i++;
    }
    sleep(20);
    network::del = true;
    return 0;
}