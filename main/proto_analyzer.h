#pragma once

#include <stdint.h>

// Protocol response analyzer.
// Correlates the UART RX stream with the kind of traffic produced by the
// Spammer macros: AT command replies, NMEA sentences and Modbus RTU frames.
// It validates checksums/CRCs so the operator can tell a well-formed answer
// from a malformed / spoofed / unchecked one.

enum ProtoEvent {
    PROTO_AT_OK = 0,
    PROTO_AT_ERROR,
    PROTO_NMEA_OK,
    PROTO_NMEA_BADCRC,
    PROTO_MODBUS_OK,
    PROTO_MODBUS_BADCRC,
    PROTO_EVENT_COUNT
};

void proto_reset();

// Feed every received byte here.
void proto_feed(uint8_t b);

// Call once whenever a burst of RX bytes has been fully drained (an inter-frame
// idle boundary). This is when a pending binary Modbus frame is evaluated.
void proto_frame_boundary();

uint32_t proto_count(int event);
const char* proto_last_desc();
int proto_last_event();
