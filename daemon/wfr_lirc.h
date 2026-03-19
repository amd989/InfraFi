#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Open a LIRC device. Returns file descriptor, or -1 on error. */
int wfr_lirc_open(const char* device);

/* Close a LIRC device. */
void wfr_lirc_close(int fd);

/*
 * Read one timing event from the LIRC device.
 * Blocks until data is available. Silently skips overflow/timeout events.
 *
 * is_pulse: set to true if this is a pulse (mark), false for space (gap)
 * duration_us: set to the duration in microseconds
 *
 * Returns 0 on success, -1 on error.
 */
int wfr_lirc_read(int fd, bool* is_pulse, uint32_t* duration_us);
