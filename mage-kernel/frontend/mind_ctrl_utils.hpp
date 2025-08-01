#ifndef __MIND_CTRL_UTILS_H__
#define __MIND_CTRL_UTILS_H__

#include <getopt.h>
#include <unistd.h>

static uint64_t parse_mac(std::string const &in)
{
    unsigned int bytes[6];
    if (std::sscanf(in.c_str(),
                    "%02x:%02x:%02x:%02x:%02x:%02x",
                    &bytes[0], &bytes[1], &bytes[2],
                    &bytes[3], &bytes[4], &bytes[5]) != 6)
    {
        throw std::runtime_error(in + std::string(" is an invalid MAC address"));
    }

    uint64_t mac_addr = 0 + bytes[0];
    for (int i = 1; i < 6; i++)
    {
        mac_addr <<= 8;
        mac_addr += bytes[i];
    }
    return mac_addr;
}

static uint32_t parse_ipv4(std::string const &in)
{
    unsigned int bytes[4];
    if (std::sscanf(in.c_str(),
                    "%u.%u.%u.%u",
                    &bytes[0], &bytes[1], &bytes[2], &bytes[3]) != 4)
    {
        throw std::runtime_error(in + std::string(" is an invalid IP address"));
    }
    uint32_t ip_addr = 0 + bytes[0];
    for (int i = 1; i < 4; i++)
    {
        ip_addr <<= 8;
        ip_addr += bytes[i];
    }
    return ip_addr;
}

#endif  /* __MIND_CTRL_UTILS_H__ */