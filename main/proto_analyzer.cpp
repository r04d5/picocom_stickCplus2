#include "proto_analyzer.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#define PROTO_LINE_MAX 128
#define PROTO_MB_MAX 256

static uint32_t counts[PROTO_EVENT_COUNT];
static int last_event = -1;
static char last_desc[48] = "";

// Text line reassembly (AT / NMEA)
static char line_buf[PROTO_LINE_MAX + 1];
static int line_len = 0;

// Modbus RTU frame accumulation (evaluated on the burst boundary)
static uint8_t mb_buf[PROTO_MB_MAX];
static int mb_len = 0;

void proto_reset() {
    for (int i = 0; i < PROTO_EVENT_COUNT; i++) {
        counts[i] = 0;
    }
    last_event = -1;
    last_desc[0] = '\0';
    line_len = 0;
    mb_len = 0;
}

static void emit(int event, const char* desc) {
    if (event >= 0 && event < PROTO_EVENT_COUNT) {
        counts[event]++;
    }
    last_event = event;
    snprintf(last_desc, sizeof(last_desc), "%s", desc);
}

// NMEA checksum: XOR of all chars between '$' and '*'.
static void analyze_nmea(const char* s, int len) {
    int star = -1;
    for (int i = 0; i < len; i++) {
        if (s[i] == '*') {
            star = i;
            break;
        }
    }
    char tag[8] = "NMEA";
    // Grab the sentence type (e.g. GPRMC) for the description when present.
    if (len > 6 && s[0] == '$') {
        snprintf(tag, sizeof(tag), "%.5s", s + 1);
    }

    if (star < 0 || star + 2 >= len) {
        // No checksum field present -> cannot be validated; treat as malformed.
        char d[48];
        snprintf(d, sizeof(d), "NMEA %s no CRC", tag);
        emit(PROTO_NMEA_BADCRC, d);
        return;
    }

    uint8_t calc = 0;
    for (int i = 1; i < star; i++) {
        calc ^= (uint8_t)s[i];
    }

    unsigned int given = 0;
    if (sscanf(s + star + 1, "%2x", &given) != 1) {
        char d[48];
        snprintf(d, sizeof(d), "NMEA %s bad CRC", tag);
        emit(PROTO_NMEA_BADCRC, d);
        return;
    }

    char d[48];
    if ((uint8_t)given == calc) {
        snprintf(d, sizeof(d), "NMEA %s CRC OK", tag);
        emit(PROTO_NMEA_OK, d);
    } else {
        snprintf(d, sizeof(d), "NMEA %s CRC FAIL", tag);
        emit(PROTO_NMEA_BADCRC, d);
    }
}

static void analyze_text_line() {
    if (line_len == 0) {
        return;
    }
    line_buf[line_len] = '\0';

    // Trim leading/trailing whitespace.
    int start = 0;
    while (start < line_len && isspace((unsigned char)line_buf[start])) start++;
    int end = line_len;
    while (end > start && isspace((unsigned char)line_buf[end - 1])) end--;
    int tlen = end - start;
    if (tlen <= 0) {
        return;
    }
    const char* s = line_buf + start;

    if (s[0] == '$') {
        analyze_nmea(s, tlen);
        return;
    }

    // AT command replies.
    if (strstr(s, "ERROR") != NULL) {
        emit(PROTO_AT_ERROR, "AT ERROR");
        return;
    }
    if (tlen == 2 && s[0] == 'O' && s[1] == 'K') {
        emit(PROTO_AT_OK, "AT OK");
        return;
    }
}

static uint16_t crc16_modbus(const uint8_t* d, int n) {
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < n; i++) {
        crc ^= d[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

static bool buffer_has_binary() {
    for (int i = 0; i < mb_len; i++) {
        uint8_t b = mb_buf[i];
        if (b != '\r' && b != '\n' && b != '\t' && (b < 32 || b > 126)) {
            return true;
        }
    }
    return false;
}

void proto_feed(uint8_t b) {
    // Text path (AT / NMEA): split on CR/LF.
    if (b == '\n' || b == '\r') {
        analyze_text_line();
        line_len = 0;
    } else if (b >= 32 && b <= 126) {
        if (line_len >= PROTO_LINE_MAX) {
            analyze_text_line();
            line_len = 0;
        }
        line_buf[line_len++] = (char)b;
    } else {
        // Non-printable byte terminates any pending text line.
        analyze_text_line();
        line_len = 0;
    }

    // Binary path (Modbus): accumulate the whole burst.
    if (mb_len < PROTO_MB_MAX) {
        mb_buf[mb_len++] = b;
    }
}

void proto_frame_boundary() {
    if (mb_len >= 4 && mb_len <= PROTO_MB_MAX) {
        uint16_t calc = crc16_modbus(mb_buf, mb_len - 2);
        uint16_t given = (uint16_t)mb_buf[mb_len - 2] | ((uint16_t)mb_buf[mb_len - 1] << 8);
        uint8_t addr = mb_buf[0];
        uint8_t func = mb_buf[1] & 0x7F;
        bool plausible = (addr >= 1 && addr <= 247) &&
                         (func == 0x01 || func == 0x02 || func == 0x03 ||
                          func == 0x04 || func == 0x05 || func == 0x06 ||
                          func == 0x0F || func == 0x10);
        if (calc == given) {
            char d[48];
            snprintf(d, sizeof(d), "Modbus a%u f%u CRC OK (%dB)", addr, mb_buf[1], mb_len);
            emit(PROTO_MODBUS_OK, d);
        } else if (plausible && buffer_has_binary()) {
            // Looks like a Modbus frame but the CRC did not verify: a firmware
            // that acts on this anyway is failing to validate integrity.
            char d[48];
            snprintf(d, sizeof(d), "Modbus a%u f%u CRC FAIL", addr, mb_buf[1]);
            emit(PROTO_MODBUS_BADCRC, d);
        }
    }
    mb_len = 0;
}

uint32_t proto_count(int event) {
    if (event < 0 || event >= PROTO_EVENT_COUNT) {
        return 0;
    }
    return counts[event];
}

const char* proto_last_desc() {
    return last_desc;
}

int proto_last_event() {
    return last_event;
}
