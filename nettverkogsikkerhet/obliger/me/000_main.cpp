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
    std::thread t(network::anounceMyIp, true);
    

    std::string u;
    std::cin >> u;
    while (u != "exit"){
        std::cin >> u;
    }

    network::del.store(true);
    t.join();
    return 0;
}
