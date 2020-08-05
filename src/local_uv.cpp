#include "sockaddr_universal.h"
#include "ssrutils.h"
#include "uvw/loop.h"
#include "uvw/process.h"
#include "uvw/stream.h"
#include "uvw/tcp.h"
#include "uvw/timer.h"
#include "uvw/util.h"

#include <memory>
#include <unordered_map>
#include <utility>
#if defined(_WIN32)
#include <WS2tcpip.h>
#else
#include <netinet/in.h>
#endif // defined(_WIN32)
#include "Buffer.hpp"
#include "CipherEnv.hpp"
#include "ConnectionContext.hpp"
#include "NetUtils.hpp"
#include "TCPRelay.hpp"
#include "UDPRelay.hpp"
#include "shadowsocks.h"
#ifdef SSR_UVW_WITH_QT
#include "qt_ui_log.h"
#endif
#include <cstdint>

class TCPRelayImpl : public virtual TCPRelay
{
private:
    static constexpr int SVERSION = 0x05;
    std::shared_ptr<uvw::Loop> loop;
    std::shared_ptr<uvw::TimerHandle> stopTimer;
    std::shared_ptr<uvw::ProcessHandle> pluginProcess;
    uint16_t pluginPort = 0;
#ifdef SSR_UVW_WITH_QT
    std::shared_ptr<uvw::TimerHandle> statisticsUpdateTimer;
#endif
    std::shared_ptr<uvw::TCPHandle> tcpServer;
    std::unique_ptr<UDPRelay> udpRelay;
    bool isStop = false;
    bool verbose = false;
    profile_t profile {};
    bool acl = false;
    socks5_address address {};
    std::unique_ptr<CipherEnv> cipherEnv;
    uint64_t tx = 0, rx = 0;
    uint64_t last_tx = 0, last_rx = 0;
    sockaddr_storage remoteAddr {};
    std::unordered_map<std::shared_ptr<uvw::TCPHandle>, std::shared_ptr<ConnectionContext>> inComingConnections;
    double last {};

private:
    void stat_update_cb()
    {
#ifdef SSR_UVW_WITH_QT
        auto diff_tx = tx - last_tx;
        auto diff_rx = rx - last_rx;
        send_traffic_stat(diff_tx, diff_rx);
        last_tx = tx;
        last_rx = rx;
#endif
    }

public:
    TCPRelayImpl() = default;
    static TCPRelayImpl& getDefaultInstance()
    {
        static TCPRelayImpl instance;
        return instance;
    }

    static void stopDefaultInstance()
    {
        getDefaultInstance().stop();
    }

    void stop() override
    {
        isStop = true;
    }

    ~TCPRelayImpl() override
    {
        isStop = true;
    }

private:
    uint16_t getLocalPort()
    {
        auto tmpTCP = loop->resource<uvw::TCPHandle>();
        struct tmp_tcp
        {
            tmp_tcp(uvw::TCPHandle& h)
                : h(h)
            {
            }
            ~tmp_tcp() { h.close(); }
            uvw::TCPHandle& h;
        };
        tmp_tcp t { *tmpTCP };
        struct sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = 0;
        tmpTCP->bind(reinterpret_cast<const struct sockaddr&>(serv_addr));
        return tmpTCP->sock().port;
    }

    void startPlugin()
    {
        if (!profile.plugin)
            return;
        std::string ss_remote_host { "SS_REMOTE_HOST=" };
        std::string ss_remote_port { "SS_REMOTE_PORT=" };
        std::string ss_local_host { "SS_LOCAL_HOST=" };
        std::string ss_local_port { "SS_LOCAL_PORT=" };
        std::string plugin_opts;
        std::vector<char*> env;
        char digitBuffer[8] = { 0 };
        char* args[2] = { nullptr };

        pluginProcess = loop->resource<uvw::ProcessHandle>();
        using Process = uvw::ProcessHandle::Process;
        using StdioFlag = uvw::ProcessHandle::StdIO;
        pluginProcess->flags(uvw::Flags<Process>::from<Process::WINDOWS_HIDE, Process::WINDOWS_HIDE_CONSOLE>());
        pluginProcess->stdio(uvw::StdOUT, StdioFlag::IGNORE_STREAM);
        pluginProcess->stdio(uvw::StdERR, StdioFlag::IGNORE_STREAM);

        ss_remote_host += profile.remote_host;
        sprintf(digitBuffer, "%d", profile.remote_port);
        ss_remote_port += digitBuffer;
        ss_local_host += profile.local_addr;
        memset(digitBuffer, 0, sizeof(digitBuffer));
        pluginPort = getLocalPort();
        sprintf(digitBuffer, "%d", pluginPort);
        ss_local_port += digitBuffer;
        env.push_back(const_cast<char*>(ss_remote_host.c_str()));
        env.push_back(const_cast<char*>(ss_remote_port.c_str()));
        env.push_back(const_cast<char*>(ss_local_host.c_str()));
        env.push_back(const_cast<char*>(ss_local_port.c_str()));
        if (profile.plugin_opts) {
            plugin_opts += "SS_PLUGIN_OPTIONS=";
            plugin_opts += profile.plugin_opts;
            env.push_back(const_cast<char*>(plugin_opts.c_str()));
        }
        args[0] = const_cast<char*>(profile.plugin);
        pluginProcess->once<uvw::ErrorEvent>([](uvw::ErrorEvent& e, uvw::ProcessHandle& h) {
            LOGE("%s", e.what());
            h.close();
        });
        pluginProcess->once<uvw::ExitEvent>([](uvw::ExitEvent& e, uvw::ProcessHandle& h) {
            LOGI("Accept signal:%d,exit status:%d", e.signal, (int)e.status);
            h.close();
        });
        //get parent path and add cwd to path
#ifndef _WIN32
        std::string path { "PATH=" + uvw::Utilities::OS::env("PATH") + ":" + uvw::Utilities::cwd() };
        env.push_back(const_cast<char*>(path.c_str()));
#endif
        LOGI("plugin \"%s\" enabled", profile.plugin);
        pluginProcess->spawn(profile.plugin, args, env.data());
    }

    void handShakeReceive(const uvw::DataEvent& event, uvw::TCPHandle& client)
    {
        if (event.data[0] == 0x05 && event.length > 1) {
            auto dataWrite = std::unique_ptr<char[]>(new char[2] { SVERSION, 0 });
            client.write(std::move(dataWrite), 2);
            client.once<uvw::DataEvent>([this](auto& e, auto& h) { handShakeSendCallBack(e, h); });
            return;
        } else if (event.length > 1) {
            auto dataWrite = std::unique_ptr<char[]>(new char[2] { SVERSION, 0 });
            client.write(std::move(dataWrite), 2);
        }
        client.close();
    }

    void readAllAddress(uvw::DataEvent& event, uvw::TCPHandle& client)
    {
        ConnectionContext& connectionContext = *inComingConnections[client.shared_from_this()];
        Buffer& buf = *connectionContext.localBuf;
        buf.copy(event);
        if (socks5_address_parse((uint8_t*)buf.begin() + 3, buf.length() - 3, &address)) {
            buf.drop(3);
            connectionContext.construct_cipher(*cipherEnv);
            startConnect(client);
        } else {
            client.once<uvw::DataEvent>([this](auto& e, auto& h) { readAllAddress(e, h); });
        }
    }

    void handShakeSendCallBack(uvw::DataEvent& event, uvw::TCPHandle& client)
    {
        int cmd;
        ConnectionContext& connectionContext = *inComingConnections[client.shared_from_this()];
        Buffer& buf = *connectionContext.localBuf;
        if (buf.length() + event.length >= 5) {
            // VER 	CMD 	RSV 	ATYP 	DST.ADDR 	DST.PORT
            // 1 	1 	    0x00 	1 	      动态 	     2
            switch (buf.length()) {
            case 0:
                cmd = event.data[1];
                break;
            case 1:
                cmd = event.data[0];
                break;
            default:
                cmd = buf[1];
                break;
            }
            buf.copy(event); // buf never equal to zero
            switch (cmd) {
            case 0x01:
                if (buf.length() != 0 && socks5_address_parse((uint8_t*)buf.begin() + 3, buf.length() - 3, &address)) {
                    buf.drop(3);
                    connectionContext.construct_cipher(*cipherEnv);
                    startConnect(client);
                } else {
                    client.once<uvw::DataEvent>([this](auto& e, auto& h) { readAllAddress(e, h); });
                    return;
                }
                break;
            case 0x03:
                udpAsscResponse(client);
                break;
            case 0x02:
            default:
                client.close();
                break;
            }
        } else {
            // shall we just close it?
            buf.copy(event);
            client.once<uvw::DataEvent>([this](auto& e, auto& h) { handShakeSendCallBack(e, h); });
        }
    }

    void udpAsscResponse(uvw::TCPHandle& client) const
    {
        uvw::details::IpTraits<uvw::IPv4>::Type addr;
        uvw::details::IpTraits<uvw::IPv4>::addrFunc(profile.local_addr, profile.local_port, &addr);
        constexpr unsigned int response_length = 4 + sizeof(addr.sin_addr) + sizeof(addr.sin_port);
        auto response = std::make_unique<char[]>(response_length);
        response[0] = 0x05;
        response[1] = 0x00;
        response[2] = 0x00;
        response[3] = 0x01;
        memcpy(response.get() + 4, &addr.sin_addr, sizeof(addr.sin_addr));
        memcpy(response.get() + 4 + sizeof(addr.sin_addr), &addr.sin_port, sizeof(addr.sin_port));
        client.write(std::move(response), response_length);
    }

    void panic(const std::shared_ptr<uvw::TCPHandle>& clientConnection)
    {
        if (verbose)
            LOGI("panic close client connection");
        if (inComingConnections.find(clientConnection) != inComingConnections.end()) {
            inComingConnections.erase(clientConnection);
        }
    }
    void sockStream(uvw::DataEvent& event, uvw::TCPHandle& client)
    {
        if (client.closing())
            return;
        auto clientPtr = client.shared_from_this();
        if (inComingConnections.find(clientPtr) == inComingConnections.end()) {
            return;
        }
        auto connectionContextPtr = inComingConnections[clientPtr];
        auto& connectionContext = *connectionContextPtr;
        Buffer& buf = *connectionContext.remoteBuf;
        buf.copy(event);
        tx += buf.length();
        int err = buf.ssEncrypt(*cipherEnv, connectionContext);
        if (err) {
            panic(clientPtr);
            return;
        }
        if (buf.length() != 0) {
            connectionContext.remote->write(buf.duplicateDataToArray(), buf.length());
            buf.clear();
            return;
        }
    }
    void remoteRecv(ConnectionContext& ctx, uvw::DataEvent& event, uvw::TCPHandle& remote)
    {
        if (remote.closing()) {
            return;
        }
        rx += event.length;
        auto& buf = *ctx.localBuf;
        buf.copy(event);
        int err = buf.ssDecrypt(*cipherEnv, ctx);
        if (err == CRYPTO_ERROR) {
            panic(ctx.client);
            return;
        } else if (err == CRYPTO_NEED_MORE) {
            return;
        }
        ctx.client->write(buf.duplicateDataToArray(), buf.length());
        buf.clear();
    }

    void connectRemote(ConnectionContext& ctx)
    {
        auto remote = ctx.remote;
        if (!remote)
            return;
        remote->connect(reinterpret_cast<const sockaddr&>(remoteAddr));
        remote->on<uvw::DataEvent>([&ctx, this](uvw::DataEvent& event, uvw::TCPHandle& remoteHandle) { remoteRecv(ctx, event, remoteHandle); });
        remote->once<uvw::ConnectEvent>([&ctx, this](const uvw::ConnectEvent&, uvw::TCPHandle& h) {
            h.read();
            ctx.client->write(std::unique_ptr<char[]>(new char[10] { 5, 0, 0, 1, 0, 0, 0, 0, 0, 0 }), 10);
            ctx.remoteBuf = std::make_unique<Buffer>();
            ctx.remoteBuf->copy(*ctx.localBuf);
            ctx.localBuf->clear();
            int err = ctx.remoteBuf->ssEncrypt(*cipherEnv, ctx);
            if (err) {
                panic(ctx.client);
                return;
            }
            ctx.remote->once<uvw::WriteEvent>([&ctx, this](auto&, auto&) {
                ctx.client->on<uvw::DataEvent>([this](uvw::DataEvent& event, uvw::TCPHandle& client) {
                    // when this event traiggered, we are in stream mode.
                    sockStream(event, client);
                });
                ctx.remoteBuf->clear();
            });
            ctx.remote->write(ctx.remoteBuf->begin(), ctx.remoteBuf->length());
            // stop remote send and start local recv
        });
    }
    void startConnect(uvw::TCPHandle& client)
    {
        auto clientPtr = client.shared_from_this();
        auto& connectionContext = *inComingConnections[clientPtr];
        if (acl) {
            // todo acl
        }
        auto remoteTcp = loop->resource<uvw::TCPHandle>();
        connectionContext.setRemoteTcpHandle(remoteTcp);
        // todo timer
        remoteTcp->once<uvw::ErrorEvent>([clientPtr, this](const uvw::ErrorEvent& e, uvw::TCPHandle&) {
            LOGE("remote error %s", e.what());
            panic(clientPtr);
        });
        remoteTcp->once<uvw::CloseEvent>([clientPtr, this](const uvw::CloseEvent&, uvw::TCPHandle&) {
            if (verbose)
                LOGI("remote close");
            panic(clientPtr);
        });
        remoteTcp->once<uvw::EndEvent>([clientPtr, this](const uvw::EndEvent&, uvw::TCPHandle&) {
            if (verbose)
                LOGI("remote end event");
            panic(clientPtr);
        });
        remoteTcp->noDelay(true);
        // fastopen is not implemented due to fastopen is still WIP
        // https://github.com/libuv/libuv/pull/1136
        connectRemote(connectionContext);
        // we send socks5 fake response after we real connected remote server;
    }

    int listen()
    {
        tcpServer = loop->resource<uvw::TCPHandle>();
        tcpServer->noDelay(true);
        tcpServer->on<uvw::ListenEvent>([this](const uvw::ListenEvent&, uvw::TCPHandle& srv) {
            std::shared_ptr<uvw::TCPHandle> client = srv.loop().resource<uvw::TCPHandle>();
            inComingConnections.emplace(std::make_pair(client, std::make_shared<ConnectionContext>(client, cipherEnv.get())));
            client->once<uvw::CloseEvent>([this](const uvw::CloseEvent&, uvw::TCPHandle& c) {
                auto clientPtr = c.shared_from_this();
                if (verbose)
                    LOGI("client close");
                panic(clientPtr);
            });
            client->once<uvw::ErrorEvent>([this](const uvw::ErrorEvent& e, uvw::TCPHandle& c) {
                auto clientPtr = c.shared_from_this();
                LOGE("client error %s", e.what());
                panic(clientPtr);
            });
            client->once<uvw::DataEvent>([this](const uvw::DataEvent& event, uvw::TCPHandle& client) { handShakeReceive(event, client); });
            srv.accept(*client);
            client->read();
        });
        sockaddr_storage localStorage {};
        if (ssr_get_sock_addr(loop, profile.local_addr, profile.local_port, &localStorage, 0) == -1) {
            LOGE("local socks server can't bind to %s:%d", profile.local_addr, profile.local_port);
            return -1;
        }
        tcpServer->bind(reinterpret_cast<const struct sockaddr&>(localStorage));
        tcpServer->listen();
        return 0;
    }

public:
    int loopMain(profile_t& p) override
    {
        verbose = p.verbose;
        profile = p;
        isStop = false;
        tx = rx = last_rx = last_tx = 0;
        loop = uvw::Loop::create();
#ifndef _WIN32
        signal(SIGPIPE, SIG_IGN);
#endif
        stopTimer = loop->resource<uvw::TimerHandle>();
        LOGI("listening at %s:%d", profile.local_addr, profile.local_port);
        cipherEnv = std::make_unique<CipherEnv>(profile.password, profile.method, profile.key);
        if (cipherEnv->crypto)
            LOGI("initializing ciphers...%s", profile.method);
        else {
            LOGI("initializing ciphers...%s failed", profile.method);
            return -1;
        }

#ifdef SSR_UVW_WITH_QT
        statisticsUpdateTimer = loop->resource<uvw::TimerHandle>();
        statisticsUpdateTimer->on<uvw::TimerEvent>([this](auto& e, auto& handle) {
            stat_update_cb();
        });
        statisticsUpdateTimer->start(uvw::TimerHandle::Time { 1000 }, uvw::TimerHandle::Time { 1000 });
#endif
        stopTimer->on<uvw::TimerEvent>([this, ssr_work_mode = p.mode](auto&, auto& handle) {
            if (isStop) {
                if (!tcpServer->closing()) {
#ifdef SSR_UVW_WITH_QT
                    statisticsUpdateTimer->stop();
                    statisticsUpdateTimer->close();
#endif
                    tcpServer->close();
                    inComingConnections.clear();
                    if (ssr_work_mode == 1) {
                        udpRelay.reset(nullptr);
                    }
                    if (pluginProcess) {
                        pluginProcess->kill(SIGTERM);
                    }
                }
                int timer_count = 0;
                uv_walk(
                    loop->raw(),
                    [](uv_handle_t* handle, void* arg) {
                        int& counter = *static_cast<int*>(arg);
                        if (uv_is_closing(handle) == 0)
                            counter++;
                    },
                    &timer_count);
                //only current timer
                if (timer_count != 1)
                    return;
                handle.stop();
                handle.close();
                loop->clear();
                loop->close();
                cipherEnv.reset(nullptr);
                loop->stop();
            }
        });
        stopTimer->start(uvw::TimerHandle::Time { 500 }, uvw::TimerHandle::Time { 500 });
        if (profile.plugin) {
            startPlugin();
            if (pluginProcess && pluginProcess->closing())
                return -1;
            if (!pluginPort)
                return -1;
        }
        if (ssr_get_sock_addr(loop,
                pluginPort ? profile.local_addr : profile.remote_host,
                pluginPort ? pluginPort : profile.remote_port,
                reinterpret_cast<struct sockaddr_storage*>(&remoteAddr),
                p.ipv6first)
            == -1)
            return -1;
        int res = 0;
        if (p.mode == 1) {
            udpRelay = std::make_unique<UDPRelay>(loop, *cipherEnv, profile);
            //udp relay need real remote not plugin
            struct sockaddr_storage realRemoteAddr;
            if (ssr_get_sock_addr(loop,
                    profile.remote_host,
                    profile.remote_port,
                    reinterpret_cast<struct sockaddr_storage*>(&realRemoteAddr),
                    p.ipv6first)
                == -1)
                return -1;
            res = udpRelay->initUDPRelay(p.mtu, p.local_addr, p.local_port, realRemoteAddr);
            LOGI("UDP relay enabled");
            if (res)
                return res;
        }
        res = listen();
        if (res)
            return res;
        loop->run();
        return 0;
    }
};

std::shared_ptr<TCPRelay> TCPRelay::create()
{
    return std::shared_ptr<TCPRelay> { new TCPRelayImpl };
}

int start_ssr_uv_local_server(profile_t profile)
{
    auto& ssr = TCPRelayImpl::getDefaultInstance();
    int res = ssr.loopMain(profile);
    if (res)
        return res;
    return 0;
}

int stop_ssr_uv_local_server()
{
    TCPRelayImpl::stopDefaultInstance();
    return 0;
}
