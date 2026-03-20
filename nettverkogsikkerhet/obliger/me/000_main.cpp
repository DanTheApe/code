#include "020_netverk.hpp"

#include <iostream>

#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

int main(){
    std::cout << "START" << std::endl;
    std::cout << "Geting my IP address" << std::endl;
    std::string ip = network::getMyIp();
    std::cout << "My IP address is: " << ip << std::endl;

    return 0;
}