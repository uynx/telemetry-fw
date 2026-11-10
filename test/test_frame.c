#include <stdio.h>
#include <string.h>
#include "../src/frame.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static void crc_matches_ccitt_false_check_value(void) {
    CHECK(crc16((const uint8_t *)"123456789", 9) == 0x29B1);
}

static void cobs_matches_published_vectors(void) {
    uint8_t out[16];
    const uint8_t a[] = {0x00};
    CHECK(cobs_encode(a, 1, out) == 2 && !memcmp(out, "\x01\x01", 2));
    const uint8_t b[] = {0x11, 0x22, 0x00, 0x33};
    CHECK(cobs_encode(b, 4, out) == 5 && !memcmp(out, "\x03\x11\x22\x02\x33", 5));
    const uint8_t c[] = {0x11, 0x00, 0x00, 0x00};
    CHECK(cobs_encode(c, 4, out) == 5 && !memcmp(out, "\x02\x11\x01\x01\x01", 5));
}

static void cobs_handles_a_254_byte_run(void) {
    uint8_t in[254], enc[260], dec[260];
    for (int i = 0; i < 254; i++) in[i] = (uint8_t)(i + 1);
    size_t n = cobs_encode(in, 254, enc);
    CHECK(n == 255 && enc[0] == 0xFF && enc[254] == 0xFE);
    CHECK(memchr(enc, 0, n) == NULL);
    CHECK(cobs_decode(enc, n, dec) == 254 && !memcmp(dec, in, 254));
}

static void cobs_decode_rejects_a_code_past_the_end(void) {
    uint8_t out[8];
    CHECK(cobs_decode((const uint8_t *)"\x05\x11", 2, out) == (size_t)-1);
}

static void frame_round_trips_and_carries_crc(void) {
    uint16_t s[3] = {1, 0x0100, 0xFFFF};
    uint8_t wire[FRAME_MAX], raw[FRAME_MAX];
    size_t n = frame_build(FRAME_SAMPLES, 7, s, 3, wire);
    CHECK(wire[n - 1] == 0 && memchr(wire, 0, n - 1) == NULL);
    size_t m = cobs_decode(wire, n - 1, raw);
    /* type, seq(2), count, 3 samples(6), crc(2) */
    CHECK(m == 12 && raw[0] == FRAME_SAMPLES && raw[1] == 7 && raw[3] == 3);
    CHECK(raw[4] == 1 && raw[5] == 0 && raw[8] == 0xFF && raw[9] == 0xFF);
    CHECK(crc16(raw, m) == 0);
}

int main(void) {
    crc_matches_ccitt_false_check_value();
    cobs_matches_published_vectors();
    cobs_handles_a_254_byte_run();
    cobs_decode_rejects_a_code_past_the_end();
    frame_round_trips_and_carries_crc();
    printf("%s\n", fails ? "FAILED" : "ok 5 tests");
    return fails != 0;
}
