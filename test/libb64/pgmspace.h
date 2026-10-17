// Native-only shim for the ESP8266 <pgmspace.h> API used by the vendored
// libb64 decoder (test/libb64/cdecode.cpp). The device build uses the real
// pgmspace header bundled with the ESP8266 core; this shim only exists so
// the verbatim vendored decoder source compiles unchanged in native tests.
//
// Semantics match the newlib-xtensa sys/pgmspace.h used by ESP8266 core
// 3.1.2: PROGMEM marks data as normal memory on the host, and
// pgm_read_byte reads one byte and returns it as uint8_t.

#ifndef TEST_LIBB64_PGMSPACE_H
#define TEST_LIBB64_PGMSPACE_H

#include <stdint.h>

#define PROGMEM

static inline uint8_t pgm_read_byte_inlined(const void* addr) {
    return *reinterpret_cast<const uint8_t*>(addr);
}

#define pgm_read_byte(addr) pgm_read_byte_inlined(addr)

#endif  // TEST_LIBB64_PGMSPACE_H
