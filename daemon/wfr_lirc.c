#include "wfr_lirc.h"

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <linux/lirc.h>

/* LIRC mode2 data format:
 * Each read returns a uint32_t where:
 *   bit 24 (0x01000000) = LIRC_MODE2_PULSE (1 = pulse, 0 = space)
 *   bits 0-23 = duration in microseconds */
#define LIRC_VALUE_MASK 0x00FFFFFF

/* Disable all kernel IR protocol decoders on the rc device
 * so raw pulses flow through to /dev/lirc instead of being consumed. */
static void disable_rc_protocols(void) {
    /* Find the rc device associated with this lirc device */
    const char* paths[] = {
        "/sys/class/rc/rc0/protocols",
        "/sys/class/rc/rc1/protocols",
        NULL,
    };

    for(int i = 0; paths[i]; i++) {
        int fd = open(paths[i], O_WRONLY);
        if(fd >= 0) {
            const char* cmd = "none";
            if(write(fd, cmd, strlen(cmd)) > 0) {
                syslog(LOG_INFO, "wifird: disabled kernel IR decoders via %s", paths[i]);
            }
            close(fd);
            return;
        }
    }
    syslog(LOG_WARNING, "wifird: could not disable kernel IR decoders — raw data may not reach lirc");
}

int wfr_lirc_open(const char* device) {
    int fd = open(device, O_RDONLY);
    if(fd < 0) {
        syslog(LOG_ERR, "wifird: failed to open %s: %s", device, strerror(errno));
        return -1;
    }

    /* Set receive mode to MODE2 (pulse/space timings) */
    unsigned int mode = LIRC_MODE_MODE2;
    if(ioctl(fd, LIRC_SET_REC_MODE, &mode) < 0) {
        syslog(LOG_WARNING, "wifird: LIRC_SET_REC_MODE failed: %s (may already be MODE2)", strerror(errno));
    } else {
        syslog(LOG_INFO, "wifird: set receive mode to MODE2");
    }

    /* Disable kernel protocol decoders that would eat our raw data */
    disable_rc_protocols();

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

        if(type == LIRC_MODE2_OVERFLOW) {
            syslog(LOG_DEBUG, "wifird: LIRC buffer overflow (data lost)");
            continue; /* skip and read next event */
        }

        if(type == LIRC_MODE2_TIMEOUT) {
            /* Long gap — treat as a very long space to trigger frame processing */
            *is_pulse = false;
            *duration_us = raw & LIRC_VALUE_MASK;
            if(*duration_us == 0) *duration_us = 100000; /* default large gap */
            return 0;
        }

        *is_pulse = (type == LIRC_MODE2_PULSE);
        *duration_us = raw & LIRC_VALUE_MASK;
        return 0;
    }
}
