#include "010_app.hpp"

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
        GOROOM,
        GJOIN,
        GMSG,
        GLEAVE,
        INVITE,
        ACCEPT,
        PMSG,
        PLEAVE,
        INVITES,
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
        if (cmd == "goroom")
            return Command::GOROOM;
        if (cmd == "gjoin")
            return Command::GJOIN;
        if (cmd == "gmsg")
            return Command::GMSG;
        if (cmd == "gleave")
            return Command::GLEAVE;
        if (cmd == "invite")
            return Command::INVITE;
        if (cmd == "accept")
            return Command::ACCEPT;
        if (cmd == "pmsg")
            return Command::PMSG;
        if (cmd == "pleave")
            return Command::PLEAVE;
        if (cmd == "invites")
            return Command::INVITES;
        return Command::UNKNOWN;
    }

    void printHelp()
    {
        std::cout << "Commands:\n"
                  << "  startu                      - move to RUNNING_UDP state\n"
                  << "  stop                        - stop application\n"
                  << "  send <text>                 - send broadcast chat message\n"
                  << "  open <room>                 - create/open a multicast room\n"
                  << "  join <room>                 - join multicast room\n"
                  << "  leave <room>                - leave multicast room\n"
                  << "  mcast <room> <text>         - send multicast chat message\n"
                  << "  goroom <room>               - create guaranteed (TCP) room\n"
                  << "  gjoin <room> <owner_ip>     - join guaranteed room\n"
                  << "  gmsg <room> <text>          - send message in guaranteed room\n"
                  << "  gleave <room>               - leave guaranteed room\n"
                  << "  invite <user> <room>        - send private room invite\n"
                  << "  accept <room>               - accept private invite\n"
                  << "  pmsg <room> <text>          - send private room message\n"
                  << "  pleave <room>               - leave private room\n"
                  << "  invites                     - show pending invitations\n"
                  << "  users                       - show active users\n"
                  << "  help                        - show this help\n";
    }

} // namespace

int app::run(network &net)
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
                    if (!net.sendUSNChat(net.getUsername(), args))
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
                    if (!net.createOpenRoom(args))
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
                auto users = net.getActiveUsers();
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
                    if (!net.joinOpenRoom(args))
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
                    if (!net.leaveOpenRoom(args))
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

                    if (!net.sendOpenRoomMessage(room, text))
                    {
                        std::cout << "Failed to send multicast message.\n";
                    }
                }
                break;
            case Command::GOROOM:
                if (args.empty())
                {
                    std::cout << "Usage: goroom <room>\n";
                }
                else
                {
                    if (!net.createGuaranteedRoom(args))
                    {
                        std::cout << "Failed to create guaranteed room.\n";
                    }
                    else
                    {
                        std::cout << "Guaranteed room created: " << args << "\n";
                    }
                }
                break;
            case Command::GJOIN:
                if (args.empty())
                {
                    std::cout << "Usage: gjoin <room> <owner_ip>\n";
                }
                else
                {
                    std::istringstream iss(args);
                    std::string room;
                    std::string ownerIp;
                    if (!(iss >> room >> ownerIp))
                    {
                        std::cout << "Usage: gjoin <room> <owner_ip>\n";
                        break;
                    }

                    if (!net.joinGuaranteedRoom(room, ownerIp))
                    {
                        std::cout << "Failed to join guaranteed room.\n";
                    }
                    else
                    {
                        std::cout << "Joined guaranteed room: " << room << "\n";
                    }
                }
                break;
            case Command::GMSG:
                if (args.empty())
                {
                    std::cout << "Usage: gmsg <room> <text>\n";
                }
                else
                {
                    std::istringstream iss(args);
                    std::string room;
                    if (!(iss >> room))
                    {
                        std::cout << "Usage: gmsg <room> <text>\n";
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
                        std::cout << "Usage: gmsg <room> <text>\n";
                        break;
                    }

                    if (!net.sendGuaranteedRoomMessage(room, text))
                    {
                        std::cout << "Failed to send message.\n";
                    }
                }
                break;
            case Command::GLEAVE:
                if (args.empty())
                {
                    std::cout << "Usage: gleave <room>\n";
                }
                else
                {
                    if (!net.leaveGuaranteedRoom(args))
                    {
                        std::cout << "Failed to leave room.\n";
                    }
                    else
                    {
                        std::cout << "Left guaranteed room: " << args << "\n";
                    }
                }
                break;
            case Command::INVITE:
                if (args.empty())
                {
                    std::cout << "Usage: invite <user> <room>\n";
                }
                else
                {
                    std::istringstream iss(args);
                    std::string user, room;
                    if (!(iss >> user >> room))
                    {
                        std::cout << "Usage: invite <user> <room>\n";
                        break;
                    }

                    if (!net.createPrivateRoom(room, user))
                    {
                        std::cout << "Failed to send invite.\n";
                    }
                    else
                    {
                        std::cout << "Invite sent to " << user << " for room: " << room << "\n";
                    }
                }
                break;
            case Command::ACCEPT:
                if (args.empty())
                {
                    std::cout << "Usage: accept <room>\n";
                }
                else
                {
                    if (!net.acceptPrivateInvite(args, ""))
                    {
                        std::cout << "Failed to accept invite.\n";
                    }
                    else
                    {
                        std::cout << "Accepted invite for room: " << args << "\n";
                    }
                }
                break;
            case Command::PMSG:
                if (args.empty())
                {
                    std::cout << "Usage: pmsg <room> <text>\n";
                }
                else
                {
                    std::istringstream iss(args);
                    std::string room;
                    if (!(iss >> room))
                    {
                        std::cout << "Usage: pmsg <room> <text>\n";
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
                        std::cout << "Usage: pmsg <room> <text>\n";
                        break;
                    }

                    if (!net.sendPrivateRoomMessage(room, text))
                    {
                        std::cout << "Failed to send message.\n";
                    }
                }
                break;
            case Command::PLEAVE:
                if (args.empty())
                {
                    std::cout << "Usage: pleave <room>\n";
                }
                else
                {
                    if (!net.leavePrivateRoom(args))
                    {
                        std::cout << "Failed to leave room.\n";
                    }
                    else
                    {
                        std::cout << "Left private room: " << args << "\n";
                    }
                }
                break;
            case Command::INVITES:
            {
                auto invites = net.getPendingInvites();
                if (invites.empty())
                {
                    std::cout << "No pending invitations.\n";
                }
                else
                {
                    std::cout << "Pending invitations:\n";
                    for (const auto &inv : invites)
                    {
                        std::cout << "  Room: " << inv.first << " from: " << inv.second << "\n";
                    }
                }
                break;
            }
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
