#include "wfr_encode.h"
#include <infrared_transmit.h>
#include <furi.h>
#include <string.h>

/* Encode a single byte into pulse/space timings (MSB first).
 * Returns number of timings written (always 16 for 8 bits). */
static size_t wfr_encode_byte(uint8_t byte, uint32_t* timings, size_t offset, size_t max) {
    size_t pos = offset;
    for(int bit = 7; bit >= 0; bit--) {
        if(pos + 2 > max) return 0;
        timings[pos++] = WFR_BIT_PULSE;
        timings[pos++] = (byte & (1 << bit)) ? WFR_BIT_ONE_SPACE : WFR_BIT_ZERO_SPACE;
    }
    return pos - offset;
}

size_t wfr_encode_frame(
    uint8_t frame_type,
    uint8_t seq_num,
    const uint8_t* payload,
    uint8_t payload_len,
    uint32_t* timings_out,
    size_t timings_max) {
    if(payload_len > WFR_MAX_PAYLOAD_PER_FRAME) return 0;

    /* Calculate required timings:
     * header (2) + frame_type (16) + seq (16) + len (16) +
     * payload (payload_len * 16) + crc8 (16) + stop (1) */
    size_t needed = 2 + 16 + 16 + 16 + (payload_len * 16) + 16 + 1;
    if(needed > timings_max) return 0;

    size_t pos = 0;

    /* Header: 9ms pulse + 4.5ms space */
    timings_out[pos++] = WFR_HEADER_PULSE;
    timings_out[pos++] = WFR_HEADER_SPACE;

    /* Build CRC input: frame_type + seq_num + payload_len + payload */
    uint8_t crc_buf[3 + WFR_MAX_PAYLOAD_PER_FRAME];
    crc_buf[0] = frame_type;
    crc_buf[1] = seq_num;
    crc_buf[2] = payload_len;
    if(payload_len > 0 && payload) {
        memcpy(&crc_buf[3], payload, payload_len);
    }
    uint8_t crc = wfr_crc8(crc_buf, 3 + payload_len);

    /* Encode frame_type */
    size_t written = wfr_encode_byte(frame_type, timings_out, pos, timings_max);
    if(!written) return 0;
    pos += written;

    /* Encode seq_num */
    written = wfr_encode_byte(seq_num, timings_out, pos, timings_max);
    if(!written) return 0;
    pos += written;

    /* Encode payload_len */
    written = wfr_encode_byte(payload_len, timings_out, pos, timings_max);
    if(!written) return 0;
    pos += written;

    /* Encode payload bytes */
    for(uint8_t i = 0; i < payload_len; i++) {
        written = wfr_encode_byte(payload[i], timings_out, pos, timings_max);
        if(!written) return 0;
        pos += written;
    }

    /* Encode CRC-8 */
    written = wfr_encode_byte(crc, timings_out, pos, timings_max);
    if(!written) return 0;
    pos += written;

    /* Stop pulse */
    if(pos >= timings_max) return 0;
    timings_out[pos++] = WFR_STOP_PULSE;

    return pos;
}

bool wfr_transmit_credentials(const WfrWifiCreds* creds) {
    if(!creds || creds->ssid[0] == '\0') return false;

    /* Build WiFi QR string */
    char wifi_str[WFR_MAX_TOTAL_PAYLOAD + 1];
    size_t wifi_len = wfr_build_wifi_string(creds, wifi_str, sizeof(wifi_str));
    if(wifi_len == 0) return false;

    const uint8_t* payload_bytes = (const uint8_t*)wifi_str;

    /* Calculate number of DATA frames needed */
    uint8_t total_data_frames =
        (uint8_t)((wifi_len + WFR_MAX_PAYLOAD_PER_FRAME - 1) / WFR_MAX_PAYLOAD_PER_FRAME);

    /* Compute whole-payload CRC-32 */
    uint32_t payload_crc32 = wfr_crc32(payload_bytes, wifi_len);

    /* Timing buffer for a single frame — heap-allocated to avoid stack overflow.
     * Max frame: header(2) + (3 + 16 payload + 1 crc) * 16 bits + stop(1) = 323 timings */
    const size_t timings_max = 400;
    uint32_t* timings = malloc(timings_max * sizeof(uint32_t));
    if(!timings) return false;

    bool success = false;

    /* Retransmit the full sequence multiple times.
     * The receiver accumulates frames across retransmissions —
     * each pass fills in whatever the hardware FIFO dropped last time. */
    for(uint8_t attempt = 0; attempt < WFR_RETRANSMIT_COUNT; attempt++) {
        /* --- Send START frame --- */
        uint8_t start_payload[2];
        start_payload[0] = total_data_frames;
        start_payload[1] = (uint8_t)wifi_len;

        size_t timing_count = wfr_encode_frame(
            WFR_FRAME_START, 0, start_payload, sizeof(start_payload), timings, timings_max);
        if(!timing_count) goto cleanup;

        infrared_send_raw_ext(timings, timing_count, true, WFR_CARRIER_FREQ, WFR_DUTY_CYCLE);
        furi_delay_ms(WFR_INTER_FRAME_DELAY_MS);

        /* --- Send DATA frames --- */
        for(uint8_t seq = 0; seq < total_data_frames; seq++) {
            size_t offset = seq * WFR_MAX_PAYLOAD_PER_FRAME;
            size_t remaining = wifi_len - offset;
            uint8_t chunk_len =
                (remaining > WFR_MAX_PAYLOAD_PER_FRAME) ? WFR_MAX_PAYLOAD_PER_FRAME : (uint8_t)remaining;

            timing_count = wfr_encode_frame(
                WFR_FRAME_DATA, seq, &payload_bytes[offset], chunk_len, timings, timings_max);
            if(!timing_count) goto cleanup;

            infrared_send_raw_ext(timings, timing_count, true, WFR_CARRIER_FREQ, WFR_DUTY_CYCLE);
            furi_delay_ms(WFR_INTER_FRAME_DELAY_MS);
        }

        /* --- Send END frame --- */
        uint8_t end_payload[4];
        end_payload[0] = (uint8_t)(payload_crc32 >> 24);
        end_payload[1] = (uint8_t)(payload_crc32 >> 16);
        end_payload[2] = (uint8_t)(payload_crc32 >> 8);
        end_payload[3] = (uint8_t)(payload_crc32 & 0xFF);

        timing_count = wfr_encode_frame(
            WFR_FRAME_END, 0, end_payload, sizeof(end_payload), timings, timings_max);
        if(!timing_count) goto cleanup;

        infrared_send_raw_ext(timings, timing_count, true, WFR_CARRIER_FREQ, WFR_DUTY_CYCLE);

        /* Gap between retransmissions */
        if(attempt < WFR_RETRANSMIT_COUNT - 1) {
            furi_delay_ms(WFR_RETRANSMIT_GAP_MS);
        }
    }

    success = true;

cleanup:
    free(timings);
    return success;
}
