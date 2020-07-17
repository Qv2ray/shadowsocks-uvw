#include "ConnectionContext.hpp"

#include "Buffer.hpp"
#include "LogHelper.h"
#include "uvw/tcp.h"
namespace
{
void dummyDisposeEncCtx(cipher_ctx_t*)
{
}

} // namespace

ConnectionContext::ConnectionContext(std::shared_ptr<uvw::TCPHandle> tcpHandle, CipherEnv* cipherEnvPtr)
    : cipherEnvPtr(cipherEnvPtr)
    , localBuf { new Buffer }
    , e_ctx { nullptr, dummyDisposeEncCtx }
    , d_ctx { nullptr, dummyDisposeEncCtx }
    , client(std::move(tcpHandle))
{
}

ConnectionContext::ConnectionContext()
    : localBuf {}
    , e_ctx { nullptr, dummyDisposeEncCtx }
    , d_ctx {
        nullptr, dummyDisposeEncCtx
    }
{
}

ConnectionContext::ConnectionContext(ConnectionContext&& that) noexcept
    : obfsClassPtr(that.obfsClassPtr)
    , cipherEnvPtr(that.cipherEnvPtr)
    , localBuf { std::move(that.localBuf) }
    , remoteBuf { std::move(that.remoteBuf) }
    , e_ctx { std::move(that.e_ctx) }
    , d_ctx { std::move(
          that.d_ctx) }
    , client(std::move(that.client))
    , remote(std::move(that.remote))
{
}

ConnectionContext& ConnectionContext::operator=(ConnectionContext&& that) noexcept
{

    localBuf = std::move(that.localBuf);
    remoteBuf = std::move(that.remoteBuf);
    e_ctx = std::move(that.e_ctx);
    d_ctx = std::move(that.d_ctx);
    client = std::move(that.client);
    remote = std::move(that.remote);
    obfsClassPtr = that.obfsClassPtr;
    cipherEnvPtr = that.cipherEnvPtr;
    return *this;
}

void ConnectionContext::setRemoteTcpHandle(std::shared_ptr<uvw::TCPHandle> tcp)
{
    remote = std::move(tcp);
}

void ConnectionContext::construct_cipher(CipherEnv& cipherEnv)
{
    if (cipherEnv.crypto) {
        auto crypto = cipherEnv.crypto;
        auto encCtxRelease = [this, crypto](cipher_ctx_t* p) {
            if (p == nullptr)
                return;
            crypto->ctx_release(p);
            free(p);
        };
        e_ctx = { reinterpret_cast<cipher_ctx_t*>(malloc(sizeof(cipher_ctx_t))), encCtxRelease };
        d_ctx = { reinterpret_cast<cipher_ctx_t*>(malloc(sizeof(cipher_ctx_t))), encCtxRelease };
        crypto->ctx_init(crypto->cipher, e_ctx.get(), 1);
        crypto->ctx_init(crypto->cipher, d_ctx.get(), 0);
    }
}

ConnectionContext::~ConnectionContext()
{
    if (remote) {
        remote->clear();
        remote->close();
    }
    if (client) {
        client->clear();
        client->close();
    }
}
