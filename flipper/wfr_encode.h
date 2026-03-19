#pragma once

#include "protocol/wfr_protocol.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Transmit WiFi credentials over IR using the WFR protocol.
 *
 * Builds the WiFi QR string, splits it into frames (START + DATA + END),
 * and sends each frame via infrared_send_raw_ext() with inter-frame delays.
 *
 * Returns true if transmission completed, false on error.
 */
bool wfr_transmit_credentials(const WfrWifiCreds* creds);

/*
 * Encode a single WFR frame into raw IR timings.
 *
 * frame_type: WFR_FRAME_START, WFR_FRAME_DATA, or WFR_FRAME_END
 * seq_num:    sequence number (0-based for DATA frames)
 * payload:    frame payload bytes
 * payload_len: number of payload bytes (0 to WFR_MAX_PAYLOAD_PER_FRAME)
 * timings_out: output buffer for pulse/space microsecond values
 * timings_max: size of timings_out buffer
 *
 * Returns number of timings written, or 0 on error.
 * Timings alternate: pulse, space, pulse, space, ... starting from pulse (mark).
 */
size_t wfr_encode_frame(
    uint8_t frame_type,
    uint8_t seq_num,
    const uint8_t* payload,
    uint8_t payload_len,
    uint32_t* timings_out,
    size_t timings_max);

#ifdef __cplusplus
}
#endif
