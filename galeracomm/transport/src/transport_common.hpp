#ifndef TRANSPORT_COMMON_HPP
#define TRANSPORT_COMMON_HPP

#include "gcomm/transport.hpp"
#include "gcomm/logger.hpp"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

class PendingWriteBuf {
public:
    WriteBuf *wb;
    size_t offset;
    PendingWriteBuf(WriteBuf *_wb, size_t _offset) : wb(_wb), offset(_offset)
	{}
    ~PendingWriteBuf() {
    }
};

inline void closefd(int fd)
{
    while (::close(fd) == -1 && errno == EINTR) {}
}

static inline std::string to_string(const sockaddr* sa)
{
    std::string ret;
    if (sa->sa_family == AF_INET) {
	ret += ::inet_ntoa(reinterpret_cast<const sockaddr_in*>(sa)->sin_addr);
	ret += ":";
	ret += to_string(gu_be16(reinterpret_cast<const sockaddr_in*>(sa)->sin_port));
    }
    else
	ret = "Unknown address family " + ::to_string(int64_t(sa->sa_family));
    return ret;
}


#endif // !TRANSPORT_COMMON_HPP
