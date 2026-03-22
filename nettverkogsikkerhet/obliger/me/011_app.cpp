#include "010_app.hpp"

#include <sstream>

namespace {

enum class State {
    IDLE,
    RUNNING,
    STOPPED
};

enum class Command {
    START,
    STOP,
    HELP,
    SEND,
    USERS,
    JOIN,
    LEAVE,
    MCAST,
    UNKNOWN,
    EMPTY
};

Command parseCommand(const std::string& line, std::string& args) {
    std::istringstream iss(line);
    std::string cmd;
    if (!(iss >> cmd)) {
        return Command::EMPTY;
    }

    std::getline(iss, args);
    if (!args.empty() && args.front() == ' ') {
        args.erase(0, 1);
    }

    if (cmd == "start") return Command::START;
    if (cmd == "stop") return Command::STOP;
    if (cmd == "help") return Command::HELP;
    if (cmd == "send") return Command::SEND;
    if (cmd == "users") return Command::USERS;
    if (cmd == "join") return Command::JOIN;
    if (cmd == "leave") return Command::LEAVE;
    if (cmd == "mcast") return Command::MCAST;
    return Command::UNKNOWN;
}

void printHelp() {
    std::cout << "Commands:\n"
              << "  start                 - move to RUNNING state\n"
              << "  stop                  - stop application\n"
              << "  send <text>           - send broadcast chat message\n"
              << "  users                 - show active users\n"
              << "  join <room>           - join multicast room\n"
              << "  leave <room>          - leave multicast room\n"
              << "  mcast <room> <text>   - send multicast chat message\n"
              << "  help                  - show this help\n";
}

} // namespace

int app::run() {
    State state = State::IDLE;

    while (state != State::STOPPED && std::cin.good()) {
        switch (state) {
            case State::IDLE: {
                std::cout << "[IDLE] Type 'start' or 'help': ";
                std::string line;
                if (!std::getline(std::cin, line)) {
                    state = State::STOPPED;
                    break;
                }

                std::string args;
                Command cmd = parseCommand(line, args);
                switch (cmd) {
                    case Command::START:
                        state = State::RUNNING;
                        std::cout << "App state -> RUNNING\n";
                        break;
                    case Command::STOP:
                        state = State::STOPPED;
                        break;
                    case Command::HELP:
                        printHelp();
                        break;
                    case Command::EMPTY:
                        break;
                    default:
                        std::cout << "Invalid command in IDLE. Try 'start' or 'help'.\n";
                        break;
                }
                break;
            }

            case State::RUNNING: {
                std::cout << "[RUNNING] Enter command ('help' for list): ";
                std::string line;
                if (!std::getline(std::cin, line)) {
                    state = State::STOPPED;
                    break;
                }

                std::string args;
                Command cmd = parseCommand(line, args);
                switch (cmd) {
                    case Command::SEND:
                        if (args.empty()) {
                            std::cout << "Usage: send <text>\n";
                        } else {
                            std::cout << "SEND -> " << args << "\n";
                        }
                        break;
                    case Command::USERS:
                        std::cout << "USERS -> (hook into active-user table)\n";
                        break;
                    case Command::JOIN:
                        if (args.empty()) {
                            std::cout << "Usage: join <room>\n";
                        } else {
                            std::cout << "JOIN -> " << args << "\n";
                        }
                        break;
                    case Command::LEAVE:
                        if (args.empty()) {
                            std::cout << "Usage: leave <room>\n";
                        } else {
                            std::cout << "LEAVE -> " << args << "\n";
                        }
                        break;
                    case Command::MCAST:
                        if (args.empty()) {
                            std::cout << "Usage: mcast <room> <text>\n";
                        } else {
                            std::cout << "MCAST -> " << args << "\n";
                        }
                        break;
                    case Command::HELP:
                        printHelp();
                        break;
                    case Command::STOP:
                        state = State::STOPPED;
                        break;
                    case Command::START:
                        std::cout << "Already RUNNING.\n";
                        break;
                    case Command::EMPTY:
                        break;
                    case Command::UNKNOWN:
                    default:
                        std::cout << "Unknown command. Type 'help'.\n";
                        break;
                }
                break;
            }

            case State::STOPPED:
            default:
                break;
        }
    }

    return 0;
}
