#include "UDPRelay.hpp"
#include "Buffer.hpp"
#include "CipherEnv.hpp"
#include "NetUtils.hpp"
#include "UDPConnectionContext.hpp"
#include "ssrutils.h"
#include "uvw/dns.h"

#if defined(IP_TOS) && !defined(_WIN32)
#define SET_IP_TOS(h)                                                      \
    do {                                                                   \
        uv_os_fd_t fd = h->fileno();                                       \
        int tos = 46 << 2;                                                 \
        int rc = setsockopt(fd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));    \
        if (rc < 0 && errno != ENOPROTOOPT) {                              \
            LOGE("setting ipv4 dscp failed: %d", errno);                   \
        }                                                                  \
        rc = setsockopt(fd, IPPROTO_IPV6, IPV6_TCLASS, &tos, sizeof(tos)); \
        if (rc < 0 && errno != ENOPROTOOPT) {                              \
            LOGE("setting ipv6 dscp failed: %d", errno);                   \
        }                                                                  \
    } while (0)
#else
#define SET_IP_TOS(fd)
#endif //IP_TOS AND NOT _WIN32

namespace
{

inline uint16_t load16_be(const void* s)
{
    const auto* in = (const uint8_t*)s;
    return ((uint16_t)in[0] << 8)
        | ((uint16_t)in[1]);
}

}

UDPRelay::UDPRelay(std::shared_ptr<uvw::Loop> loop, CipherEnv& cipherEnv, const profile_t& profile)
    : loop(std::move(loop))
    , cipherEnvPtr(&cipherEnv)
    , timeout { profile.timeout }
{
}
UDPRelay::~UDPRelay()
{
    socketCache.clear();
    if (protocol_global) {
        free(protocol_global);
        protocol_global = nullptr;
    }
    if (udpServer) {
        udpServer->clear();
        udpServer->close();
    }
}
int UDPRelay::initUDPRelay(int mtu, const char* host, int port, struct sockaddr_storage remote_addr)
{
    remoteAddr = remote_addr;
    if (mtu > 0) {
        packet_size = mtu - PACKET_HEADER_SIZE;
        buf_size = packet_size * 2;
    }
    udpServer = loop->resource<uvw::UDPHandle>();
    udpServer->on<uvw::ErrorEvent>([this](auto& e, auto& h) {
        LOGE("[udp]local error %s", e.what());
    });
    sockaddr_storage localStorage;
    if (ssr_get_sock_addr(loop, host, port, &localStorage, 0) == -1) {
        LOGE("[udp]can't bind to %s:%d", host, port);
        return -1;
    }
    udpServer->bind(reinterpret_cast<const sockaddr&>(localStorage), uvw::Flags<uvw::UDPHandle::Bind>::from<uvw::UDPHandle::Bind::REUSEADDR>());
    SET_IP_TOS(udpServer);
    udpServer->on<uvw::UDPDataEvent>([this](auto& e, auto& h) {
        localBuf = std::make_unique<Buffer>();
        serverRecv(e, h);
    });
    udpServer->recv();
    return 0;
}
void UDPRelay::serverRecv(uvw::UDPDataEvent& data, uvw::UDPHandle& handle)
{
    /*
     *
     * SOCKS5 UDP Request
     * +----+------+------+----------+----------+----------+
     * |RSV | FRAG | ATYP | DST.ADDR | DST.PORT |   DATA   |
     * +----+------+------+----------+----------+----------+
     * | 2  |  1   |  1   | Variable |    2     | Variable |
     * +----+------+------+----------+----------+----------+
     *
     * SOCKS5 UDP Response
     * +----+------+------+----------+----------+----------+
     * |RSV | FRAG | ATYP | DST.ADDR | DST.PORT |   DATA   |
     * +----+------+------+----------+----------+----------+
     * | 2  |  1   |  1   | Variable |    2     | Variable |
     * +----+------+------+----------+----------+----------+
     *
     * shadowsocks UDP Request (before encrypted)
     * +------+----------+----------+----------+
     * | ATYP | DST.ADDR | DST.PORT |   DATA   |
     * +------+----------+----------+----------+
     * |  1   | Variable |    2     | Variable |
     * +------+----------+----------+----------+
     *
     * shadowsocks UDP Response (before encrypted)
     * +------+----------+----------+----------+
     * | ATYP | DST.ADDR | DST.PORT |   DATA   |
     * +------+----------+----------+----------+
     * |  1   | Variable |    2     | Variable |
     * +------+----------+----------+----------+
     *
     * shadowsocks UDP Request and Response (after encrypted)
     * +-------+--------------+
     * |   IV  |    PAYLOAD   |
     * +-------+--------------+
     * | Fixed |   Variable   |
     * +-------+--------------+
     *
     */
    int addr_header_len = 0;
    int frag = data.data[2];
    char host[257] = { 0 };
    char port[65] = { 0 };
    unsigned int offset = 3;
    sockaddr_storage dst_addr;
    memset(&dst_addr, 0, sizeof(struct sockaddr_storage));
    addr_header_len = parseUDPRelayHeader(data.data.get() + offset,
        data.length - offset, host, port, &dst_addr);
    if (addr_header_len == 0) {
        LOGE("[udp] panic:parse header error");
        panic(data.sender);
        return;
    }
    if (socketCache.find(data.sender) != socketCache.end()) {
        auto& ctx = socketCache[data.sender];
        if (!SockAddrEqual {}(data.sender, ctx->srcAddr)) {
            socketCache.erase(data.sender);
        }
    }
    if (frag) {
        LOGE("[udp] drop a message since frag is not 0, but %d", frag);
        panic(data.sender);
        return;
    }
    std::shared_ptr<UDPConnectionContext> remoteCtx;
    if (socketCache.find(data.sender) == socketCache.end()) {
        auto remoteSocket = loop->resource<uvw::UDPHandle>();
        remoteSocket->on<uvw::ErrorEvent>([this](auto& e, auto& h) {
            LOGE("[udp]remote error %s", e.what());
        });
        auto remoteAddrType = reinterpret_cast<sockaddr*>(&remoteAddr)->sa_family;
        if (remoteAddrType == AF_INET6) {
            struct sockaddr_in6 addr;
            memset(&addr, 0, sizeof(struct sockaddr_in6));
            addr.sin6_family = AF_INET6;
            addr.sin6_addr = in6addr_any;
            addr.sin6_port = 0;
            remoteSocket->bind(reinterpret_cast<const sockaddr&>(addr));
        } else {
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(struct sockaddr_in));
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = 0;
            remoteSocket->bind(reinterpret_cast<const sockaddr&>(addr));
        }
        SET_IP_TOS(remoteSocket);
        remoteCtx = std::make_shared<UDPConnectionContext>(data.sender, remoteSocket);
        remoteCtx->initTimer(
            loop, [this, addr = data.sender]() { panic(addr); }, uvw::TimerHandle::Time { timeout });
        socketCache.insert({ data.sender, remoteCtx });
        remoteSocket->on<uvw::UDPDataEvent>([this, addr = data.sender](auto& e, auto& h) {
            this->remoteRecv(e, h, addr);
        });
        remoteSocket->recv();
    } else {
        remoteCtx = socketCache[data.sender];
        remoteCtx->resetTimeoutTimer();
    }
    if (offset > 0) {
        localBuf->copyFromBegin(data.data.get() + offset, data.length - offset);
    }
    int err = localBuf->ssEncryptAll(*cipherEnvPtr);
    if (err) {
        panic(data.sender);
        return;
    }
    remoteCtx->remote->send(reinterpret_cast<const sockaddr&>(remoteAddr), localBuf->duplicateDataToArray(), localBuf->length());
    localBuf->setLength(0);
}
void UDPRelay::panic(const uvw::Addr& addr)
{
    if (socketCache.find(addr) != socketCache.end())
        socketCache.erase(addr);
}
void UDPRelay::remoteRecv(uvw::UDPDataEvent& data, uvw::UDPHandle& handle, const uvw::Addr& localSrcAddr)
{
    if (socketCache.find(localSrcAddr) == socketCache.end()) {
        panic(localSrcAddr);
        return;
    }
    auto& ctx = socketCache[localSrcAddr];
    ctx->remoteBuf->copy(data);
    int err = ctx->remoteBuf->ssDecryptALl(*cipherEnvPtr);
    if (err) {
        panic(localSrcAddr);
        return;
    }
    int len = parseUDPRelayHeader(*ctx->remoteBuf->getBufPtr(), ctx->remoteBuf->length(), nullptr, nullptr, nullptr);
    if (len == 0) {
        LOGE("[udp] error in parse header");
        panic(localSrcAddr);
        return;
    }
    auto response = std::make_unique<char[]>(ctx->remoteBuf->length() + 3);
    memcpy(response.get() + 3, ctx->remoteBuf->begin(), ctx->remoteBuf->length());
    ctx->resetTimeoutTimer();
    udpServer->send(ctx->srcAddr, std::move(response), ctx->remoteBuf->length() + 3);
    ctx->remoteBuf->setLength(0);
}
int UDPRelay::parseUDPRelayHeader(const char* buf, size_t buf_len, char* host, char* port, struct sockaddr_storage* storage)
{
    const uint8_t atyp = *(uint8_t*)buf;
    int offset = 1;

    // get remote addr and port
    if ((atyp & ADDRTYPE_MASK) == 1) {
        // IP V4
        size_t in_addr_len = sizeof(struct in_addr);
        if (buf_len >= in_addr_len + 3) {
            if (storage != nullptr) {
                auto* addr = (struct sockaddr_in*)storage;
                addr->sin_family = AF_INET;
                memcpy(&addr->sin_addr, buf + offset, sizeof(struct in_addr));
                memcpy(&addr->sin_port, buf + offset + in_addr_len, sizeof(uint16_t));
            }
            if (host != nullptr) {
                uv_inet_ntop(AF_INET, (const void*)(buf + offset),
                    host, INET_ADDRSTRLEN);
            }
            offset += in_addr_len;
        }
    } else if ((atyp & ADDRTYPE_MASK) == 3) {
        // Domain name
        uint8_t name_len = *(uint8_t*)(buf + offset);
        if (name_len + 4 <= buf_len) {
            if (storage != nullptr) {
                char tmp[256] = { 0 };
                memcpy(tmp, buf + offset + 1, name_len);
                if (uv_ip4_addr(tmp, 0, reinterpret_cast<sockaddr_in*>(storage)) == 0) {
                    auto addr = reinterpret_cast<sockaddr_in*>(storage);
                    memcpy(&addr->sin_port, buf + offset + 1 + name_len, sizeof(uint16_t));
                }
                if (uv_ip6_addr(tmp, 0, reinterpret_cast<sockaddr_in6*>(storage)) == 0) {
                    auto addr = reinterpret_cast<sockaddr_in6*>(storage);
                    memcpy(&addr->sin6_port, buf + offset + 1 + name_len, sizeof(uint16_t));
                }
            }
            if (host != nullptr) {
                memcpy(host, buf + offset + 1, name_len);
            }
            offset += 1 + name_len;
        }
    } else if ((atyp & ADDRTYPE_MASK) == 4) {
        // IP V6
        size_t in6_addr_len = sizeof(struct in6_addr);
        if (buf_len >= in6_addr_len + 3) {
            if (storage != nullptr) {
                auto* addr = (struct sockaddr_in6*)storage;
                addr->sin6_family = AF_INET6;
                memcpy(&addr->sin6_addr, buf + offset, sizeof(struct in6_addr));
                memcpy(&addr->sin6_port, buf + offset + in6_addr_len, sizeof(uint16_t));
            }
            if (host != nullptr) {
                uv_inet_ntop(AF_INET6, (const void*)(buf + offset),
                    host, INET6_ADDRSTRLEN);
            }
            offset += in6_addr_len;
        }
    }

    if (offset == 1) {
        LOGE("[udp] invalid header with addr type %d", atyp);
        return 0;
    }

    if (port != nullptr) {
        sprintf(port, "%d", load16_be(buf + offset));
    }
    offset += 2;

    return offset;
}
