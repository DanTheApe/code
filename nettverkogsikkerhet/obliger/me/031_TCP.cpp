#include "030_TCP.hpp"
#include "040_felles.hpp"

// TYPE|ROOM|USERNAME|PAYLOAD

tcp::tcp(std::string username)
    : username(std::move(username)), myip(felles::getMyIp())
{
}

tcp::~tcp() = default;
