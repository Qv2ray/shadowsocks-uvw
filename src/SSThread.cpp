#include "SSThread.hpp"
#include "TCPRelay.hpp"
SSThread::SSThread(int localPort,
    int remotePort,
    std::string local_addr,
    std::string remote_host,
    std::string method,
    std::string password,
    std::string plugin,
    std::string plugin_opts,
    std::string key)
    : localPort(localPort)
    , remotePort(remotePort)
    , local_addr(std::move(local_addr))
    , remote_host(std::move(remote_host))
    , method(std::move(method))
    , password(std::move(password))
    , plugin(std::move(plugin))
    , plugin_opts(std::move(plugin_opts))
    , key(std::move(key))
    , tcpRelay(TCPRelay::create())
{
}

SSThread::SSThread(int localPort,
    int remotePort,
    int timeout,
    int mtu,
    SSR_WORK_MODE work_mode,
    std::string local_addr,
    std::string remote_host,
    std::string method,
    std::string password,
    std::string plugin,
    std::string plugin_opts,
    std::string key,
    int ipv6first,
    int verbose)
    : localPort(localPort)
    , remotePort(remotePort)
    , timeout(timeout)
    , mtu(mtu)
    , mode(static_cast<int>(work_mode))
    , local_addr(std::move(local_addr))
    , remote_host(std::move(remote_host))
    , method(std::move(method))
    , password(std::move(password))
    , plugin(std::move(plugin))
    , plugin_opts(std::move(plugin_opts))
    , key(std::move(key))
    , ipv6first(ipv6first)
    , verbose(verbose)
    , tcpRelay(TCPRelay::create())
{
}

SSThread::~SSThread()
{
    stop();
}

void SSThread::run()
{
    profile_t profile;
    profile.remote_host = remote_host.data();
    profile.local_addr = local_addr.empty() ? nullptr : local_addr.data();
    profile.method = method.data();
    profile.timeout = timeout;
    profile.password = password.data();
    profile.plugin = plugin.empty()?nullptr:plugin.data();
    profile.plugin_opts = plugin_opts.empty() ?nullptr:plugin_opts.data();
    profile.key=key.empty()?nullptr:key.data();
    profile.remote_port = remotePort;
    profile.local_port = localPort;
    profile.mtu = mtu;
    profile.mode = mode;
    profile.acl = nullptr;
    profile.fast_open = 1; // libuv is not supported fastopen yet.
    profile.verbose = verbose;
    profile.ipv6first = ipv6first;
    tcpRelay->loopMain(profile);
}

void SSThread::stop()
{
    if (isRunning()) {
        tcpRelay->stop();
        wait();
    }
}
