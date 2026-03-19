#include "wfr_lirc.h"
#include "wfr_decode.h"
#include "wfr_network.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <stdbool.h>
#include <getopt.h>

static volatile bool running = true;

static void signal_handler(int sig) {
    (void)sig;
    running = false;
}

static void print_usage(const char* prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -d, --device PATH   LIRC device (default: /dev/lirc0)\n");
    fprintf(stderr, "  -f, --foreground     Run in foreground (don't daemonize)\n");
    fprintf(stderr, "  -v, --verbose        Verbose logging\n");
    fprintf(stderr, "  -s, --stdin          Read ir-ctl format from stdin instead of LIRC device\n");
    fprintf(stderr, "  -h, --help           Show this help\n");
}

/* Parse one ir-ctl output token like "+8956" or "-4467" */
static bool parse_irctl_token(const char* token, bool* is_pulse, uint32_t* duration_us) {
    if(token[0] == '+') {
        *is_pulse = true;
        *duration_us = (uint32_t)atoi(token + 1);
        return true;
    } else if(token[0] == '-') {
        *is_pulse = false;
        *duration_us = (uint32_t)atoi(token + 1);
        return true;
    }
    return false; /* skip comments, overflow markers, etc. */
}

static void handle_credentials(const char* payload) {
    syslog(LOG_INFO, "received credentials: %s", payload);

    WfrWifiCreds creds;
    if(!wfr_parse_wifi_string(payload, &creds)) {
        syslog(LOG_ERR, "failed to parse WiFi string: %s", payload);
        return;
    }

    syslog(
        LOG_INFO,
        "parsed: SSID=%s, security=%d, hidden=%d",
        creds.ssid,
        creds.security,
        creds.hidden);

    /* Save current connection for rollback */
    char prev_ssid[WFR_SSID_MAX_LEN + 1] = {0};
    wfr_net_get_current(prev_ssid, sizeof(prev_ssid));

    if(prev_ssid[0]) {
        syslog(LOG_INFO, "current connection: %s (saved for rollback)", prev_ssid);
    }

    /* Attempt connection */
    if(wfr_net_connect(&creds)) {
        syslog(LOG_INFO, "successfully connected to %s", creds.ssid);
    } else {
        syslog(LOG_ERR, "failed to connect to %s", creds.ssid);
        if(prev_ssid[0]) {
            syslog(LOG_INFO, "attempting rollback to %s", prev_ssid);
            if(wfr_net_rollback(prev_ssid)) {
                syslog(LOG_INFO, "rollback successful");
            } else {
                syslog(LOG_ERR, "rollback failed — manual intervention may be needed");
            }
        }
    }
}

static int run_stdin_mode(int log_level) {
    (void)log_level;
    WfrDecoder decoder;
    wfr_decode_init(&decoder);
    char payload[WFR_MAX_TOTAL_PAYLOAD + 1];
    char line[4096];

    syslog(LOG_INFO, "wifird reading from stdin (ir-ctl format)");

    while(running && fgets(line, sizeof(line), stdin)) {
        /* Detect overflow lines — reset decoder frame state */
        if(strstr(line, "# overflow") || strstr(line, "#overflow")) {
            wfr_decode_overflow(&decoder);
        }

        /* Tokenize: ir-ctl output is "+1234 -5678 +..." on one line */
        char* token = strtok(line, " \t\n\r");
        while(token && running) {
            bool is_pulse;
            uint32_t duration_us;

            if(parse_irctl_token(token, &is_pulse, &duration_us)) {
                int result = wfr_decode_feed(
                    &decoder, is_pulse, duration_us, payload, sizeof(payload));
                if(result > 0) {
                    handle_credentials(payload);
                }
            }
            token = strtok(NULL, " \t\n\r");
        }
    }
    return 0;
}

static int run_lirc_mode(const char* device) {
    int lirc_fd = wfr_lirc_open(device);
    if(lirc_fd < 0) {
        syslog(LOG_ERR, "failed to open LIRC device %s", device);
        return 1;
    }

    WfrDecoder decoder;
    wfr_decode_init(&decoder);
    char payload[WFR_MAX_TOTAL_PAYLOAD + 1];

    syslog(LOG_INFO, "wifird ready, listening for IR transmissions");

    while(running) {
        bool is_pulse;
        uint32_t duration_us;

        if(wfr_lirc_read(lirc_fd, &is_pulse, &duration_us) < 0) {
            if(!running) break;
            continue;
        }

        int result = wfr_decode_feed(&decoder, is_pulse, duration_us, payload, sizeof(payload));
        if(result > 0) {
            handle_credentials(payload);
        }
    }

    wfr_lirc_close(lirc_fd);
    return 0;
}

int main(int argc, char* argv[]) {
    const char* device = "/dev/lirc0";
    bool foreground = false;
    bool use_stdin = false;
    int log_level = LOG_INFO;

    static struct option long_opts[] = {
        {"device", required_argument, NULL, 'd'},
        {"foreground", no_argument, NULL, 'f'},
        {"verbose", no_argument, NULL, 'v'},
        {"stdin", no_argument, NULL, 's'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0},
    };

    int opt;
    while((opt = getopt_long(argc, argv, "d:fvsh", long_opts, NULL)) != -1) {
        switch(opt) {
        case 'd':
            device = optarg;
            break;
        case 'f':
            foreground = true;
            break;
        case 'v':
            log_level = LOG_DEBUG;
            break;
        case 's':
            use_stdin = true;
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    openlog("wifird", LOG_PID | (foreground ? LOG_PERROR : 0), LOG_DAEMON);
    setlogmask(LOG_UPTO(log_level));

    syslog(LOG_INFO, "wifird starting (%s)", use_stdin ? "stdin" : device);

    struct sigaction sa = {0};
    sa.sa_handler = signal_handler;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    int ret;
    if(use_stdin) {
        ret = run_stdin_mode(log_level);
    } else {
        ret = run_lirc_mode(device);
    }

    syslog(LOG_INFO, "wifird shutting down");
    closelog();
    return ret;
}
