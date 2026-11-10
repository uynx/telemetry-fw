#pragma once
#include <stddef.h>
#include <stdint.h>

enum { FRAME_SAMPLES = 1, FRAME_STATUS = 2, FRAME_MAX_VALUES = 32 };
/* type, seq, count, values, crc, then COBS overhead and the 0x00 delimiter */
#define FRAME_MAX (4 + 2 * FRAME_MAX_VALUES + 2 + 2 + 1)

uint16_t crc16(const uint8_t *p, size_t n);
size_t cobs_encode(const uint8_t *in, size_t n, uint8_t *out);
size_t cobs_decode(const uint8_t *in, size_t n, uint8_t *out);
size_t frame_build(uint8_t type, uint16_t seq, const uint16_t *v, uint8_t count, uint8_t *wire);
