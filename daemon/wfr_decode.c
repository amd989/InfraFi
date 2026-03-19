#include "wfr_decode.h"
#include <string.h>
#include <syslog.h>

#define WFR_TIMEOUT_SEC 5

void wfr_decode_init(WfrDecoder* dec) {
    memset(dec, 0, sizeof(WfrDecoder));
    dec->state = WfrDecodeIdle;
}

static void wfr_decode_reset(WfrDecoder* dec) {
    dec->state = WfrDecodeIdle;
    dec->frame_buf_len = 0;
    dec->current_byte = 0;
    dec->bit_index = 0;
    dec->expecting_space = false;
    dec->in_transmission = false;
    dec->expected_frames = 0;
    dec->expected_total_len = 0;
    dec->frames_received = 0;
    memset(dec->received_mask, 0, sizeof(dec->received_mask));
}

static bool wfr_decode_check_timeout(WfrDecoder* dec) {
    if(!dec->in_transmission) return false;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    long elapsed = (now.tv_sec - dec->start_time.tv_sec);
    if(now.tv_nsec < dec->start_time.tv_nsec) elapsed--;

    if(elapsed >= WFR_TIMEOUT_SEC) {
        syslog(LOG_WARNING, "wifird: transmission timeout after %lds", elapsed);
        wfr_decode_reset(dec);
        return true;
    }
    return false;
}

/* Start reading bits for a new frame */
static void wfr_decode_begin_frame(WfrDecoder* dec) {
    dec->state = WfrDecodeReadingBits;
    dec->frame_buf_len = 0;
    dec->current_byte = 0;
    dec->bit_index = 7;
    dec->expecting_space = false;
}

/*
 * Process a completed frame buffer.
 * Returns: >0 if full payload decoded, 0 if more frames needed, -1 on error.
 */
static int wfr_decode_process_frame(WfrDecoder* dec, char* out, size_t out_size) {
    /* Minimum frame: type(1) + seq(1) + len(1) + crc(1) = 4 bytes */
    if(dec->frame_buf_len < 4) {
        syslog(LOG_WARNING, "wifird: frame too short (%zu bytes)", dec->frame_buf_len);
        return -1;
    }

    uint8_t frame_type = dec->frame_buf[0];
    uint8_t seq_num = dec->frame_buf[1];
    uint8_t payload_len = dec->frame_buf[2];

    /* Validate payload_len matches what we actually received */
    size_t expected_total = 3 + payload_len + 1; /* type+seq+len + payload + crc */
    if(dec->frame_buf_len != expected_total) {
        syslog(
            LOG_WARNING,
            "wifird: frame length mismatch (got %zu, expected %zu)",
            dec->frame_buf_len,
            expected_total);
        return -1;
    }

    /* Verify CRC-8 (over everything except the CRC byte itself) */
    uint8_t received_crc = dec->frame_buf[dec->frame_buf_len - 1];
    uint8_t computed_crc = wfr_crc8(dec->frame_buf, dec->frame_buf_len - 1);
    if(received_crc != computed_crc) {
        syslog(
            LOG_WARNING,
            "wifird: CRC-8 mismatch (got 0x%02x, expected 0x%02x)",
            received_crc,
            computed_crc);
        return -1;
    }

    const uint8_t* payload = &dec->frame_buf[3];

    switch(frame_type) {
    case WFR_FRAME_START:
        if(payload_len != 2) {
            syslog(LOG_WARNING, "wifird: START frame bad payload len %d", payload_len);
            return -1;
        }
        dec->expected_frames = payload[0];
        dec->expected_total_len = payload[1];
        dec->frames_received = 0;
        dec->in_transmission = true;
        memset(dec->received_mask, 0, sizeof(dec->received_mask));
        memset(dec->payload_buf, 0, sizeof(dec->payload_buf));
        clock_gettime(CLOCK_MONOTONIC, &dec->start_time);
        syslog(
            LOG_INFO,
            "wifird: START received (frames=%d, total_len=%d)",
            dec->expected_frames,
            dec->expected_total_len);
        return 0;

    case WFR_FRAME_DATA:
        if(!dec->in_transmission) {
            syslog(LOG_WARNING, "wifird: DATA frame without START");
            return -1;
        }
        if(seq_num >= dec->expected_frames) {
            syslog(LOG_WARNING, "wifird: DATA seq %d out of range (max %d)", seq_num, dec->expected_frames - 1);
            return -1;
        }
        /* Check for duplicate */
        if(dec->received_mask[seq_num / 8] & (1 << (seq_num % 8))) {
            syslog(LOG_WARNING, "wifird: duplicate DATA seq %d", seq_num);
            return 0; /* ignore duplicate, not an error */
        }
        /* Copy payload to correct position */
        {
            size_t offset = seq_num * WFR_MAX_PAYLOAD_PER_FRAME;
            if(offset + payload_len > WFR_MAX_TOTAL_PAYLOAD) {
                syslog(LOG_WARNING, "wifird: DATA would overflow payload buffer");
                return -1;
            }
            memcpy(&dec->payload_buf[offset], payload, payload_len);
            dec->received_mask[seq_num / 8] |= (1 << (seq_num % 8));
            dec->frames_received++;
            syslog(LOG_DEBUG, "wifird: DATA seq %d (%d bytes)", seq_num, payload_len);
        }
        return 0;

    case WFR_FRAME_END:
        if(!dec->in_transmission) {
            syslog(LOG_WARNING, "wifird: END frame without START");
            return -1;
        }
        if(payload_len != 4) {
            syslog(LOG_WARNING, "wifird: END frame bad payload len %d", payload_len);
            wfr_decode_reset(dec);
            return -1;
        }
        /* Check all DATA frames received */
        if(dec->frames_received != dec->expected_frames) {
            syslog(
                LOG_WARNING,
                "wifird: END but only %d/%d DATA frames received",
                dec->frames_received,
                dec->expected_frames);
            wfr_decode_reset(dec);
            return -1;
        }
        /* Verify CRC-32 */
        {
            uint32_t expected_crc32 = ((uint32_t)payload[0] << 24) |
                                      ((uint32_t)payload[1] << 16) |
                                      ((uint32_t)payload[2] << 8) |
                                      ((uint32_t)payload[3]);
            uint32_t computed_crc32 = wfr_crc32(dec->payload_buf, dec->expected_total_len);
            if(expected_crc32 != computed_crc32) {
                syslog(
                    LOG_WARNING,
                    "wifird: CRC-32 mismatch (got 0x%08x, expected 0x%08x)",
                    computed_crc32,
                    expected_crc32);
                wfr_decode_reset(dec);
                return -1;
            }
        }
        /* Success! Copy payload to output */
        if((size_t)dec->expected_total_len >= out_size) {
            syslog(LOG_ERR, "wifird: output buffer too small");
            wfr_decode_reset(dec);
            return -1;
        }
        memcpy(out, dec->payload_buf, dec->expected_total_len);
        out[dec->expected_total_len] = '\0';
        syslog(LOG_INFO, "wifird: transmission complete (%d bytes)", dec->expected_total_len);
        {
            int result_len = dec->expected_total_len;
            wfr_decode_reset(dec);
            return result_len;
        }

    default:
        syslog(LOG_WARNING, "wifird: unknown frame type 0x%02x", frame_type);
        return -1;
    }
}

int wfr_decode_feed(
    WfrDecoder* dec,
    bool is_pulse,
    uint32_t duration_us,
    char* out,
    size_t out_size) {
    /* Check timeout on each timing event */
    if(wfr_decode_check_timeout(dec)) {
        return -1;
    }

    switch(dec->state) {
    case WfrDecodeIdle:
        /* Looking for header pulse (9000us) */
        if(is_pulse && wfr_timing_match(duration_us, WFR_HEADER_PULSE)) {
            dec->state = WfrDecodeHeaderSpace;
        }
        return 0;

    case WfrDecodeHeaderSpace:
        /* Looking for header space (4500us) */
        if(!is_pulse && wfr_timing_match(duration_us, WFR_HEADER_SPACE)) {
            wfr_decode_begin_frame(dec);
        } else {
            /* Not a valid header, go back to idle */
            dec->state = WfrDecodeIdle;
        }
        return 0;

    case WfrDecodeReadingBits:
        if(dec->expecting_space) {
            /* We just saw a bit pulse, now we see the space that determines bit value */
            if(!is_pulse) {
                if(wfr_timing_match(duration_us, WFR_BIT_ONE_SPACE)) {
                    dec->current_byte |= (1 << dec->bit_index);
                } else if(wfr_timing_match(duration_us, WFR_BIT_ZERO_SPACE)) {
                    /* bit is already 0, nothing to do */
                } else {
                    /* Could be a frame gap or noise — try to process what we have */
                    if(duration_us > WFR_FRAME_GAP / 2) {
                        /* Frame gap detected mid-read; process accumulated frame */
                        goto try_process;
                    }
                    /* Bad timing, reset frame */
                    syslog(LOG_DEBUG, "wifird: bad space timing %u us", duration_us);
                    dec->state = WfrDecodeIdle;
                    return -1;
                }
                dec->expecting_space = false;

                if(dec->bit_index == 0) {
                    /* Byte complete */
                    if(dec->frame_buf_len < sizeof(dec->frame_buf)) {
                        dec->frame_buf[dec->frame_buf_len++] = dec->current_byte;
                    }
                    dec->current_byte = 0;
                    dec->bit_index = 7;

                    /* Check if we have enough data to know the full frame length */
                    if(dec->frame_buf_len >= 3) {
                        uint8_t payload_len = dec->frame_buf[2];
                        size_t expected_total = 3 + payload_len + 1;
                        if(dec->frame_buf_len == expected_total) {
                            /* Frame complete! */
                            goto try_process;
                        }
                    }
                } else {
                    dec->bit_index--;
                }
            } else {
                /* Got a pulse when expecting space — error */
                dec->state = WfrDecodeIdle;
                return -1;
            }
        } else {
            /* Expecting a bit pulse */
            if(is_pulse) {
                if(wfr_timing_match(duration_us, WFR_BIT_PULSE)) {
                    dec->expecting_space = true;
                } else if(wfr_timing_match(duration_us, WFR_STOP_PULSE)) {
                    /* Could be stop pulse — same timing as bit pulse, handle after space */
                    dec->expecting_space = true;
                } else if(wfr_timing_match(duration_us, WFR_HEADER_PULSE)) {
                    /* New header while reading — process current frame first, then start new */
                    int result = wfr_decode_process_frame(dec, out, out_size);
                    if(result > 0) return result;
                    dec->state = WfrDecodeHeaderSpace;
                    return (result == 0) ? 0 : -1;
                } else {
                    dec->state = WfrDecodeIdle;
                    return -1;
                }
            } else {
                /* Got a space when expecting pulse */
                if(duration_us > WFR_FRAME_GAP / 2) {
                    /* Frame gap — process frame */
                    goto try_process;
                }
                dec->state = WfrDecodeIdle;
                return -1;
            }
        }
        return 0;

    try_process: {
        int result = wfr_decode_process_frame(dec, out, out_size);
        dec->state = WfrDecodeIdle;
        if(result > 0) {
            return result;
        } else if(result < 0) {
            /* Frame error but don't reset transmission state for recoverable errors */
            return -1;
        }
        return 0;
    }
    }

    return 0;
}
