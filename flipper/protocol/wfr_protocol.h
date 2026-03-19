#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * WFR (Wi-FiR Raw) IR Protocol
 *
 * Custom pulse-distance encoding over 38kHz consumer IR.
 * Payload is a WiFi QR string: WIFI:T:<type>;S:<ssid>;P:<pass>;H:<hidden>;;
 *
 * Frame structure:
 *   [Header 9000/4500] [frame_type:8] [seq:8] [len:8] [payload:0-32] [CRC-8] [Stop]
 */

/* IR timing constants (microseconds) */
#define WFR_HEADER_PULSE   9000
#define WFR_HEADER_SPACE   4500
#define WFR_BIT_PULSE      560
#define WFR_BIT_ONE_SPACE  1690
#define WFR_BIT_ZERO_SPACE 560
#define WFR_STOP_PULSE     560
#define WFR_FRAME_GAP      40000

/* IR carrier */
#define WFR_CARRIER_FREQ 38000
#define WFR_DUTY_CYCLE   0.33f

/* Timing tolerance for decoding (+/- 30%) */
#define WFR_TOLERANCE_PCT 30

/* Frame types */
#define WFR_FRAME_START 0x01
#define WFR_FRAME_DATA  0x02
#define WFR_FRAME_END   0x03

/* Protocol limits
 * 16 bytes/frame generates ~291 timings = 1164 bytes of mode2 data,
 * well within the typical 2048-byte kernel LIRC buffer. */
#define WFR_MAX_PAYLOAD_PER_FRAME 16
#define WFR_MAX_TOTAL_PAYLOAD     255
#define WFR_INTER_FRAME_DELAY_MS  100

/* WiFi credential limits */
#define WFR_SSID_MAX_LEN 32
#define WFR_PASS_MAX_LEN 63

/* Security types */
#define WFR_SEC_OPEN 0
#define WFR_SEC_WPA  1
#define WFR_SEC_WEP  2
#define WFR_SEC_SAE  3

/* WiFi credentials struct — shared between Flipper and daemon */
typedef struct {
    char ssid[WFR_SSID_MAX_LEN + 1];
    char password[WFR_PASS_MAX_LEN + 1];
    uint8_t security;
    bool hidden;
} WfrWifiCreds;

/*
 * CRC-8/CCITT (poly 0x07, init 0x00)
 * Used for per-frame integrity check.
 */
uint8_t wfr_crc8(const uint8_t* data, size_t len);

/*
 * CRC-32 (standard ethernet polynomial)
 * Used for whole-payload integrity check in END frame.
 */
uint32_t wfr_crc32(const uint8_t* data, size_t len);

/*
 * Build a WiFi QR string from credentials.
 * Returns number of bytes written (excluding null terminator), or 0 on error.
 * Output is null-terminated.
 *
 * Example output: WIFI:T:WPA;S:MyNetwork;P:MyPass123;H:false;;
 */
size_t wfr_build_wifi_string(const WfrWifiCreds* creds, char* out, size_t out_size);

/*
 * Parse a WiFi QR string into credentials.
 * Returns true on success, false on parse error.
 * Handles backslash-escaped special characters (; : \ ")
 */
bool wfr_parse_wifi_string(const char* str, WfrWifiCreds* creds);

/*
 * Check if a timing value matches an expected value within tolerance.
 */
static inline bool wfr_timing_match(uint32_t actual, uint32_t expected) {
    uint32_t margin = expected * WFR_TOLERANCE_PCT / 100;
    return (actual >= expected - margin) && (actual <= expected + margin);
}

#ifdef __cplusplus
}
#endif
