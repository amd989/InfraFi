#pragma once

#include "protocol/wfr_protocol.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Progress callback, called after each payload byte is sent.
 * sent/total count payload bytes across all retransmission passes.
 * Return false to cancel the transmission.
 */
typedef bool (*WfrTxProgressCallback)(uint16_t sent, uint16_t total, void* context);

/*
 * Transmit WiFi credentials over IR.
 *
 * Builds the WiFi QR string, sends it as a sequence of IR messages
 * (START + DATA bytes + END), repeated WFR_RETRANSMIT_COUNT times.
 * Uses RC-6 or NEC encoding based on the protocol parameter.
 * Blocks until done, so call it from a worker thread when the UI must animate.
 * progress may be NULL.
 *
 * Returns true if transmission completed, false on error or cancel.
 */
bool wfr_transmit_credentials(
    const WfrWifiCreds* creds,
    WfrIrProtocol protocol,
    WfrTxProgressCallback progress,
    void* context);

#ifdef __cplusplus
}
#endif
