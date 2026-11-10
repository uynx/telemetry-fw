#include "frame.h"

uint16_t crc16(const uint8_t *p, size_t n) {
    uint16_t c = 0xFFFF;
    while (n--) {
        c ^= (uint16_t)(*p++ << 8);
        for (int i = 0; i < 8; i++) c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
    }
    return c;
}

size_t cobs_encode(const uint8_t *in, size_t n, uint8_t *out) {
    size_t code_at = 0, o = 1;
    uint8_t code = 1;
    for (size_t i = 0; i < n; i++) {
        if (in[i]) {
            out[o++] = in[i];
            code++;
        }
        if (!in[i] || code == 0xFF) {
            out[code_at] = code;
            code = 1;
            code_at = o++;
            if (in[i] && i + 1 == n) return o - 1;
        }
    }
    out[code_at] = code;
    return o;
}

size_t cobs_decode(const uint8_t *in, size_t n, uint8_t *out) {
    size_t i = 0, o = 0;
    while (i < n) {
        uint8_t code = in[i++];
        if (!code || i + code - 1 > n) return (size_t)-1;
        for (uint8_t k = 1; k < code; k++) out[o++] = in[i++];
        if (code < 0xFF && i < n) out[o++] = 0;
    }
    return o;
}

size_t frame_build(uint8_t type, uint16_t seq, const uint16_t *v, uint8_t count, uint8_t *wire) {
    uint8_t raw[FRAME_MAX];
    size_t n = 0;
    raw[n++] = type;
    raw[n++] = (uint8_t)seq;
    raw[n++] = (uint8_t)(seq >> 8);
    raw[n++] = count;
    for (uint8_t i = 0; i < count; i++) {
        raw[n++] = (uint8_t)v[i];
        raw[n++] = (uint8_t)(v[i] >> 8);
    }
    uint16_t c = crc16(raw, n);
    raw[n++] = (uint8_t)(c >> 8);
    raw[n++] = (uint8_t)c;
    size_t w = cobs_encode(raw, n, wire);
    wire[w++] = 0;
    return w;
}
