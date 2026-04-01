#include "020_netverk.hpp"
#include "010_app.hpp"
#include "030_TCP.hpp"
#include "040_felles.hpp"

#include <iostream>

#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <thread>
#include <chrono>

int main()
{
    std::cout << "Type your username: ";
    std::string username;
    std::getline(std::cin, username);
    felles::setUsername(username);

    network net;
    tcp tcpObj(net.getUsername());

    std::cout << "START" << std::endl;
    std::cout << "Geting my IP address" << std::endl;
    std::string ip = net.getMyIp();
    std::cout << "My IP address is: " << ip << std::endl;

    net.setTcpObject(&tcpObj);

    std::cout << "Anouncing my IP address" << std::endl;
    std::thread t(&network::anounceMyIp, &net, true);
    std::thread l(&network::listen, &net, false, true);
    std::thread tcpThread(&tcp::tcpListen, &tcpObj);

    std::string anounceMsg = net.anounceMyIp(false);
    std::cout << "Anounce message: " << anounceMsg << std::endl;

    std::string msg = net.anounceMyIp(false);
    std::vector<std::string> parts = net.parseMessage(msg);
    for (const auto &part : parts)
    {
        std::cout << "Part: " << part << std::endl;
    }

    app::run(net);

    net.stop();
    t.join();
    l.join();
    tcpThread.join();

    return 0;
}
