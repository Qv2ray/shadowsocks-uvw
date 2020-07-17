#ifndef SSTHREAD_HPP
#define SSTHREAD_HPP
#include <QThread>
class TCPRelay;
class SSThread : public QThread
{
    Q_OBJECT
public:
    enum class SSR_WORK_MODE { TCP_ONLY = 0,
        TCP_AND_UDP = 1 };
    explicit SSThread() = default;
    explicit SSThread(int localPort,
        int remotePort,
        std::string local_addr,
        std::string remote_host,
        std::string method,
        std::string password,
        std::string plugin,
        std::string plugin_opts,
        std::string key);
    explicit SSThread(int localPort,
        int remotePort,
        int timeout,
        int mtu,
        SSR_WORK_MODE mode,
        std::string local_addr,
        std::string remote_host,
        std::string method,
        std::string password,
        std::string plugin,
        std::string plugin_opts,
        std::string key,
        int ipv6first = 0,
        int verbose = 0);
    ~SSThread() override;
signals:
    void OnDataReady(quint64 dataUp, quint64 dataDown);
    void onSSRThreadLog(QString);

protected:
    void run() override;

public slots:
    void stop();

private:
    int localPort = 0;
    int remotePort = 0;
    int timeout = 60000; //ms
    int mtu = 0;
    int mode = 0;
    std::string local_addr;
    std::string remote_host;
    std::string method;
    std::string password;
    std::string plugin;
    std::string plugin_opts;
    std::string key;
    int ipv6first = 0;
    int verbose = 0;
    std::shared_ptr<TCPRelay> tcpRelay;
};
#endif // SSRTHREAD_HPP
