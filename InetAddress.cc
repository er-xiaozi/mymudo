#include "InetAddress.h"

#include <strings.h>
#include <string.h>

InetAddress::InetAddress(uint16_t port, std::string ip)
{
    bzero(&addr_, sizeof addr_);                   // 清空addr_结构体
    addr_.sin_family = AF_INET;                    // 指定地址族为 AF_INET（IPv4 协议）。
    addr_.sin_port = htons(port);                  // htons 将端口号从主机字节序（小端）转换为网络字节序（大端）
    addr_.sin_addr.s_addr = inet_addr(ip.c_str()); // inet_addr 将点分十进制格式的 IP 地址（如 "192.168.1.1"）转换为 32 位的网络字节序二进制值
}

std::string InetAddress::toIp() const
{
    // addr_
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf); // 二进制网络字节序转换为点分十进制
    return buf;
}
std::string InetAddress::toIpPort() const
{
    // ip:port
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
    size_t end = strlen(buf);
    uint16_t port = ntohs(addr_.sin_port);
    sprintf(buf + end, ":%u", port);
    return buf;
}
uint16_t InetAddress::toPort() const
{
    return ntohs(addr_.sin_port);
}

// int main()
// {
//     InetAddress addr(8080);
//     std::cout << addr.toIpPort() <<std::endl;
//     return 0;
// }