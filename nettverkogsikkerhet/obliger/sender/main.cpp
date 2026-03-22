

#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

volatile sig_atomic_t keepRunning = 1;

void handleSignal(int) {
    keepRunning = 0;
}

std::string getUsername() {
    const char* user = std::getenv("USER");
    if (user != nullptr && std::strlen(user) > 0) {
        return user;
    }
    return "unknown";
}

}  // namespace

int main() {
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    constexpr int kPort = 50000;
    const std::vector<std::string> destinations = {
        "255.255.255.255",
        "192.168.11.255",
    };

    const std::string username = getUsername();

    const int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket: " << std::strerror(errno) << '\n';
        return 1;
    }

    int enableBroadcast = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &enableBroadcast, sizeof(enableBroadcast)) < 0) {
        std::cerr << "Failed to enable SO_BROADCAST: " << std::strerror(errno) << '\n';
        close(sock);
        return 1;
    }

    while (keepRunning) {
        for (const std::string& destination : destinations) {
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(kPort);

            if (inet_pton(AF_INET, destination.c_str(), &addr.sin_addr) != 1) {
                std::cerr << "Invalid destination IP: " << destination << '\n';
                continue;
            }

            const std::string message = "PRESENCE|-|" + username + "|" + destination;
            const ssize_t sent = sendto(
                sock,
                message.c_str(),
                message.size(),
                0,
                reinterpret_cast<const sockaddr*>(&addr),
                sizeof(addr));

            if (sent < 0) {
                std::cerr << "Send failed to " << destination << ": " << std::strerror(errno) << '\n';
            } else {
                std::cout << "Sent to " << destination << ':' << kPort << " -> " << message << '\n';
            }
        }

        if (keepRunning) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }

    close(sock);
    return 0;
}