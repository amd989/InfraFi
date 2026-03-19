#include "wfr_network.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>

/* Shell-escape a string for safe use in shell commands.
 * Wraps in single quotes and escapes embedded single quotes.
 * Returns bytes written (excluding null), or 0 on overflow. */
static size_t shell_escape(const char* src, char* dst, size_t dst_size) {
    size_t pos = 0;

    if(pos >= dst_size) return 0;
    dst[pos++] = '\'';

    for(size_t i = 0; src[i]; i++) {
        if(src[i] == '\'') {
            /* End quote, escaped quote, start quote: '\'' */
            if(pos + 4 >= dst_size) return 0;
            dst[pos++] = '\'';
            dst[pos++] = '\\';
            dst[pos++] = '\'';
            dst[pos++] = '\'';
        } else {
            if(pos + 1 >= dst_size) return 0;
            dst[pos++] = src[i];
        }
    }

    if(pos + 1 >= dst_size) return 0;
    dst[pos++] = '\'';
    dst[pos] = '\0';
    return pos;
}

/* Run a command and return its exit status. */
static int run_cmd(const char* cmd) {
    syslog(LOG_DEBUG, "wifird: exec: %s", cmd);
    int status = system(cmd);
    if(status == -1) {
        syslog(LOG_ERR, "wifird: system() failed for: %s", cmd);
        return -1;
    }
    return WEXITSTATUS(status);
}

/* Run a command and capture first line of stdout. */
static bool run_cmd_output(const char* cmd, char* out, size_t out_size) {
    FILE* fp = popen(cmd, "r");
    if(!fp) {
        syslog(LOG_ERR, "wifird: popen() failed for: %s", cmd);
        return false;
    }

    bool got_line = false;
    if(fgets(out, (int)out_size, fp)) {
        /* Strip trailing newline */
        size_t len = strlen(out);
        if(len > 0 && out[len - 1] == '\n') out[len - 1] = '\0';
        got_line = true;
    }

    pclose(fp);
    return got_line;
}

/* Verify network connectivity by resolving a hostname. */
static bool verify_connectivity(void) {
    struct addrinfo hints = {0};
    struct addrinfo* result = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int err = getaddrinfo("connectivitycheck.gstatic.com", "80", &hints, &result);
    if(err == 0 && result) {
        freeaddrinfo(result);
        return true;
    }
    return false;
}

/*
 * Check WPA association state via wpa_cli.
 * Returns:
 *   "COMPLETED"    — authenticated and associated
 *   "DISCONNECTED" — not connected
 *   "4WAY_HANDSHAKE" / "ASSOCIATING" — still working
 *   "INACTIVE"     — interface down or no network
 *   NULL           — wpa_cli not available or parse error
 */
static const char* get_wpa_state(const char* iface) {
    static char state[64];
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "wpa_cli -i %s status 2>/dev/null | grep '^wpa_state=' | cut -d= -f2", iface);

    if(run_cmd_output(cmd, state, sizeof(state)) && state[0]) {
        return state;
    }
    return NULL;
}

/*
 * Wait for WPA association with retries.
 * Polls wpa_cli status every 2 seconds, up to max_wait seconds.
 * Returns: 1 = COMPLETED, 0 = timeout/still trying, -1 = auth failure
 */
static int wait_for_wpa_association(const char* iface, int max_wait) {
    for(int elapsed = 0; elapsed < max_wait; elapsed += 2) {
        sleep(2);
        const char* state = get_wpa_state(iface);

        if(!state) {
            syslog(LOG_DEBUG, "wifird: wpa_cli not available, falling back to connectivity check");
            return 0; /* can't determine state, caller should check connectivity */
        }

        syslog(LOG_DEBUG, "wifird: wpa_state=%s (%ds elapsed)", state, elapsed + 2);

        if(strcmp(state, "COMPLETED") == 0) {
            syslog(LOG_INFO, "wifird: WPA association completed");
            return 1;
        }

        /* These states mean auth definitively failed */
        if(strcmp(state, "DISCONNECTED") == 0 && elapsed > 6) {
            syslog(LOG_ERR, "wifird: WPA disconnected — wrong password or network not found");
            return -1;
        }
    }

    syslog(LOG_WARNING, "wifird: WPA association timed out after %ds", max_wait);
    return 0;
}

/*
 * Wait for DHCP to assign an IP address.
 * Checks the interface for a non-link-local IP every 2 seconds.
 * Returns true if an IP was obtained, false on timeout.
 */
static bool wait_for_dhcp(const char* iface, int max_wait) {
    char cmd[256];
    char ip[64];

    for(int elapsed = 0; elapsed < max_wait; elapsed += 2) {
        sleep(2);
        snprintf(cmd, sizeof(cmd),
            "ip -4 addr show %s 2>/dev/null | grep -oP 'inet \\K[0-9.]+'", iface);

        if(run_cmd_output(cmd, ip, sizeof(ip)) && ip[0]) {
            /* Ignore link-local (169.254.x.x) */
            if(strncmp(ip, "169.254.", 8) != 0) {
                syslog(LOG_INFO, "wifird: DHCP obtained IP %s on %s", ip, iface);
                return true;
            }
        }

        syslog(LOG_DEBUG, "wifird: waiting for DHCP... (%ds elapsed)", elapsed + 2);
    }

    syslog(LOG_WARNING, "wifird: DHCP timed out after %ds on %s", max_wait, iface);
    return false;
}

/*
 * Detect the WiFi interface name.
 * Tries common names and falls back to scanning /sys/class/net.
 */
static bool detect_wifi_iface(char* out, size_t out_size) {
    /* Scan /sys/class/net for any interface with a wireless directory */
    char iface[32];
    if(run_cmd_output(
           "for d in /sys/class/net/*/wireless; do [ -d \"$d\" ] && basename \"$(dirname \"$d\")\" && break; done",
           iface, sizeof(iface)) && iface[0]) {
        strncpy(out, iface, out_size - 1);
        out[out_size - 1] = '\0';
        return true;
    }

    syslog(LOG_ERR, "wifird: no WiFi interface found");
    return false;
}

bool wfr_net_has_networkmanager(void) {
    if(run_cmd("which nmcli >/dev/null 2>&1") != 0) return false;
    return run_cmd("systemctl is-active --quiet NetworkManager") == 0;
}

bool wfr_net_get_current(char* out, size_t out_size) {
    if(wfr_net_has_networkmanager()) {
        char line[256];
        if(run_cmd_output(
               "nmcli -t -f active,ssid dev wifi 2>/dev/null | grep '^yes:'",
               line,
               sizeof(line))) {
            char* ssid = strchr(line, ':');
            if(ssid && *(ssid + 1)) {
                ssid++;
                strncpy(out, ssid, out_size - 1);
                out[out_size - 1] = '\0';
                return true;
            }
        }
    } else {
        /* ifupdown/wpa_supplicant: ask wpa_cli for current SSID */
        char iface[32];
        if(detect_wifi_iface(iface, sizeof(iface))) {
            char cmd[256];
            snprintf(cmd, sizeof(cmd),
                "wpa_cli -i %s status 2>/dev/null | grep '^ssid=' | cut -d= -f2", iface);
            if(run_cmd_output(cmd, out, out_size) && out[0]) {
                return true;
            }
        }
    }

    out[0] = '\0';
    return false;
}

static bool wfr_net_connect_nmcli(const WfrWifiCreds* creds) {
    char escaped_ssid[256];
    char escaped_pass[256];

    if(!shell_escape(creds->ssid, escaped_ssid, sizeof(escaped_ssid))) {
        syslog(LOG_ERR, "wifird: SSID too long to escape");
        return false;
    }

    char cmd[1024];

    if(creds->security == WFR_SEC_OPEN) {
        snprintf(
            cmd,
            sizeof(cmd),
            "nmcli dev wifi connect %s%s 2>&1",
            escaped_ssid,
            creds->hidden ? " hidden yes" : "");
    } else {
        if(!shell_escape(creds->password, escaped_pass, sizeof(escaped_pass))) {
            syslog(LOG_ERR, "wifird: password too long to escape");
            return false;
        }
        snprintf(
            cmd,
            sizeof(cmd),
            "nmcli dev wifi connect %s password %s%s 2>&1",
            escaped_ssid,
            escaped_pass,
            creds->hidden ? " hidden yes" : "");
    }

    syslog(LOG_INFO, "wifird: connecting to SSID: %s (security=%d)", creds->ssid, creds->security);

    int status = run_cmd(cmd);
    if(status != 0) {
        syslog(LOG_ERR, "wifird: nmcli connect failed (exit %d)", status);
        return false;
    }

    sleep(3);

    if(!verify_connectivity()) {
        syslog(LOG_WARNING, "wifird: connected but no internet connectivity");
    }

    syslog(LOG_INFO, "wifird: successfully connected to %s", creds->ssid);
    return true;
}

static bool wfr_net_connect_interfaces(const WfrWifiCreds* creds) {
    char iface[32];
    if(!detect_wifi_iface(iface, sizeof(iface))) {
        syslog(LOG_ERR, "wifird: cannot connect — no WiFi interface");
        return false;
    }

    syslog(LOG_INFO, "wifird: using WiFi interface: %s", iface);

    const char* conf_path = "/etc/network/interfaces.d/wifird";
    char wpa_conf_path[128];
    snprintf(wpa_conf_path, sizeof(wpa_conf_path),
        "/etc/wpa_supplicant/wpa_supplicant-wifird-%s.conf", iface);

    /* Write wpa_supplicant config */
    FILE* wpa = fopen(wpa_conf_path, "w");
    if(!wpa) {
        syslog(LOG_ERR, "wifird: cannot write %s: %s", wpa_conf_path, strerror(errno));
        return false;
    }
    fprintf(wpa, "ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev\n");
    fprintf(wpa, "update_config=1\n\n");
    fprintf(wpa, "network={\n");
    fprintf(wpa, "    ssid=\"%s\"\n", creds->ssid);
    if(creds->security == WFR_SEC_OPEN) {
        fprintf(wpa, "    key_mgmt=NONE\n");
    } else if(creds->security == WFR_SEC_SAE) {
        fprintf(wpa, "    key_mgmt=SAE\n");
        fprintf(wpa, "    psk=\"%s\"\n", creds->password);
    } else {
        fprintf(wpa, "    psk=\"%s\"\n", creds->password);
    }
    if(creds->hidden) {
        fprintf(wpa, "    scan_ssid=1\n");
    }
    fprintf(wpa, "}\n");
    fclose(wpa);

    /* Write interfaces config */
    FILE* ifcfg = fopen(conf_path, "w");
    if(!ifcfg) {
        syslog(LOG_ERR, "wifird: cannot write %s: %s", conf_path, strerror(errno));
        unlink(wpa_conf_path);
        return false;
    }
    fprintf(ifcfg, "# Managed by wifird\n");
    fprintf(ifcfg, "auto %s\n", iface);
    fprintf(ifcfg, "iface %s inet dhcp\n", iface);
    fprintf(ifcfg, "    wpa-conf %s\n", wpa_conf_path);
    fclose(ifcfg);

    syslog(LOG_INFO, "wifird: wrote config, bringing up %s", iface);

    /* Bring interface down and back up instead of full networking restart
     * to avoid disrupting other interfaces (e.g. ethernet) */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ifdown %s 2>&1; ifup %s 2>&1", iface, iface);
    int status = run_cmd(cmd);
    if(status != 0) {
        syslog(LOG_WARNING, "wifird: ifup exited %d (may still be connecting)", status);
    }

    /* Stage 1: wait for WPA authentication (up to 20 seconds) */
    int wpa_result = wait_for_wpa_association(iface, 20);
    if(wpa_result < 0) {
        syslog(LOG_ERR, "wifird: WPA authentication failed for %s", creds->ssid);
        return false;
    }

    /* Stage 2: wait for DHCP (up to 15 seconds) */
    if(!wait_for_dhcp(iface, 15)) {
        syslog(LOG_ERR, "wifird: no IP address obtained on %s", iface);
        return false;
    }

    /* Stage 3: verify actual internet connectivity */
    if(verify_connectivity()) {
        syslog(LOG_INFO, "wifird: connected to %s with internet access", creds->ssid);
    } else {
        syslog(LOG_WARNING, "wifird: connected to %s but no internet (DNS failed)", creds->ssid);
        /* Still return true — WiFi link is up even without internet */
    }

    return true;
}

bool wfr_net_connect(const WfrWifiCreds* creds) {
    if(!creds || creds->ssid[0] == '\0') return false;

    syslog(LOG_INFO, "wifird: connecting to SSID: %s (security=%d, hidden=%d)",
        creds->ssid, creds->security, creds->hidden);

    if(wfr_net_has_networkmanager()) {
        return wfr_net_connect_nmcli(creds);
    } else {
        return wfr_net_connect_interfaces(creds);
    }
}

bool wfr_net_rollback(const char* ssid) {
    if(!ssid || ssid[0] == '\0') return false;

    syslog(LOG_INFO, "wifird: rolling back to previous SSID: %s", ssid);

    if(wfr_net_has_networkmanager()) {
        char escaped_ssid[256];
        if(!shell_escape(ssid, escaped_ssid, sizeof(escaped_ssid))) return false;

        char cmd[512];
        snprintf(cmd, sizeof(cmd), "nmcli con up %s 2>&1", escaped_ssid);
        int status = run_cmd(cmd);
        if(status != 0) {
            syslog(LOG_ERR, "wifird: rollback to %s failed (exit %d)", ssid, status);
            return false;
        }
        syslog(LOG_INFO, "wifird: rolled back to %s", ssid);
        return true;
    }

    /* For non-NM systems, remove our config and bring interface back up */
    char iface[32];
    if(detect_wifi_iface(iface, sizeof(iface))) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "ifdown %s 2>&1", iface);
        run_cmd(cmd);
    }

    unlink("/etc/network/interfaces.d/wifird");

    /* Remove any wifird wpa_supplicant configs */
    run_cmd("rm -f /etc/wpa_supplicant/wpa_supplicant-wifird-*.conf 2>/dev/null");

    if(iface[0]) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "ifup %s 2>&1", iface);
        run_cmd(cmd);
    }

    syslog(LOG_INFO, "wifird: removed wifird config, reverted to previous state");
    return true;
}
