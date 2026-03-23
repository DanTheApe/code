#include "010_app.hpp"

#include <sstream>

namespace
{

    enum class State
    {
        IDLE,
        RUNNING_UDP,
        STOPPED
    };

    enum class Command
    {
        STARTU,
        STOP,
        HELP,
        SEND,
        OPEN,
        USERS,
        JOIN,
        LEAVE,
        MCAST,
        UNKNOWN,
        EMPTY
    };

    Command parseCommand(const std::string &line, std::string &args)
    {
        std::istringstream iss(line);
        std::string cmd;
        if (!(iss >> cmd))
        {
            return Command::EMPTY;
        }

        std::getline(iss, args);
        if (!args.empty() && args.front() == ' ')
        {
            args.erase(0, 1);
        }

        if (cmd == "startu")
            return Command::STARTU;
        if (cmd == "stop")
            return Command::STOP;
        if (cmd == "help")
            return Command::HELP;
        if (cmd == "send")
            return Command::SEND;
        if (cmd == "open")
            return Command::OPEN;
        if (cmd == "users")
            return Command::USERS;
        if (cmd == "join")
            return Command::JOIN;
        if (cmd == "leave")
            return Command::LEAVE;
        if (cmd == "mcast")
            return Command::MCAST;
        return Command::UNKNOWN;
    }

    void printHelp()
    {
        std::cout << "Commands:\n"
                  << "  startu                 - move to RUNNING_UDP state\n"
                  << "  stop                  - stop application\n"
                  << "  send <text>           - send broadcast chat message\n"
                  << "  open <room>           - create/open a multicast room\n"
                  << "  users                 - show active users\n"
                  << "  join <room>           - join multicast room\n"
                  << "  leave <room>          - leave multicast room\n"
                  << "  mcast <room> <text>   - send multicast chat message\n"
                  << "  help                  - show this help\n";
    }

} // namespace

int app::run()
{
    State state = State::IDLE;

    while (state != State::STOPPED && std::cin.good())
    {
        switch (state)
        {
        case State::IDLE:
        {
            std::cout << "[IDLE] Type 'startu' or 'help': ";
            std::string line;
            if (!std::getline(std::cin, line))
            {
                state = State::STOPPED;
                break;
            }

            std::string args;
            Command cmd = parseCommand(line, args);
            switch (cmd)
            {
            case Command::STARTU:
                state = State::RUNNING_UDP;
                std::cout << "App state -> RUNNING_UDP\n";
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
                std::cout << "Invalid command in IDLE. Try 'startu' or 'help'.\n";
                break;
            }
            break;
        }

        case State::RUNNING_UDP:
        {
            std::cout << "[RUNNING_UDP] Enter command ('help' for list): ";
            std::string line;
            if (!std::getline(std::cin, line))
            {
                state = State::STOPPED;
                break;
            }

            std::string args;
            Command cmd = parseCommand(line, args);
            switch (cmd)
            {
            case Command::SEND:
                if (args.empty())
                {
                    std::cout << "Usage: send <text>\n";
                }
                else
                {
                    if (!network::sendUSNChat(network::getUsername(), args))
                    {
                        std::cout << "Failed to send message.\n";
                    }
                }
                break;
            case Command::OPEN:
                if (args.empty())
                {
                    std::cout << "Usage: open <room>\n";
                }
                else
                {
                    if (!network::createOpenRoom(args))
                    {
                        std::cout << "Failed to create room.\n";
                    }
                    else
                    {
                        std::cout << "Open room announced: " << args << "\n";
                    }
                }
                break;
            case Command::USERS:
            {
                auto users = network::getActiveUsers();
                if (users.empty())
                {
                    std::cout << "No active users.\n";
                }
                else
                {
                    std::cout << "Active users:\n";
                    for (const auto &u : users)
                    {
                        std::cout << "  " << u << "\n"; // username|ip
                    }
                }
                break;
            }

            case Command::JOIN:
                if (args.empty())
                {
                    std::cout << "Usage: join <room>\n";
                }
                else
                {
                    if (!network::joinOpenRoom(args))
                    {
                        std::cout << "Failed to join room.\n";
                    }
                    else
                    {
                        std::cout << "Joined room: " << args << "\n";
                    }
                }
                break;
            case Command::LEAVE:
                if (args.empty())
                {
                    std::cout << "Usage: leave <room>\n";
                }
                else
                {
                    if (!network::leaveOpenRoom(args))
                    {
                        std::cout << "Failed to leave room.\n";
                    }
                    else
                    {
                        std::cout << "Left room: " << args << "\n";
                    }
                }
                break;
            case Command::MCAST:
                if (args.empty())
                {
                    std::cout << "Usage: mcast <room> <text>\n";
                }
                else
                {
                    std::istringstream iss(args);
                    std::string room;
                    if (!(iss >> room))
                    {
                        std::cout << "Usage: mcast <room> <text>\n";
                        break;
                    }

                    std::string text;
                    std::getline(iss, text);
                    if (!text.empty() && text.front() == ' ')
                    {
                        text.erase(0, 1);
                    }

                    if (text.empty())
                    {
                        std::cout << "Usage: mcast <room> <text>\n";
                        break;
                    }

                    if (!network::sendOpenRoomMessage(room, text))
                    {
                        std::cout << "Failed to send multicast message.\n";
                    }
                }
                break;
            case Command::HELP:
                printHelp();
                break;
            case Command::STOP:
                state = State::STOPPED;
                break;
            case Command::STARTU:
                std::cout << "Already RUNNING_UDP.\n";
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
