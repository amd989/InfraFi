#include "wfr_lirc.h"

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <linux/lirc.h>

#define LIRC_VALUE_MASK 0x00FFFFFF

int wfr_lirc_open(const char* device) {
    int fd = open(device, O_RDONLY);
    if(fd < 0) {
        syslog(LOG_ERR, "wifird: failed to open %s: %s", device, strerror(errno));
        return -1;
    }

    /* Set receive mode to MODE2 (pulse/space timings) */
    unsigned int mode = LIRC_MODE_MODE2;
    if(ioctl(fd, LIRC_SET_REC_MODE, &mode) < 0) {
        syslog(
            LOG_WARNING,
            "wifird: LIRC_SET_REC_MODE failed: %s (may already be MODE2)",
            strerror(errno));
    } else {
        syslog(LOG_INFO, "wifird: set receive mode to MODE2");
    }

    syslog(LOG_INFO, "wifird: opened %s (fd=%d)", device, fd);
    return fd;
}

void wfr_lirc_close(int fd) {
    if(fd >= 0) {
        close(fd);
    }
}

int wfr_lirc_read(int fd, bool* is_pulse, uint32_t* duration_us) {
    for(;;) {
        uint32_t raw;
        ssize_t n = read(fd, &raw, sizeof(raw));
        if(n != (ssize_t)sizeof(raw)) {
            if(n < 0 && errno != EINTR) {
                syslog(LOG_ERR, "wifird: lirc read error: %s", strerror(errno));
            }
            return -1;
        }

        uint32_t type = raw & LIRC_MODE2_MASK;

        /* Silently skip overflow and timeout events */
        if(type == LIRC_MODE2_OVERFLOW || type == LIRC_MODE2_TIMEOUT) {
            continue;
        }

        *is_pulse = (type == LIRC_MODE2_PULSE);
        *duration_us = raw & LIRC_VALUE_MASK;
        return 0;
    }
}
