#include "wfr_lirc.h"

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

/* LIRC mode2 data format:
 * Each read returns a uint32_t where:
 *   bit 24 (0x01000000) = LIRC_MODE2_PULSE (1 = pulse, 0 = space)
 *   bits 0-23 = duration in microseconds
 *
 * LIRC_MODE2_PULSE is defined in linux/lirc.h but we define it here
 * to avoid pulling in the full kernel header. */
#define LIRC_MODE2_PULSE 0x01000000
#define LIRC_VALUE_MASK  0x00FFFFFF

int wfr_lirc_open(const char* device) {
    int fd = open(device, O_RDONLY);
    if(fd < 0) {
        syslog(LOG_ERR, "wifird: failed to open %s: %s", device, strerror(errno));
        return -1;
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
    uint32_t raw;
    ssize_t n = read(fd, &raw, sizeof(raw));
    if(n != sizeof(raw)) {
        if(n < 0 && errno != EINTR) {
            syslog(LOG_ERR, "wifird: lirc read error: %s", strerror(errno));
        }
        return -1;
    }

    *is_pulse = (raw & LIRC_MODE2_PULSE) != 0;
    *duration_us = raw & LIRC_VALUE_MASK;
    return 0;
}
