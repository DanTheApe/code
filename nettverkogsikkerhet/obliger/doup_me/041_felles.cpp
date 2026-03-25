#include "040_felles.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

std::string felles::myUsername = "BiG";
std::atomic_bool felles::del{false};

std::string felles::getUsername()
{
    return myUsername;
}

void felles::setUsername(const std::string &username)
{
    myUsername = username;
}

std::string felles::getMyIp()
{
    int s = socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in remote{};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr);

    connect(s, reinterpret_cast<sockaddr *>(&remote), sizeof(remote));

    sockaddr_in local{};
    socklen_t len = sizeof(local);
    getsockname(s, reinterpret_cast<sockaddr *>(&local), &len);

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &local.sin_addr, ip, sizeof(ip));
    std::string myIp(ip);

    if (myIp == "0.0.0.0")
    {
        myIp = fallbackLocalIp;
    }

    close(s);
    return myIp;
}

std::vector<std::string> felles::parseMessage(const std::string &msg)
{
    std::string clean = msg;
    while (!clean.empty() && (clean.back() == '\n' || clean.back() == '\r'))
    {
        clean.pop_back();
    }

    std::vector<std::string> parts;
    int start = 0;
    int pos = clean.find('|');

    while (pos != std::string::npos)
    {
        parts.push_back(clean.substr(start, pos - start));
        start = pos + 1;
        pos = clean.find('|', start);
    }
    parts.push_back(clean.substr(start));

    if (parts.size() != 4)
    {
        return {};
    }

    return parts;
}
std::string felles::encryptXor(const std::string &plaintext, const std::string &key)
{
    if (key.empty())
        return plaintext;

    std::string encrypted;
    for (size_t i = 0; i < plaintext.size(); ++i)
    {
        encrypted += plaintext[i] ^ key[i % key.size()];
    }
    return encrypted;
}

std::string felles::decryptXor(const std::string &ciphertext, const std::string &key)
{
    return encryptXor(ciphertext, key);
}