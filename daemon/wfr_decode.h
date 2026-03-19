#pragma once

#include "../flipper/protocol/wfr_protocol.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>

/*
 * WFR protocol decoder state machine.
 * Feed it pulse/space timings and it reconstructs the WiFi credential payload.
 */

typedef enum {
    WfrDecodeIdle,
    WfrDecodeHeaderSpace,
    WfrDecodeReadingBits,
} WfrDecodeState;

typedef struct {
    WfrDecodeState state;

    /* Bit accumulation within a frame */
    uint8_t frame_buf[3 + WFR_MAX_PAYLOAD_PER_FRAME + 1]; /* type+seq+len+payload+crc */
    size_t frame_buf_len;
    uint8_t current_byte;
    uint8_t bit_index; /* 0-7, counts down from 7 (MSB first) */
    bool expecting_space; /* after a bit pulse, we expect a space */

    /* Multi-frame reassembly */
    uint8_t payload_buf[WFR_MAX_TOTAL_PAYLOAD + 1];
    uint8_t expected_frames;
    uint8_t expected_total_len;
    uint8_t received_mask[32]; /* bitmask: which seq_nums received (up to 255 frames) */
    uint8_t frames_received;
    bool in_transmission; /* true after receiving START, until END or timeout */

    /* Timeout tracking */
    struct timespec start_time;
} WfrDecoder;

/*
 * Initialize/reset the decoder to its idle state.
 */
void wfr_decode_init(WfrDecoder* dec);

/*
 * Feed one timing event into the decoder.
 *
 * is_pulse: true for pulse (mark), false for space (gap)
 * duration_us: duration in microseconds
 * out: output buffer for the decoded WiFi QR payload string (null-terminated)
 * out_size: size of the output buffer
 *
 * Returns:
 *   >0 : payload fully decoded; return value is the length of the string in out
 *    0 : timing consumed, more data needed
 *   -1 : error (bad CRC, timeout, etc.) — decoder resets itself
 */
int wfr_decode_feed(
    WfrDecoder* dec,
    bool is_pulse,
    uint32_t duration_us,
    char* out,
    size_t out_size);

/*
 * Signal that a LIRC buffer overflow occurred.
 * Resets frame-level state (goes to Idle) but preserves transmission
 * state so retransmissions can still fill in missing frames.
 */
void wfr_decode_overflow(WfrDecoder* dec);
