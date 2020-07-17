#ifndef SHADOWSOCKS_UVW_SHADOWSOCKS_H
#define SHADOWSOCKS_UVW_SHADOWSOCKS_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        /*  Required  */
        const char* remote_host; // hostname or ip of remote server
        const char* local_addr; // local ip to bind
        const char* method; // encryption method
        const char* password; // password of remote server
        int remote_port; // port number of remote server
        int local_port; // port number of local server
        int timeout; // connection timeout
        const char* plugin; // ssr
        const char* plugin_opts; // ssr
        const char* key;
        /*  Optional, set NULL if not valid   */
        const char* acl; // file path to acl
        int fast_open; // enable tcp fast open
        int mode; // enable udp relay
        // mode 0 is TCP_ONLY
        // mode 1 is TCP_AND_UDP
        int mtu; // MTU of interface
        int verbose; // verbose mode
        int ipv6first;
    } profile_t;

    int start_ssr_uv_local_server(profile_t profile);
    int stop_ssr_uv_local_server();
#ifdef __cplusplus
}
#endif

#endif // SHADOWSOCKSR_UVW_SHADOWSOCKSR_H
