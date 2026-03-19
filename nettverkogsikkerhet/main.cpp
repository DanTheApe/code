#include "chat_node.hpp"

#include "network_utils.hpp"

#include "network_utils.cpp"
#include "chat_node.cpp"

#include <csignal>
#include <iostream>
#include <string>

namespace {

volatile std::sig_atomic_t gInterrupted = 0;

void onInterrupt(int) {
    gInterrupted = 1;
}

void printHelp() {
    std::cout
        << "Commands:\n"
        << "  /help\n"
        << "  /users\n"
        << "  /rooms\n"
        << "  /common <message>\n"
        << "  /mkgroup <room>\n"
        << "  /join <room>\n"
        << "  /leave <room>\n"
        << "  /gmsg <room> <message>\n"
        << "  /invite <username>\n"
        << "  /dm <username> <message>\n"
        << "  /quit\n";
}

} // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, onInterrupt);

    std::string username;

    if (argc >= 2) {
        username = argv[1];
    }

    if (username.empty()) {
        std::cout << "Username: ";
        std::getline(std::cin, username);
        username = net::trim(username);
    }
    if (username.empty()) {
        std::cerr << "Username cannot be empty\n";
        return 1;
    }

    ChatNode node(username);
    if (!node.start()) {
        return 1;
    }

    std::cout << "Started as '" << node.username() << "' @ " << node.localIp() << "\n";
    std::cout << "RFC UDP port: " << ChatNode::kUdpPort << ", broadcast: " << node.broadcastIp() << "\n";
    printHelp();

    std::string line;
    while (!gInterrupted) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) {
            break;
        }

        line = net::trim(line);
        if (line.empty()) {
            continue;
        }

        if (line == "/quit") {
            break;
        }
        if (line == "/help") {
            printHelp();
            continue;
        }
        if (line == "/users") {
            node.printUsers();
            continue;
        }
        if (line == "/rooms") {
            node.printRooms();
            continue;
        }

        if (line.rfind("/common ", 0) == 0) {
            node.sendCommonMessage(net::trim(line.substr(8)));
            continue;
        }

        if (line.rfind("/mkgroup ", 0) == 0) {
            const std::string room = net::trim(line.substr(9));
            if (room.empty()) {
                std::cerr << "Usage: /mkgroup <room>\n";
            } else if (!node.createGroupRoom(room)) {
                std::cerr << "Failed creating room\n";
            }
            continue;
        }

        if (line.rfind("/join ", 0) == 0) {
            const auto room = net::trim(line.substr(6));
            if (!node.joinGroupRoom(room)) {
                std::cerr << "Unknown room\n";
            }
            continue;
        }

        if (line.rfind("/leave ", 0) == 0) {
            node.leaveGroupRoom(net::trim(line.substr(7)));
            continue;
        }

        if (line.rfind("/gmsg ", 0) == 0) {
            const auto firstSpace = line.find(' ', 6);
            if (firstSpace == std::string::npos) {
                std::cerr << "Usage: /gmsg <room> <message>\n";
                continue;
            }
            const std::string room = line.substr(6, firstSpace - 6);
            const std::string message = net::trim(line.substr(firstSpace + 1));
            node.sendGroupMessage(room, message);
            continue;
        }

        if (line.rfind("/invite ", 0) == 0) {
            const auto user = net::trim(line.substr(8));
            node.inviteOneToOne(user);
            continue;
        }

        if (line.rfind("/dm ", 0) == 0) {
            const auto firstSpace = line.find(' ', 4);
            if (firstSpace == std::string::npos) {
                std::cerr << "Usage: /dm <user> <message>\n";
                continue;
            }
            const std::string user = line.substr(4, firstSpace - 4);
            const std::string message = net::trim(line.substr(firstSpace + 1));
            node.sendDirectMessage(user, message);
            continue;
        }

        std::cerr << "Unknown command. Use /help\n";
    }

    node.stop();
    return 0;
}