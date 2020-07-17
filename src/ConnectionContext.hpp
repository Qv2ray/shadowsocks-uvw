#ifndef CONNECTIONCONTEXT_H
#define CONNECTIONCONTEXT_H
#include <memory>
extern "C"
{
#include "crypto.h"
#include "shadowsocks.h"
}
#include "CipherEnv.hpp"
namespace uvw
{
class TCPHandle;
}
#include "Buffer.hpp"

#include <functional>
class ConnectionContext
{
private:
    ObfsClass* obfsClassPtr = nullptr;
    CipherEnv* cipherEnvPtr = nullptr;

public:
    using cihper_ctx_release_t = std::function<void(cipher_ctx_t*)>;
    std::unique_ptr<Buffer> localBuf;
    std::unique_ptr<Buffer> remoteBuf;
    std::unique_ptr<cipher_ctx_t, cihper_ctx_release_t> e_ctx;
    std::unique_ptr<cipher_ctx_t, cihper_ctx_release_t> d_ctx;
    std::shared_ptr<uvw::TCPHandle> client;
    std::shared_ptr<uvw::TCPHandle> remote;

    ConnectionContext(std::shared_ptr<uvw::TCPHandle> tcpHandle, CipherEnv* cipherEnvPtr);

    ConnectionContext();

    ConnectionContext(ConnectionContext&&) noexcept;

    ConnectionContext& operator=(ConnectionContext&&) noexcept;

    void construct_cipher(CipherEnv& cipherEnv);
    void setRemoteTcpHandle(std::shared_ptr<uvw::TCPHandle> tcp);

    ~ConnectionContext();
};

#endif // CONNECTIONCONTEXT_H
