// Native shim for ESP8266 pgmspace API used by test/libb64/cdecode.cpp.
// PROGMEM is inert on host; pgm_read_byte reads one byte.

#ifndef TEST_LIBB64_PGMSPACE_H
#define TEST_LIBB64_PGMSPACE_H

#include <stdint.h>

#define PROGMEM

static inline uint8_t pgm_read_byte_inlined(const void* addr) {
    return *reinterpret_cast<const uint8_t*>(addr);
}

#define pgm_read_byte(addr) pgm_read_byte_inlined(addr)

#endif  // TEST_LIBB64_PGMSPACE_H
