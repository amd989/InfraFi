#include "wfr_network.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <netdb.h>

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

bool wfr_net_has_networkmanager(void) {
    return run_cmd("systemctl is-active --quiet NetworkManager") == 0;
}

bool wfr_net_get_current(char* out, size_t out_size) {
    if(wfr_net_has_networkmanager()) {
        /* nmcli -t -f active,ssid dev wifi | grep '^yes:' */
        char line[256];
        if(run_cmd_output(
               "nmcli -t -f active,ssid dev wifi 2>/dev/null | grep '^yes:'",
               line,
               sizeof(line))) {
            /* Output format: "yes:MySSID" */
            char* ssid = strchr(line, ':');
            if(ssid && *(ssid + 1)) {
                ssid++;
                strncpy(out, ssid, out_size - 1);
                out[out_size - 1] = '\0';
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

    /* Give the connection a moment to establish */
    sleep(3);

    /* Verify connectivity */
    if(!verify_connectivity()) {
        syslog(LOG_WARNING, "wifird: connected but no internet connectivity");
        /* Still return true — WiFi connected even if no internet */
    }

    syslog(LOG_INFO, "wifird: successfully connected to %s", creds->ssid);
    return true;
}

static bool wfr_net_connect_interfaces(const WfrWifiCreds* creds) {
    /* Write wpa_supplicant config and interfaces entry for systems without NetworkManager */
    const char* conf_path = "/etc/network/interfaces.d/wi-fir";
    const char* wpa_conf_path = "/etc/wpa_supplicant/wpa_supplicant-wi-fir.conf";

    /* Write wpa_supplicant config */
    FILE* wpa = fopen(wpa_conf_path, "w");
    if(!wpa) {
        syslog(LOG_ERR, "wifird: cannot write %s", wpa_conf_path);
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
    FILE* iface = fopen(conf_path, "w");
    if(!iface) {
        syslog(LOG_ERR, "wifird: cannot write %s", conf_path);
        return false;
    }
    fprintf(iface, "# Managed by wi-fir daemon\n");
    fprintf(iface, "auto wlan0\n");
    fprintf(iface, "iface wlan0 inet dhcp\n");
    fprintf(iface, "    wpa-conf %s\n", wpa_conf_path);
    fclose(iface);

    syslog(LOG_INFO, "wifird: wrote interfaces config, restarting networking");
    int status = run_cmd("systemctl restart networking 2>&1");
    if(status != 0) {
        syslog(LOG_ERR, "wifird: networking restart failed (exit %d)", status);
        return false;
    }

    sleep(5);
    return verify_connectivity();
}

bool wfr_net_connect(const WfrWifiCreds* creds) {
    if(!creds || creds->ssid[0] == '\0') return false;

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

    /* For non-NM systems, remove our config and restart */
    unlink("/etc/network/interfaces.d/wi-fir");
    unlink("/etc/wpa_supplicant/wpa_supplicant-wi-fir.conf");
    run_cmd("systemctl restart networking 2>&1");
    syslog(LOG_INFO, "wifird: removed wi-fir config, reverted to default");
    return true;
}
