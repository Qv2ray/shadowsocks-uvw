#if !defined(_WIN32)
#include <getopt.h>
#include <unistd.h>
#else
#include "win/getopt.h"
#endif
#include "shadowsocks.h"
#include "signal.h"
#include "ssrutils.h"

static void usage()
{
    printf("\n");
    printf("shadowsocks-uvw \n\n");
    printf(
        "  maintained by DuckVador <Lx3JQkmzRS@protonmail.com>\n\n");
    printf("  usage:\n\n");
    printf("    ssr-local\n");
    printf("\n");
    printf(
        "       -s <server_host>           Host name or IP address of your remote server.\n");
    printf(
        "       -p <server_port>           Port number of your remote server.\n");
    printf(
        "       [-6]                       Resovle hostname to IPv6 address first.\n");
    printf(
        "       -b <local_address>         Local address to bind.\n");
    printf(
        "       -l <local_port>            Port number of your local server.\n");
    printf(
        "       -k <password>              Password of your remote server.\n");
    printf(
        "       -m <encrypt_method>        Encrypt method: rc4-md5, \n");
    printf(
        "                                  aes-128-gcm, aes-192-gcm, aes-256-gcm,\n");
    printf(
        "                                  aes-128-cfb, aes-192-cfb, aes-256-cfb,\n");
    printf(
        "                                  aes-128-ctr, aes-192-ctr, aes-256-ctr,\n");
    printf(
        "                                  camellia-128-cfb, camellia-192-cfb,\n");
    printf(
        "                                  camellia-256-cfb, bf-cfb,\n");
    printf(
        "                                  chacha20-ietf-poly1305,\n");
    printf(
        "                                  xchacha20-ietf-poly1305,\n");
    printf(
        "                                  salsa20, chacha20 and chacha20-ietf.\n");
    printf(
        "                                  The default cipher is chacha20-ietf-poly1305.\n");
    printf("\n");
    printf(
        "       [-t <timeout>]             Socket timeout in seconds.\n");
    printf("\n");
    printf(
        "       [-u]                       Enable UDP relay.\n");
    //    printf(
    //        "       [-U]                       Enable UDP relay and disable TCP relay.\n");
    printf("\n");
    printf(
        "       [--mtu <MTU>]              MTU of your network interface.\n");
    printf("\n");
    printf(
        "       [--plugin <name>]          Enable SIP003 plugin. (Experimental)\n");
    printf(
        "       [--plugin-opts <options>]  Set SIP003 plugin options. (Experimental)\n");
    printf("\n");
    printf(
        "       [-v]                       Verbose mode.\n");
    printf(
        "       [-h, --help]               Print this message.\n");
    printf("\n");
}
void sigintHandler(int sig_num)
{
    /* Reset handler to catch SIGINT next time.
       Refer http://en.cppreference.com/w/c/program/signal */
    signal(SIGINT, sigintHandler);
    stop_ssr_uv_local_server();
    LOGI("waiting main loop to exit");
    fflush(stdout);
}
enum {
    GETOPT_VAL_HELP = 257,
    GETOPT_VAL_MTU,
    GETOPT_VAL_PLUGIN,
    GETOPT_VAL_PLUGIN_OPTS,
    GETOPT_VAL_PASSWORD,
    GETOPT_VAL_KEY,
};

int main(int argc, char** argv)
{
    int c;
    int option_index = 0;
    profile_t p {};
    p.method = "chacha20-ietf-poly1305";
    p.local_addr = "0.0.0.0";
    p.remote_host = "127.0.0.1";
    p.remote_port = 0;
    p.timeout = 60000;
    p.mtu = 1500;
    p.plugin = nullptr;
    p.plugin_opts = nullptr;
    p.password = "shadowsocksr-uvw";
    opterr = 0;
    static struct option long_options[] = {
        { "mtu",         required_argument, NULL, GETOPT_VAL_MTU         },
        { "plugin",      required_argument, NULL, GETOPT_VAL_PLUGIN      },
        { "plugin-opts", required_argument, NULL, GETOPT_VAL_PLUGIN_OPTS },
        { "password",    required_argument, NULL, GETOPT_VAL_PASSWORD    },
        { "key",         required_argument, NULL, GETOPT_VAL_KEY         },
        { "help",        no_argument,       NULL, GETOPT_VAL_HELP        },
        { nullptr, 0, nullptr, 0 }
    };
    while ((c = getopt_long(argc, argv, "s:p:l:k:t:m:i:b:L::huvA6",
                long_options, &option_index))
        != -1) {
        switch (c) {
        case GETOPT_VAL_MTU:
            p.mtu=atoi(optarg);
            break;
        case GETOPT_VAL_PLUGIN:
            p.plugin=optarg;
            break;
        case GETOPT_VAL_PLUGIN_OPTS:
            p.plugin_opts=optarg;
            break;
        case GETOPT_VAL_PASSWORD:
            p.password=optarg;
            break;
        case GETOPT_VAL_KEY:
            p.key=optarg;
            break;
        case 's':
            p.remote_host = optarg;
            break;
        case 'p':
            p.remote_port = atoi(optarg);
            break;
        case 'l':
            p.local_port = atoi(optarg);
            break;
        case 'k':
            p.password = optarg;
            break;
        case 't':
            p.timeout = atoi(optarg) * 1000;
            break;
        case 'm':
            p.method = optarg;
            break;
        case '6':
            p.ipv6first = 1;
            break;
        case 'b':
            p.local_addr = optarg;
            break;
        case 'u':
            p.mode = 1;
            break;
        case 'v':
            p.verbose = 1;
            break;
        case GETOPT_VAL_HELP:
        case 'h':
            usage();
            exit(EXIT_SUCCESS);
        case '?':
            // The option character is not recognized.
            LOGE("Unrecognized option: %s", optarg);
            opterr = 1;
            break;
        }
    }
    if (p.local_port == 0 || p.remote_port == 0) {
        opterr = 1;
    }
    if (opterr) {
        usage();
        exit(EXIT_FAILURE);
    }
    USE_TTY();
    signal(SIGINT, sigintHandler);
    start_ssr_uv_local_server(p);
    return 0;
}
